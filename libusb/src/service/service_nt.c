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
#include <windows.h>
#include <dbt.h>
#include <initguid.h>

#include "service.h"
#include "win_debug.h"


#define LIBUSB_SERVICE_NAME "libusbd"

DEFINE_GUID(GUID_DEVINTERFACE_USB_HUB, 0xf18a0e88, 0xc30c, 0x11d0, 0x88, \
	    0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8);

DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, \
	    0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

static SERVICE_STATUS service_status;       
static SERVICE_STATUS_HANDLE service_status_handle;
static HDEVNOTIFY notification_handle_hub, notification_handle_dev;

static HANDLE service_stop_event;


static void WINAPI usb_service_main(int argc, char **argv);
static DWORD WINAPI usb_service_handler(DWORD code, DWORD event_type, 
					VOID *event_data, VOID *context);

static void usb_service_run(int argc, char **argv);
static void usb_service_set_status(DWORD status, DWORD exit_code);
static void usb_register_notifications(void);
static void usb_unregister_notifications(void);

int main(int argc, char **argv)
{
  SERVICE_TABLE_ENTRY dispatch_table[] =
    {
      { LIBUSB_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)usb_service_main },
      { NULL, NULL }
    };

  StartServiceCtrlDispatcher(dispatch_table);
  return 0;
}


static void WINAPI usb_service_main(int argc, char **argv)
{
  memset(&service_status, 0, sizeof(service_status));

  service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status.dwCurrentState = SERVICE_START_PENDING; 
  service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP 
    | SERVICE_ACCEPT_PAUSE_CONTINUE;

  service_status.dwWaitHint = 5000;

  service_status_handle = RegisterServiceCtrlHandlerEx(LIBUSB_SERVICE_NAME, 
						       usb_service_handler,
						       NULL);
  
  if(!service_status_handle)
    return;

  usb_register_notifications();
  usb_service_run(argc, argv);

  return;
}


static DWORD WINAPI usb_service_handler(DWORD code, DWORD event_type, 
					VOID *event_data, VOID *context)
{
  switch(code)
    { 
    case SERVICE_CONTROL_STOP: 
      usb_service_set_status(SERVICE_STOP_PENDING, NO_ERROR); 
      usb_unregister_notifications();
      
      if(service_stop_event)
	{ 
	  SetEvent(service_stop_event);
	}
      break;
    case SERVICE_CONTROL_DEVICEEVENT:
      if(event_type == DBT_DEVICEARRIVAL)
	{
	  usb_unregister_notifications();
	  usb_registry_start_filter();
	  usb_register_notifications();
	}
      usb_service_set_status(SERVICE_RUNNING, NO_ERROR);
      break;
    case SERVICE_CONTROL_PAUSE:
      usb_service_set_status(SERVICE_PAUSE_PENDING, NO_ERROR);
      usb_unregister_notifications();
      usb_service_set_status(SERVICE_PAUSED, NO_ERROR);
      break;
    case SERVICE_CONTROL_CONTINUE:
      usb_service_set_status(SERVICE_CONTINUE_PENDING, NO_ERROR);
      usb_register_notifications();
      usb_service_set_status(SERVICE_RUNNING, NO_ERROR);
      break;
    default: 
      ;
    } 

  return NO_ERROR; 
}

static void usb_service_run(int argc, char **argv)
{
  usb_service_set_status(SERVICE_START_PENDING, NO_ERROR);
  service_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
  
  if(!service_stop_event)
    {
      usb_service_set_status(SERVICE_STOPPED, NO_ERROR);
      return;
    }
  
  usb_service_set_status(SERVICE_RUNNING, NO_ERROR);

  while(WaitForSingleObject(service_stop_event, INFINITE));

  CloseHandle(service_stop_event);
  usb_service_set_status(SERVICE_STOPPED, NO_ERROR);
}

static void usb_service_set_status(DWORD status, DWORD exit_code)
{
  service_status.dwCurrentState  = status;
  service_status.dwWin32ExitCode = exit_code;

  SetServiceStatus(service_status_handle, &service_status); 
}

static void usb_register_notifications(void)
{
  DEV_BROADCAST_DEVICEINTERFACE dev_if;
  dev_if.dbcc_size = sizeof(dev_if);
  dev_if.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_HUB;

  notification_handle_hub 
    = RegisterDeviceNotification((HANDLE)service_status_handle, &dev_if, 
				 DEVICE_NOTIFY_SERVICE_HANDLE);

  dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

  notification_handle_dev 
    = RegisterDeviceNotification((HANDLE)service_status_handle, &dev_if, 
				 DEVICE_NOTIFY_SERVICE_HANDLE);
}

static void usb_unregister_notifications(void)
{
  if(notification_handle_hub)
    UnregisterDeviceNotification(notification_handle_hub);
  if(notification_handle_dev)
    UnregisterDeviceNotification(notification_handle_dev);
}
