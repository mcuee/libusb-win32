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


#include <windows.h>
#include <setupapi.h>
#include <ddk/newdev.h>
#include <stdio.h>
#include <ddk/cfgmgr32.h>
#include <regstr.h>

#include "service.h"
#include "registry.h"
#include "win_debug.h"



#define LIBUSB_DRIVER_PATH "system32\\drivers\\libusb0.sys"

void usb_delete_services(void);
void usb_create_services(void);
void CALLBACK usb_install_and_update_devices(HWND wnd, HINSTANCE instance,
					     LPSTR cmd_line, int cmd_show);


void usb_create_services(void)
{
  char display_name[REGISTRY_BUF_SIZE];
  
  usb_registry_stop_filter(); /* remove all filter drivers */
  usb_registry_stop_libusb_devices(); /* stop all libusb devices */

  /* the old driver is unloaded now */ 

  if(usb_registry_is_nt())
    {
      snprintf(display_name, sizeof(display_name) - 1,
	       "LibUsb-Win32 - Kernel Driver, Version %d.%d.%d.%d", 
	       VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);

      /* create the kernel service */
      usb_create_service(LIBUSB_DRIVER_NAME_NT, display_name,
			 LIBUSB_DRIVER_PATH,
			 SERVICE_KERNEL_DRIVER,
			 SERVICE_DEMAND_START);
    }
     
  usb_registry_start_libusb_devices(); /* restart libusb devices */
  usb_registry_start_filter();

  if(usb_registry_is_nt())
    {
      snprintf(display_name, sizeof(display_name) - 1,
	       "LibUsb-Win32 - Daemon, Version %d.%d.%d.%d", 
	       VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);
      
      /* create the system service */
      usb_create_service(LIBUSB_SERVICE_NAME, display_name,
			 LIBUSB_SERVICE_PATH, 
			 SERVICE_WIN32_OWN_PROCESS,
			 SERVICE_AUTO_START); 

      usb_start_service(LIBUSB_SERVICE_NAME);
    }
}

void usb_delete_services(void)
{
  HANDLE win; 

  if(usb_registry_is_nt())
    {
      usb_stop_service(LIBUSB_SERVICE_NAME); 
      usb_delete_service(LIBUSB_SERVICE_NAME);
    }
  else
    {
      do {
	win = FindWindow("LIBUSB_WINDOW_CLASS", NULL);
	if(win != INVALID_HANDLE_VALUE)
	  {
	    PostMessage(win, WM_DESTROY, 0, 0);
	    Sleep(500);
	  }
      } while(win);
    } 

  usb_registry_stop_filter(); /* remove filter drivers */
}

void CALLBACK usb_install_and_update_devices(HWND wnd, HINSTANCE instance,
					     LPSTR cmd_line, int cmd_show)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  INFCONTEXT inf_context;
  HINF inf_handle;
  DWORD config_flags, problem, status;
  BOOL reboot;
  char inf_file[MAX_PATH];
  char id[MAX_PATH];
  char tmp_id[MAX_PATH];
  char *p;
  int dev_index;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  GetFullPathName(cmd_line, MAX_PATH, inf_file, NULL);

  inf_handle = SetupOpenInfFile(inf_file, NULL, INF_STYLE_WIN4, NULL);

  SetupFindFirstLine(inf_handle, "Devices", NULL, &inf_context);

  do {
    SetupGetStringField(&inf_context, 2, id, sizeof(id), NULL);
    strlwr(id);

    /* update all connected devices */
    UpdateDriverForPlugAndPlayDevices(NULL, id, inf_file, 0, &reboot);
    
    /* copy the .inf file */
    SetupCopyOEMInf(inf_file, NULL, SPOST_PATH, 0, NULL, 0, NULL, NULL);

    dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES);
    
    if(dev_info == INVALID_HANDLE_VALUE)
      {
	SetupCloseInfFile(inf_handle);
	return;
      }
 
    dev_index = 0;

    while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
      {
	if(SetupDiGetDeviceRegistryProperty(dev_info, &dev_info_data,
					    SPDRP_HARDWAREID, NULL,  
					    (BYTE *)tmp_id, 
					    sizeof(tmp_id), NULL))
	  {
	    for(p = tmp_id; *p; p += (strlen(p) + 1))
	      {
		strlwr(p);
		
		if(strstr(p, id))
		  {
		    if(CM_Get_DevNode_Status(&status,
					     &problem,
					     dev_info_data.DevInst,
					     0) == CR_NO_SUCH_DEVINST)
		      {
			/* found an unattached device */
			if(SetupDiGetDeviceRegistryProperty(dev_info, 
							    &dev_info_data,
							    SPDRP_CONFIGFLAGS, 
							    NULL,  
							    (BYTE *)&config_flags, 
							    sizeof(config_flags),
							    NULL))
			  {
			    /* mark the device to be reinstalled */
			    config_flags |= CONFIGFLAG_REINSTALL;
			    
			    SetupDiSetDeviceRegistryProperty(dev_info, 
							     &dev_info_data,
							     SPDRP_CONFIGFLAGS,
							     (BYTE *)&config_flags, 
							     sizeof(config_flags));
			  }
		      }
		    break;
		  }
	      }
	  }
	dev_index++;
      }
    
    SetupDiDestroyDeviceInfoList(dev_info);

  } while(SetupFindNextLine(&inf_context, &inf_context));

  SetupCloseInfFile(inf_handle);

  usb_registry_stop_libusb_devices(); /* stop all libusb devices */
  usb_registry_start_libusb_devices(); /* restart all libusb devices */
}
