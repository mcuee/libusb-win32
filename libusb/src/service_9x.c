/* LIBUSB-WIN32, Generic Windows USB Driver
 * Copyright (C) 2002-2004 Stephan Meyer, <ste_meyer@web.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define WINVER 0x0500
#define INITGUID

#include <windows.h>
#include <dbt.h>
#include <initguid.h>

#include "service.h"
#include "win_debug.h"

DEFINE_GUID(GUID_DEVINTERFACE_USB_HUB, 0xf18a0e88, 0xc30c, 0x11d0, 0x88, \
            0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8);

DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, \
            0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

#define LIBUSB_SERVICE_WINDOW_CLASS "LIBUSB_SERVICE_WINDOW_CLASS"


static HDEVNOTIFY notification_handle_hub, notification_handle_dev;
static HWND win_handle;
typedef DWORD WINAPI (*register_service_process_t)(DWORD, DWORD);

static void usb_register_notifications(void);
static void usb_unregister_notifications(void);

LRESULT CALLBACK window_proc(HWND handle, UINT message, WPARAM w_param, 
                             LPARAM l_param);
LRESULT CALLBACK window_proc(HWND handle, UINT message, WPARAM w_param, 
                             LPARAM l_param)
{
  switch(message)
    {
    case WM_DESTROY:
      if(notification_handle_hub)
        UnregisterDeviceNotification(notification_handle_hub);
      if(notification_handle_dev)
        UnregisterDeviceNotification(notification_handle_dev);

      PostQuitMessage(0);
      break;
    case WM_DEVICECHANGE:      
      if(w_param == DBT_DEVICEARRIVAL)
        {
          usb_unregister_notifications();
          usb_registry_start_filter(TRUE);
          usb_register_notifications();
        }
      break;
    case WM_USER + LIBUSB_SERVICE_CONTROL_PAUSE:
      usb_unregister_notifications();
      break;
    case WM_USER + LIBUSB_SERVICE_CONTROL_CONTINUE:
      usb_register_notifications();
      break;
    default:
      return DefWindowProc(handle, message, w_param, l_param);
    } 
  return TRUE;
}


int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, 
                   LPSTR cmd_line, int show_cmd)
{
  HMODULE kernel32_dll;
  register_service_process_t register_service_process;
  WNDCLASS win_class = {0, window_proc, 0, 0, instance, 
                        NULL, NULL, NULL, NULL, 
                        LIBUSB_SERVICE_WINDOW_CLASS};
  MSG win_message;

  kernel32_dll = GetModuleHandle("kernel32.dll");
  
  if(!kernel32_dll)
    {
      return 1;
    }

  register_service_process = (register_service_process_t) 
    GetProcAddress(kernel32_dll, "RegisterServiceProcess");

  if(!register_service_process)
    {
      return 1;
    }

  RegisterClass(&win_class);
  win_handle = CreateWindowEx(WS_EX_APPWINDOW, LIBUSB_SERVICE_WINDOW_CLASS, 
                              NULL, WS_OVERLAPPEDWINDOW, -1, -1, -1, -1,
                              NULL, NULL, instance, NULL);

  CreateMutex(NULL, TRUE, "libusb_service_mutex");
  usb_register_notifications();
  register_service_process(GetCurrentProcessId(), 1);

  while(GetMessage(&win_message, NULL, 0, 0))
    {
      TranslateMessage(&win_message);
      DispatchMessage(&win_message);
    }
  
  DestroyWindow(win_handle);
  UnregisterClass(LIBUSB_SERVICE_WINDOW_CLASS, instance);
  register_service_process(GetCurrentProcessId(), 0);

  return 0;
}

static void usb_register_notifications(void)
{
  DEV_BROADCAST_DEVICEINTERFACE dev_if;
  dev_if.dbcc_size = sizeof(dev_if);
  dev_if.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_HUB;

  notification_handle_hub 
    = RegisterDeviceNotification(win_handle, &dev_if, 
                                 DEVICE_NOTIFY_SERVICE_HANDLE);

  dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

  notification_handle_dev 
    = RegisterDeviceNotification(win_handle, &dev_if, 
                                 DEVICE_NOTIFY_SERVICE_HANDLE);
}

static void usb_unregister_notifications(void)
{
  if(notification_handle_hub)
    UnregisterDeviceNotification(notification_handle_hub);
  if(notification_handle_dev)
    UnregisterDeviceNotification(notification_handle_dev);
}
