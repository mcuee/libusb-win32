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

void CALLBACK usb_create_kernel_service_rundll(HWND wnd, HINSTANCE instance,
                                               LPSTR cmd_line, int cmd_show);
void CALLBACK usb_remove_kernel_service_rundll(HWND wnd, HINSTANCE instance,
                                               LPSTR cmd_line, int cmd_show);

void CALLBACK usb_install_driver_rundll(HWND wnd, HINSTANCE instance,
                                        LPSTR cmd_line, int cmd_show);


void CALLBACK usb_create_kernel_service_rundll(HWND wnd, HINSTANCE instance,
                                               LPSTR cmd_line, int cmd_show)
{
  char display_name[MAX_PATH];
  
  memset(display_name, 0, sizeof(display_name));

  /* stop all libusb devices */
  usb_registry_stop_libusb_devices();

  /* the old driver is unloaded now */ 

  /* create the Display Name */
  snprintf(display_name, sizeof(display_name) - 1,
           "LibUsb-Win32 - Kernel Driver, Version %d.%d.%d.%d", 
           VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);

  /* create the kernel service */
  usb_create_service(LIBUSB_DRIVER_NAME, display_name, LIBUSB_DRIVER_PATH,
                     SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START);
     
  /* restart libusb devices */
  usb_registry_start_libusb_devices(); 
  /* insert filter drivers */
  usb_registry_start_filter();

  /* register libusb's DLL as a device co-installer */

  /* USB class */
  usb_registry_insert_co_installer("{36FC9E60-C465-11CF-8056-444553540000}");
  /* Net class */
  usb_registry_insert_co_installer("{4D36E972-E325-11CE-BFC1-08002BE10318}");
  /* Image class */
  usb_registry_insert_co_installer("{6BDD1FC6-810F-11D0-BEC7-08002BE2092F}");
  /* HID class */
  usb_registry_insert_co_installer("{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}");
  /* Media class */
  usb_registry_insert_co_installer("{4D36E96C-E325-11CE-BFC1-08002BE10318}");
  /* Modem class */
  usb_registry_insert_co_installer("{4D36E96D-E325-11CE-BFC1-08002BE10318}");
  /* SmartCardReader class */
  usb_registry_insert_co_installer("{50DD5230-BA8A-11D1-BF5D-0000F805F530}");
}

void CALLBACK usb_remove_kernel_service_rundll(HWND wnd, HINSTANCE instance,
                                               LPSTR cmd_line, int cmd_show)
{
  /* unregister the device co-installer */

  /* USB class */
  usb_registry_remove_co_installer("{36FC9E60-C465-11CF-8056-444553540000}");
  /* Net class */
  usb_registry_remove_co_installer("{4D36E972-E325-11CE-BFC1-08002BE10318}");
  /* Image class */
  usb_registry_remove_co_installer("{6BDD1FC6-810F-11D0-BEC7-08002BE2092F}");
  /* HID class */
  usb_registry_remove_co_installer("{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}");
  /* Media class */
  usb_registry_remove_co_installer("{4D36E96C-E325-11CE-BFC1-08002BE10318}");
  /* Modem class */
  usb_registry_remove_co_installer("{4D36E96D-E325-11CE-BFC1-08002BE10318}");
  /* SmartCardReader class */
  usb_registry_remove_co_installer("{50DD5230-BA8A-11D1-BF5D-0000F805F530}");

  /* remove filter drivers */
  usb_registry_stop_filter(); 
}

void CALLBACK usb_install_driver_rundll(HWND wnd, HINSTANCE instance,
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


  /* retrieve the full .inf file path */
  if(!GetFullPathName(cmd_line, MAX_PATH, inf_file, NULL))
    {
      usb_debug_error("usb_install_driver(): .inf file %s not found\n", 
                      cmd_line);
      return;
    }

  /* open the .inf file */
  inf_handle = SetupOpenInfFile(inf_file, NULL, INF_STYLE_WIN4, NULL);

  if(inf_handle == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_install_driver(): unable to open .inf file %s\n", 
                      cmd_line);
      return;
    }

  /* find the .inf file's device description section marked "libusb-win32" */
  if(!SetupFindFirstLine(inf_handle, "libusb-win32", NULL, &inf_context))
    {
      usb_debug_error("usb_install_driver(): .inf file %s does not contain "
                      "any device descriptions\n", cmd_line);
      SetupCloseInfFile(inf_handle);
      return;
    }


  do {
    /* get the device ID from the .inf file */
    if(!SetupGetStringField(&inf_context, 2, id, sizeof(id), NULL))
      {
        continue;
      }

    /* convert the string to lowercase */
    strlwr(id);

    reboot = FALSE;

    /* force to update all connected devices matching this ID */
    /* UpdateDriverForPlugAndPlayDevices(NULL, id, inf_file,  */
    /*      					  INSTALLFLAG_FORCE , &reboot); */


    /* update all connected devices matching this ID, but only if this */
    /* driver is better or newer */
    UpdateDriverForPlugAndPlayDevices(NULL, id, inf_file, 0, &reboot);
    

    /* copy the .inf file to the system directory so that is will be found */
    /* when new  devices are plugged in */
    SetupCopyOEMInf(inf_file, NULL, SPOST_PATH, 0, NULL, 0, NULL, NULL);


    /* now search the registry for device nodes representing currently  */
    /* unattached devices */


    /* get all USB device nodes from the registry, present and non-present */
    /* devices */
    dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES);
    
    if(dev_info == INVALID_HANDLE_VALUE)
      {
        SetupCloseInfFile(inf_handle);
        return;
      }
 
    dev_index = 0;

    /* enumerate the device list to find all attached and unattached */
    /* devices */
    while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
      {
        /* get the harware ID from the registry, this is a multi-zero string */
        if(SetupDiGetDeviceRegistryProperty(dev_info, &dev_info_data,
                                            SPDRP_HARDWAREID, NULL,  
                                            (BYTE *)tmp_id, 
                                            sizeof(tmp_id), NULL))
          {
            /* check all possible IDs contained in that multi-zero string */
            for(p = tmp_id; *p; p += (strlen(p) + 1))
              {
                /* convert the string to lowercase */
                strlwr(p);
		
                /* found a match? */
                if(strstr(p, id))
                  {
                    /* is this device disconnected? */
                    if(CM_Get_DevNode_Status(&status,
                                             &problem,
                                             dev_info_data.DevInst,
                                             0) == CR_NO_SUCH_DEVINST)
                      {
                        /* found a device node that represents an unattached */
                        /* device */
                        if(SetupDiGetDeviceRegistryProperty(dev_info, 
                                                            &dev_info_data,
                                                            SPDRP_CONFIGFLAGS, 
                                                            NULL,  
                                                            (BYTE *)&config_flags, 
                                                            sizeof(config_flags),
                                                            NULL))
                          {
                            /* mark the device to be reinstalled the next time it is */
                            /* plugged in */
                            config_flags |= CONFIGFLAG_REINSTALL;
			    
                            /* write the property back to the registry */
                            SetupDiSetDeviceRegistryProperty(dev_info, 
                                                             &dev_info_data,
                                                             SPDRP_CONFIGFLAGS,
                                                             (BYTE *)&config_flags, 
                                                             sizeof(config_flags));
                          }
                      }
                    /* a match was found, skip the rest */
                    break;
                  }
              }
          }
        /* check the next device node */
        dev_index++;
      }
    
    SetupDiDestroyDeviceInfoList(dev_info);

    /* get the next device ID from the .inf file */ 
  } while(SetupFindNextLine(&inf_context, &inf_context));

  /* we are done, close the .inf file */
  SetupCloseInfFile(inf_handle);

  usb_registry_stop_libusb_devices(); /* stop all libusb devices */
  usb_registry_start_libusb_devices(); /* restart all libusb devices */

  return;
}

HRESULT WINAPI usb_coinstaller(DI_FUNCTION install_function,
                               HDEVINFO dev_info,
                               SP_DEVINFO_DATA *dev_info_data,
                               COINSTALLER_CONTEXT_DATA *context)
{
  switch(install_function)
    {
      /* case DIF_PROPERTYCHANGE: */
    case DIF_INSTALLDEVICE: 

      /* connect the kernel driver to this device */
      usb_registry_insert_filter(dev_info, dev_info_data,
                                 LIBUSB_DRIVER_NAME);
      break;        
    
    default:
      ;
    }
    
  return NO_ERROR;    
}

