/* LIBUSB-WIN32, Generic Windows USB Driver
 * Copyright (C) 2002-2003 Stephan Meyer, <ste_meyer@web.de>
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


#include <windows.h>
#include "service.h"


#define LIBUSB_WINDOW_CLASS "LIBUSB_WINDOW_CLASS"
#define LIBUSB_TIMER_ID 100

typedef DWORD WINAPI (*register_service_process_t)(DWORD, DWORD);

LRESULT CALLBACK window_proc(HWND handle, UINT message, WPARAM w_param, 
			     LPARAM l_param);

LRESULT CALLBACK window_proc(HWND handle, UINT message, WPARAM w_param, 
			     LPARAM l_param)
{
  switch(message)
    {
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_ENDSESSION:
      if(!l_param) /* system shutdown */
	{
	  PostQuitMessage(0);
	}
      break;
    case WM_TIMER:
      usb_service_start_filter();
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
			LIBUSB_WINDOW_CLASS};
  HWND win_handle;
  MSG win_message;
  UINT win_timer;

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
  win_handle = CreateWindowEx(WS_EX_APPWINDOW, LIBUSB_WINDOW_CLASS, 
			      NULL, WS_OVERLAPPEDWINDOW, -1, -1, -1, -1,
			      NULL, NULL, instance, NULL);

  register_service_process(GetCurrentProcessId(), 1);

  usb_service_start_filter();

  win_timer = SetTimer(win_handle, LIBUSB_TIMER_ID, 2000, NULL);

  while(GetMessage(&win_message, NULL, 0, 0))
    {
      TranslateMessage(&win_message);
      DispatchMessage(&win_message);
    }
  
  KillTimer(win_handle, LIBUSB_TIMER_ID);
  DestroyWindow(win_handle);
  UnregisterClass(LIBUSB_WINDOW_CLASS, instance);
  register_service_process(GetCurrentProcessId(), 0);

  return 0;
}

