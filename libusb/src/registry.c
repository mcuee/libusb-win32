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

#include <ctype.h>
#include <ddk/cfgmgr32.h>

#include "registry.h"
#include "win_debug.h"


static bool_t usb_registry_set_device_state(DWORD state, HDEVINFO dev_info, 
                                            SP_DEVINFO_DATA *dev_info_data);

bool_t usb_registry_get_property(DWORD which, HDEVINFO dev_info, 
                                 SP_DEVINFO_DATA *dev_info_data,
                                 char *buf, int size)
{
  DWORD length;
  memset(buf, 0, size);

  if(!SetupDiGetDeviceRegistryProperty(dev_info, dev_info_data, which, 
                                       NULL, (BYTE *)buf, 
                                       size, &length))
    {
      return FALSE;
    }
  
  while(*buf)
    {
      strlwr(buf);
      buf += (strlen(buf) + 1);
    }
  
  return TRUE;
}

bool_t usb_registry_set_property(DWORD which, HDEVINFO dev_info, 
                                 SP_DEVINFO_DATA *dev_info_data, 
                                 char *buf, int size)
{
  if(size > 2)
    {
      if(!SetupDiSetDeviceRegistryProperty(dev_info, dev_info_data,
                                           which, (BYTE *)buf, size))
        {
          usb_debug_error("usb_registry_set_property(): setting "
                          "property failed");
          return FALSE;
        }
    }
  else
    {
      if(!SetupDiSetDeviceRegistryProperty(dev_info, dev_info_data,
                                               which, NULL, 0))
        {
          usb_debug_error("usb_registry_set_property(): setting "
                          "property failed");
          return FALSE;
        }
    }

  return TRUE;
}

bool_t usb_registry_insert_filter(HDEVINFO dev_info,
                                  SP_DEVINFO_DATA *dev_info_data, 
                                  char *filter_name)
{
  char filters[MAX_PATH];
  
  usb_registry_get_property(SPDRP_LOWERFILTERS, dev_info, dev_info_data, 
                            filters, sizeof(filters));

  if(usb_registry_mz_string_insert(filters, filter_name))
    {
      if(!usb_registry_set_property(SPDRP_LOWERFILTERS, dev_info, 
                                    dev_info_data, filters,
                                    usb_registry_mz_string_size(filters)))
        {
          return FALSE;
        }
      return TRUE;
    }

  return FALSE;
}


bool_t usb_registry_remove_filter(HDEVINFO dev_info, 
                                  SP_DEVINFO_DATA *dev_info_data, 
                                  char *filter_name)
{
  char filters[MAX_PATH];

  usb_registry_get_property(SPDRP_LOWERFILTERS, dev_info, dev_info_data, 
                            filters, sizeof(filters));

  if(usb_registry_mz_string_remove(filters, filter_name))
    {
      if(!usb_registry_set_property(SPDRP_LOWERFILTERS, dev_info, 
                                    dev_info_data, filters,
                                    usb_registry_mz_string_size(filters)))
        {
          return FALSE;
        }
      return TRUE;
    }
  
  return FALSE;
}

static bool_t usb_registry_set_device_state(DWORD state, HDEVINFO dev_info, 
                                            SP_DEVINFO_DATA *dev_info_data)
{
  SP_PROPCHANGE_PARAMS params;
  
  memset(&params, 0, sizeof(SP_PROPCHANGE_PARAMS));
  
  params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
  params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
  params.StateChange = state;
  params.Scope = DICS_FLAG_GLOBAL;
  params.HwProfile = 0;
	  
  if(!SetupDiSetClassInstallParams(dev_info, dev_info_data,
                                   (SP_CLASSINSTALL_HEADER *) &params,
                                   sizeof(SP_PROPCHANGE_PARAMS)))
    {
      usb_debug_error("usb_registry_set_device_state(): setting class "
                      "install parameters failed");
      return FALSE;
    }
	  
  if(!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, dev_info, 
                                dev_info_data))
    {
      usb_debug_error("usb_registry_set_device_state(): calling class "
                      "installer failed");
      return FALSE;
    }

  return TRUE;
}

bool_t usb_registry_restart_device(HDEVINFO dev_info, 
                                   SP_DEVINFO_DATA *dev_info_data)
{
  return usb_registry_set_device_state(DICS_PROPCHANGE, dev_info, 
                                       dev_info_data);
}

bool_t usb_registry_stop_device(HDEVINFO dev_info, 
                                SP_DEVINFO_DATA *dev_info_data)
{
  return usb_registry_set_device_state(DICS_DISABLE, dev_info, 
                                       dev_info_data);
}

bool_t usb_registry_start_device(HDEVINFO dev_info, 
                                 SP_DEVINFO_DATA *dev_info_data)
{
  return usb_registry_set_device_state(DICS_ENABLE, dev_info, 
                                       dev_info_data);
}

bool_t usb_registry_is_service_libusb(HDEVINFO dev_info, 
                                      SP_DEVINFO_DATA *dev_info_data)
{
  char service_name[MAX_PATH];

  if(!usb_registry_get_property(SPDRP_SERVICE, dev_info, dev_info_data,
                                service_name, sizeof(service_name)))
    {
      return FALSE;
    }
 
  if(usb_registry_mz_string_find_sub(service_name, LIBUSB_DRIVER_NAME))
    {
      return TRUE;
    }

  return FALSE;
}

void usb_registry_start_filter(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;

  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  dev_index = 0;
  dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, 
                                 /* DIGCF_ALLCLASSES | DIGCF_PRESENT); */
                                 DIGCF_ALLCLASSES);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_registry_start_filter(): getting "
                      "device info set failed");
      
      return;
    }
  
  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(usb_registry_match(dev_info, &dev_info_data))
        {
          if(usb_registry_insert_filter(dev_info, &dev_info_data,
                                        LIBUSB_DRIVER_NAME))
            {
              usb_registry_restart_device(dev_info, &dev_info_data);
            }
        }
      dev_index++;
    }

  SetupDiDestroyDeviceInfoList(dev_info);
}


void usb_registry_stop_filter(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_index = 0;

  dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_registry_stop_filter(): getting "
                      "device info set failed");
      return;
    }
  
  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(usb_registry_remove_filter(dev_info, &dev_info_data, 
                                    LIBUSB_DRIVER_NAME))
        {
          usb_registry_restart_device(dev_info, &dev_info_data);
        }
      dev_index++;
    }
  
  SetupDiDestroyDeviceInfoList(dev_info);
}

void usb_registry_stop_libusb_devices(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_index = 0;

  dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
                                 DIGCF_ALLCLASSES | DIGCF_PRESENT);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_registry_stop_libusb_devices(): getting "
                      "device info set failed");
      return;
    }
  
  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(usb_registry_is_service_libusb(dev_info, &dev_info_data))
        {
          usb_registry_stop_device(dev_info, &dev_info_data);
        }
      dev_index++;
    }
  
  SetupDiDestroyDeviceInfoList(dev_info);
}

void usb_registry_start_libusb_devices(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_index = 0;

  dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
                                 DIGCF_ALLCLASSES | DIGCF_PRESENT);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_registry_stop_libusb_devices(): getting "
                      "device info set failed");
      return;
    }
  
  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(usb_registry_is_service_libusb(dev_info, &dev_info_data))
        {
          usb_registry_start_device(dev_info, &dev_info_data);
        }
      dev_index++;
    }
  
  SetupDiDestroyDeviceInfoList(dev_info);
}

bool_t usb_registry_match(HDEVINFO dev_info, SP_DEVINFO_DATA *dev_info_data)
{
  char tmp[MAX_PATH];
  
  if(!usb_registry_get_property(SPDRP_HARDWAREID, dev_info, dev_info_data,
                                tmp, sizeof(tmp)))
    {
      usb_debug_error("usb_registry_match(): getting hardware id failed");
      return FALSE;
    }
  
  /* search for USB devices, skip root hubs and interfaces of composite */
  /* devices */
  if(!usb_registry_mz_string_find_sub(tmp, "usb\\vid_")
     || usb_registry_mz_string_find_sub(tmp, "root_hub")
     || usb_registry_mz_string_find_sub(tmp, "&mi_"))
    {
      return FALSE;
    }

  if(!usb_registry_get_property(SPDRP_SERVICE, dev_info, dev_info_data,
                                tmp, sizeof(tmp)))
    {
      usb_debug_error("usb_registry_match(): getting service failed");
      return FALSE;
    }
  
  /* is this device handled by libusb's kernel driver? */
  if(usb_registry_mz_string_find_sub(tmp, "libusb"))
    {
      return FALSE;
    }

  return TRUE;
}

int usb_registry_mz_string_size(char *src)
{
  char *p = src;
  
  while(*p)
    {
      p += (strlen(p) + 1);
    }
  
  return p - src + 1;
}

char *usb_registry_mz_string_find_sub(char *src, char *str)
{
  while(*src)
    {
      if(strstr(src, str))
        {
          return src;
        }
      src += (strlen(src) + 1);
    }

  return NULL;
}

char *usb_registry_mz_string_find(char *src, char *str)
{
  while(*src)
    {
      if(!strcmp(src, str))
        {
          return src;
        }
      src += (strlen(src) + 1);
    }

  return NULL;
}

bool_t usb_registry_mz_string_insert(char *src, char *str)
{
  if(usb_registry_mz_string_find(src, str))
    {
      return FALSE;
    }

  while(*src)
    {
      src += (strlen(src) + 1);
    }

  memcpy(src, str, strlen(str));

  src += strlen(str);

  *src = 0;
  *(src + 1) = 0;

  return TRUE;
}

bool_t usb_registry_mz_string_remove(char *src, char *str)
{
  char *p, *q;

  p = usb_registry_mz_string_find(src, str);

  if(!p)
    {
      return FALSE;
    }

  q = p;

  while(*q)
    {
      q += (strlen(q) + 1);
    }

  memmove(p, p + strlen(p) + 1, q - p + strlen(p) + 1);

  return TRUE;
}

bool_t usb_registry_insert_co_installer(char *guid)
{
  HKEY reg_key = NULL;
  char tmp[MAX_PATH];
  DWORD size = MAX_PATH;

  memset(tmp, 0, sizeof(tmp));

  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                  "SYSTEM\\CurrentControlSet\\Control\\CoDeviceInstallers",
                  0, KEY_ALL_ACCESS, &reg_key)
     == ERROR_SUCCESS)
    {

      RegQueryValueEx(reg_key, guid, NULL, NULL, 
                      (BYTE *)tmp, &size);

      if(usb_registry_mz_string_insert(tmp, LIBUSB_COINSTALLER_ENTRY))
        {
          if(RegSetValueEx(reg_key, guid, 0, REG_MULTI_SZ, (BYTE *)tmp, 
                           usb_registry_mz_string_size(tmp)) != ERROR_SUCCESS)
            {
              usb_debug_error("usb_registry_insert_co_installer(): setting registry value failed");
            }
          else
            {
              return TRUE;
            }
        }

      RegCloseKey(reg_key);
      return TRUE;
    }
  return FALSE;
}

bool_t usb_registry_remove_co_installer(char *guid)
{
  HKEY reg_key = NULL;
  char tmp[MAX_PATH];
  DWORD size = MAX_PATH;

  memset(tmp, 0, sizeof(tmp));

  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                  "SYSTEM\\CurrentControlSet\\Control\\CoDeviceInstallers",
                  0, KEY_ALL_ACCESS, &reg_key)
     == ERROR_SUCCESS)
    {

      RegQueryValueEx(reg_key, guid, NULL, NULL, 
                      (BYTE *)tmp, &size);

      if(usb_registry_mz_string_remove(tmp, LIBUSB_COINSTALLER_ENTRY))
        {
          if(*tmp)
            {
              if(RegSetValueEx(reg_key, guid, 0, REG_MULTI_SZ, (BYTE *)tmp, 
                               usb_registry_mz_string_size(tmp)) != ERROR_SUCCESS)
                {
                  usb_debug_error("usb_registry_remove_co_installer(): setting registry value failed");
                }
              else
                {
                  return TRUE;
                }
            }
          else
            {
              RegDeleteValue(reg_key, guid);
            }
        }

      RegCloseKey(reg_key);
      return TRUE;
    }
  return FALSE;
}
