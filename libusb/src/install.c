/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <windows.h>
#include <setupapi.h>
#include <stdio.h>

#ifdef __GNUC__
#include <ddk/cfgmgr32.h>
#else
#include <cfgmgr32.h>
#endif

#include <regstr.h>
#include <wchar.h>

#include "usb.h"
#include "registry.h"
#include "win_debug.h"
#include "driver/driver_api.h"


#define LIBUSB_DRIVER_PATH  "system32\\drivers\\libusb0.sys"

#define INSTALLFLAG_FORCE 0x00000001

typedef BOOL (WINAPI * update_driver_for_plug_and_play_devices_t)(HWND, 
                                                                  LPCSTR, 
                                                                  LPCSTR, 
                                                                  DWORD,
                                                                  PBOOL);

typedef SC_HANDLE (WINAPI * open_sc_manager_t)(LPCTSTR, LPCTSTR, DWORD);
typedef SC_HANDLE (WINAPI * open_service_t)(SC_HANDLE, LPCTSTR, DWORD);
typedef BOOL (WINAPI * change_service_config_t)(SC_HANDLE, DWORD, DWORD, 
                                                DWORD, LPCTSTR, LPCTSTR, 
                                                LPDWORD, LPCTSTR, LPCTSTR, 
                                                LPCTSTR, LPCTSTR);
typedef BOOL (WINAPI * close_service_handle_t)(SC_HANDLE);
typedef SC_HANDLE (WINAPI * create_service_t)(SC_HANDLE, LPCTSTR, LPCTSTR,
                                              DWORD, DWORD,DWORD, DWORD,
                                              LPCTSTR, LPCTSTR, LPDWORD,
                                              LPCTSTR, LPCTSTR, LPCTSTR);


static bool_t usb_create_service(const char *name, const char *display_name,
                                 const char *binary_path, unsigned long type,
                                 unsigned long start_type);

void CALLBACK usb_touch_inf_file_rundll(HWND wnd, HINSTANCE instance,
                                 LPSTR cmd_line, int cmd_show);

void CALLBACK usb_install_service_np_rundll(HWND wnd, HINSTANCE instance,
                                            LPSTR cmd_line, int cmd_show)
{
  usb_install_service_np();
}

void CALLBACK usb_uninstall_service_np_rundll(HWND wnd, HINSTANCE instance,
                                              LPSTR cmd_line, int cmd_show)
{
  usb_uninstall_service_np();
}

void CALLBACK usb_install_driver_np_rundll(HWND wnd, HINSTANCE instance,
                                           LPSTR cmd_line, int cmd_show)
{
  usb_install_driver_np(cmd_line);
}


int usb_install_service_np(void)
{
  char display_name[MAX_PATH];
  int ret = 0;


  memset(display_name, 0, sizeof(display_name));


  /* remove filter drivers */
  usb_registry_remove_filter();
  usb_registry_restart_root_hubs(); 

  /* stop all libusb devices */
  usb_registry_stop_libusb_devices();

  /* the old driver is unloaded now */ 

  if(usb_registry_is_nt())
    {
      /* create the Display Name */
      _snprintf(display_name, sizeof(display_name) - 1,
               "LibUsb-Win32 - Kernel Driver, Version %d.%d.%d.%d", 
                LIBUSB_VERSION_MAJOR, LIBUSB_VERSION_MINOR, 
                LIBUSB_VERSION_MICRO, LIBUSB_VERSION_NANO);
      
      /* create the kernel service */
      if(!usb_create_service(LIBUSB_DRIVER_NAME_NT, display_name, 
                             LIBUSB_DRIVER_PATH,
                             SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START))
        ret = -1;
    }
  
  /* restart libusb devices */
  usb_registry_start_libusb_devices(); 
  /* insert filter drivers */
  usb_registry_insert_filter();
  usb_registry_restart_root_hubs(); 

  return ret;
}

int usb_uninstall_service_np(void)
{
  /* remove filter drivers */
  usb_registry_remove_filter();
  usb_registry_restart_root_hubs(); 

  return 1;
}

int usb_install_driver_np(const char *inf_file)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  INFCONTEXT inf_context;
  HINF inf_handle;
  DWORD config_flags, problem, status;
  BOOL reboot;
  char inf_path[MAX_PATH];
  char id[MAX_PATH];
  char tmp_id[MAX_PATH];
  char *p;
  int dev_index;
  HANDLE newdev_dll = NULL;
  
  update_driver_for_plug_and_play_devices_t UpdateDriverForPlugAndPlayDevices;

  newdev_dll = LoadLibrary("newdev.dll");

  if(!newdev_dll)
    {
      usb_debug_error("usb_install_driver(): loading newdev.dll failed\n");
      return -1;
    }
  
  UpdateDriverForPlugAndPlayDevices =  
    (update_driver_for_plug_and_play_devices_t) 
    GetProcAddress(newdev_dll, "UpdateDriverForPlugAndPlayDevicesA");

  if(!UpdateDriverForPlugAndPlayDevices)
    {
      usb_debug_error("usb_install_driver(): loading newdev.dll failed\n");
      return -1;
    }


  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);


  /* retrieve the full .inf file path */
  if(!GetFullPathName(inf_file, MAX_PATH, inf_path, NULL))
    {
      usb_debug_error("usb_install_driver(): .inf file %s not found\n", 
                      inf_file);
      return -1;
    }

  /* open the .inf file */
  inf_handle = SetupOpenInfFile(inf_path, NULL, INF_STYLE_WIN4, NULL);

  if(inf_handle == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_install_driver(): unable to open .inf file %s\n", 
                      inf_file);
      return -1;
    }

  /* find the .inf file's device description section marked "Devices" */
  if(!SetupFindFirstLine(inf_handle, "Devices", NULL, &inf_context))
    {
      usb_debug_error("usb_install_driver(): .inf file %s does not contain "
                      "any device descriptions\n", inf_file);
      SetupCloseInfFile(inf_handle);
      return -1;
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

    /* copy the .inf file to the system directory so that is will be found */
    /* when new devices are plugged in */
    SetupCopyOEMInf(inf_path, NULL, SPOST_PATH, 0, NULL, 0, NULL, NULL);

    /* update all connected devices matching this ID, but only if this */
    /* driver is better or newer */
    UpdateDriverForPlugAndPlayDevices(NULL, id, inf_path, INSTALLFLAG_FORCE, 
                                      &reboot);
    

    /* now search the registry for device nodes representing currently  */
    /* unattached devices */


    /* get all USB device nodes from the registry, present and non-present */
    /* devices */
    dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES);
    
    if(dev_info == INVALID_HANDLE_VALUE)
      {
        SetupCloseInfFile(inf_handle);
        break;
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

  return 0;
}

bool_t usb_create_service(const char *name, const char *display_name,
                          const char *binary_path, unsigned long type,
                          unsigned long start_type)
{
  HANDLE advapi32_dll = NULL;
  open_sc_manager_t open_sc_manager = NULL;
  open_service_t open_service = NULL;
  change_service_config_t change_service_config = NULL;
  close_service_handle_t close_service_handle = NULL;
  create_service_t create_service = NULL;

  SC_HANDLE scm = NULL;
  SC_HANDLE service = NULL;
  bool_t ret = FALSE;

  advapi32_dll = LoadLibrary("advapi32.dll");
  
  if(!advapi32_dll)
    {
      usb_debug_error("usb_create_service(): loading advapi32.dll failed");
      return FALSE;
    }
  
  open_sc_manager = (open_sc_manager_t)
    GetProcAddress(advapi32_dll, "OpenSCManagerA");
  
  open_service = (open_service_t)
    GetProcAddress(advapi32_dll, "OpenServiceA");
  
  change_service_config = (change_service_config_t)
    GetProcAddress(advapi32_dll, "ChangeServiceConfigA");
  
  close_service_handle = (close_service_handle_t)
    GetProcAddress(advapi32_dll, "CloseServiceHandle");
  
  create_service = (create_service_t)
    GetProcAddress(advapi32_dll, "CreateServiceA");
  
  
  if(!open_sc_manager || !open_service || !change_service_config
     || !close_service_handle || !create_service)
    {
      FreeLibrary(advapi32_dll);
      usb_debug_error("usb_create_service(): loading advapi32.dll "
                      "functions failed");
      
      return FALSE;
    }

  do 
    {
      scm = open_sc_manager(NULL, SERVICES_ACTIVE_DATABASE, 
                            SC_MANAGER_ALL_ACCESS);

      if(!scm)
        {
          usb_debug_error("usb_service_create(): opening service control "
                          "manager failed: %s", win_error_to_string());
          break;
        }
      
      service = open_service(scm, name, SERVICE_ALL_ACCESS);

      if(service)
        {
          if(!change_service_config(service,
                                    type,
                                    start_type,
                                    SERVICE_ERROR_NORMAL,
                                    binary_path,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL,
                                    display_name))
            {
              usb_debug_error("usb_service_create(): changing config of "
                              "service '%s' failed: %s", 
                              name, win_error_to_string());
              break;
            }
          ret = TRUE;
          break;
        }
  
      if(GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
        {
          service = create_service(scm,
                                   name,
                                   display_name,
                                   GENERIC_EXECUTE,
                                   type,
                                   start_type,
                                   SERVICE_ERROR_NORMAL, 
                                   binary_path,
                                   NULL, NULL, NULL, NULL, NULL);
	  
          if(!service)
            {
              usb_debug_error("usb_service_create(): creating "
                              "service '%s' failed: %s",
                              name, win_error_to_string());
            }
          ret = TRUE;	
        }
    } while(0);

  if(service)
    {
      close_service_handle(service);
    }
  
  if(scm)
    {
      close_service_handle(scm);
    }
  
  return ret;
}


void CALLBACK usb_touch_inf_file_np_rundll(HWND wnd, HINSTANCE instance,
                                           LPSTR cmd_line, int cmd_show)
{
  usb_touch_inf_file_np(cmd_line);
}

int usb_touch_inf_file_np(const char *inf_file)
{
  const char inf_comment[] = ";added by libusb to break this file's digital "
    "signature";
  const wchar_t inf_comment_uni[] = L";added by libusb to break this file's "
    L"digital signature";

  char buf[1024];
  wchar_t wbuf[1024];
  int found = 0;
  OSVERSIONINFO version;
  FILE *f;

  version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

  if(!GetVersionEx(&version))
     return -1;


  /* XP system */
  if((version.dwMajorVersion == 5) && (version.dwMinorVersion >= 1))
    {
      f = fopen(inf_file, "rb");
      
      if(!f)
        return -1;

      while(fgetws(wbuf, sizeof(wbuf)/2, f))
        {
          if(wcsstr(wbuf, inf_comment_uni))
            {
              found = 1;
              break;
            }
        }

      fclose(f);

      if(!found)
        {
          f = fopen(inf_file, "ab");
/*           fputwc(0x000d, f); */
/*           fputwc(0x000d, f); */
          fputws(inf_comment_uni, f);
          fclose(f);
        }
    }
  else
    {
      f = fopen(inf_file, "r");
      
      if(!f)
        return -1;

      while(fgets(buf, sizeof(buf), f))
        {
          if(strstr(buf, inf_comment))
            {
              found = 1;
              break;
            }
        }

      fclose(f);

      if(!found)
        {
          f = fopen(inf_file, "a");
          fputs("\n", f);
          fputs(inf_comment, f);
          fputs("\n", f);
          fclose(f);
        }
    }

  return 0;
}
