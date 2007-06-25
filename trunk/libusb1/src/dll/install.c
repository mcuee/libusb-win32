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
#include <winsvc.h>

/* SetupCopyOEMInf will be loaded dynamically */
#define SetupCopyOEMInfA SetupCopyOEMInfA_orig
#define SetupCopyOEMInfW SetupCopyOEMInfW_orig
#include <setupapi.h>
#undef SetupCopyOEMInfA
#undef SetupCopyOEMInfW

#include <stdio.h>
#include <regstr.h>
#include <wchar.h>
#include <string.h>

#ifdef __GNUC__
#include <ddk/cfgmgr32.h>
#else
#include <cfgmgr32.h>
#define strlwr(p) _strlwr(p)
#endif

#include "usbi.h"
#include "dll_load.h"


/* newdev.dll interface */
DLL_DECLARE(WINAPI, BOOL, UpdateDriverForPlugAndPlayDevices, 
            (HWND, LPCSTR, LPCSTR, DWORD, PBOOL));

/* setupapi.dll interface */
#ifndef INSTALLFLAG_FORCE
#define INSTALLFLAG_FORCE 0x00000001
#endif

DLL_DECLARE(WINAPI, BOOL, SetupCopyOEMInf, (PCSTR, PCSTR, DWORD, DWORD,
                                            PSTR, DWORD, PDWORD, PSTR*));




int usbi_install_inf_file(const char *inf_file)
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

  DLL_LOAD(newdev.dll, UpdateDriverForPlugAndPlayDevices, TRUE);
  DLL_LOAD(setupapi.dll, SetupCopyOEMInf, TRUE);

  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  /* retrieve the full .inf file path */
  if(!GetFullPathName(inf_file, MAX_PATH, inf_path, NULL))
    {
      USBI_DEBUG_ERROR(".inf file %s not found\n", inf_file);
      return -1;
    }

  /* open the .inf file */
  inf_handle = SetupOpenInfFile(inf_path, NULL, INF_STYLE_WIN4, NULL);

  if(inf_handle == INVALID_HANDLE_VALUE)
    {
      USBI_DEBUG_ERROR("unable to open .inf file %s\n", inf_file);
      return -1;
    }

  /* find the .inf file's device description section marked "Devices" */
  if(!SetupFindFirstLine(inf_handle, "Devices", NULL, &inf_context))
    {
      USBI_DEBUG_ERROR(".inf file %s does not contain any device descriptions\n", 
                inf_file);
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

#if 0
  usb_registry_stop_libusb_devices(); /* stop all libusb devices */
  usb_registry_start_libusb_devices(); /* restart all libusb devices */
#endif

  return 0;
}


int usbi_install_touch_inf_file(const char *inf_file)
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

int usbi_install_needs_restart(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  SP_DEVINSTALL_PARAMS install_params;
  int ret = FALSE;

  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_info = SetupDiGetClassDevs(NULL, NULL, NULL,
                                 DIGCF_ALLCLASSES | DIGCF_PRESENT);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      USBI_DEBUG_ERROR("unable to get device inforamtion set");
      return ret;
    }
  
  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      memset(&install_params, 0, sizeof(SP_PROPCHANGE_PARAMS));
      install_params.cbSize = sizeof(SP_DEVINSTALL_PARAMS);

      if(SetupDiGetDeviceInstallParams(dev_info, &dev_info_data, 
                                       &install_params))
        {
          if(install_params.Flags & (DI_NEEDRESTART | DI_NEEDREBOOT))
            {
              USBI_DEBUG_TRACE("restart needed");
              ret = TRUE;
              break;
            }
        }
      
      dev_index++;
    }
  
  SetupDiDestroyDeviceInfoList(dev_info);

  return ret;
}
