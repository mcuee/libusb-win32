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

bool_t usb_registry_is_nt(void)
{
  return GetVersion() < 0x80000000 ? 1 : 0;
}

bool_t usb_registry_get_property(DWORD which, HDEVINFO dev_info, 
                                 SP_DEVINFO_DATA *dev_info_data,
                                 char *buf, int size)
{
  DWORD reg_type;
  DWORD key_type;
  DWORD length = size;
  char *p = NULL;
  char *val_name = NULL;
  HKEY reg_key = NULL;
  
  memset(buf, 0, size);

  switch(which)
    {
    case SPDRP_LOWERFILTERS:
      val_name = "LowerFilters";
      key_type = DIREG_DEV; 
      break;
    case SPDRP_UPPERFILTERS:
      val_name = "UpperFilters";
      key_type = DIREG_DEV; 
      break;
    case SPDRP_SERVICE:
      val_name = "NTMPDriver";
      key_type = DIREG_DRV;
      break;
    case SPDRP_CLASS:
      val_name = "Class";
      key_type = DIREG_DEV;    
      break;
    case SPDRP_HARDWAREID:
      val_name = "HardwareID";
      key_type = DIREG_DEV;
      break;
    case SPDRP_DEVICEDESC:
      val_name = "DeviceDesc";
      key_type = DIREG_DEV;
      break;
    case SPDRP_MFG:
      val_name = "Mfg";
      key_type = DIREG_DEV;
      break;

    default:
      return FALSE;
    }

  if(usb_registry_is_nt())
    {
      if(!SetupDiGetDeviceRegistryProperty(dev_info, dev_info_data, which,  
                                           &reg_type, (BYTE *)buf, 
                                           size, &length))
        {
          return FALSE;
        }

      if(length <= 2)
        {
          return FALSE;
        }
    }
  else /* Win9x */
    {
      reg_key = SetupDiOpenDevRegKey(dev_info, dev_info_data, 
                                     DICS_FLAG_GLOBAL, 
                                     0, key_type, KEY_ALL_ACCESS);
      
      if(reg_key == INVALID_HANDLE_VALUE)
        {
          usb_debug_error("usb_registry_get_property(): reading "
                          "registry key failed");
          return FALSE;
        }
      
      if(RegQueryValueEx(reg_key, val_name, NULL, &reg_type, 
                         (BYTE *)buf, &length) != ERROR_SUCCESS
         || length <= 2)
        {
          RegCloseKey(reg_key);
          return FALSE;
        }

      RegCloseKey(reg_key);
      
      if(reg_type == REG_MULTI_SZ)
        {
          p = buf;
          while(*p)
            {
              if(*p == ',')
                {
                  *p = 0;
                }
              p++;
            }
        }
    }
 
  return TRUE;
}

bool_t usb_registry_set_property(DWORD which, HDEVINFO dev_info, 
                                 SP_DEVINFO_DATA *dev_info_data, 
                                 char *buf, int size)
{
  char *val_name = NULL;
  char *p = NULL;
  HKEY reg_key;
  DWORD reg_type;

  switch(which)
    {
    case SPDRP_LOWERFILTERS:
      reg_type = usb_registry_is_nt() ? REG_MULTI_SZ : REG_SZ;
      val_name = "LowerFilters";
      break;
    case SPDRP_UPPERFILTERS:
      reg_type = usb_registry_is_nt() ? REG_MULTI_SZ : REG_SZ;
      val_name = "UpperFilters";
      break;
    default:
      return 0;
    }

  if(usb_registry_is_nt())
    {
      if(size > 2)
        {
          if(!SetupDiSetDeviceRegistryProperty(dev_info, dev_info_data,
                                               which, (BYTE *)buf, size))
            {
              usb_debug_error("usb_registry_set_property(): setting "
                              "property '%s' failed", val_name);
              return FALSE;
            }
        }
      else
        {
          if(!SetupDiSetDeviceRegistryProperty(dev_info, dev_info_data,
                                               which, NULL, 0))
            {
              usb_debug_error("usb_registry_set_property(): deleting "
                              "property '%s' failed", val_name);
              return FALSE;
            }
        }
    }
  else
    {
      p = buf;
      
      while(*p)
        {
          if(*p == ',')
            {
              *p = 0;
            }
          p += (strlen(p) + 1);
        }

      reg_key = SetupDiOpenDevRegKey(dev_info, dev_info_data, 
                                     DICS_FLAG_GLOBAL, 
                                     0, DIREG_DEV, KEY_ALL_ACCESS);
      
      if(reg_key == INVALID_HANDLE_VALUE)
        {
          usb_debug_error("usb_registry_set_property(): reading "
                          "registry key failed");
          return FALSE;
        }
      
      if(size > 3)
        {
          if(RegSetValueEx(reg_key, val_name, 0, reg_type, (BYTE *)buf, 
                           size) != ERROR_SUCCESS)
            {
              usb_debug_error("usb_registry_set_property(): setting "
                              "property '%s' failed", val_name);
              RegCloseKey(reg_key);
              return FALSE;
            }
        }
      else
        {
          if(RegDeleteValue(reg_key, val_name) != ERROR_SUCCESS)
            {
              usb_debug_error("usb_registry_set_property(): deleting "
                              "property '%s' failed", val_name);
              RegCloseKey(reg_key);
              return FALSE;
            }
        }
      RegCloseKey(reg_key);
    }

  return TRUE;
}

bool_t usb_registry_insert_filter(HDEVINFO dev_info,
                                  SP_DEVINFO_DATA *dev_info_data, 
                                  char *filter_name)
{
  char filters[MAX_PATH];
  
  usb_registry_get_property(SPDRP_UPPERFILTERS, dev_info, dev_info_data, 
                            filters, sizeof(filters));

  usb_registry_mz_string_lower(filters);

  if(!usb_registry_mz_string_find(filters, filter_name))
    {
      usb_registry_mz_string_insert(filters, filter_name);
      
      if(!usb_registry_set_property(SPDRP_UPPERFILTERS, dev_info, 
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

  usb_registry_get_property(SPDRP_UPPERFILTERS, dev_info, dev_info_data, 
                            filters, sizeof(filters));

  usb_registry_mz_string_lower(filters);

  if(usb_registry_mz_string_find(filters, filter_name))
    {
      usb_registry_mz_string_remove(filters, filter_name);
      
      if(!usb_registry_set_property(SPDRP_UPPERFILTERS, dev_info, 
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
  
   usb_registry_mz_string_lower(service_name);
   
   if(usb_registry_mz_string_find_sub(service_name, "libusb"))
     {
       return TRUE;
     }

  return FALSE;
}

void usb_registry_start_filter(bool_t restart)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char *filter_name = NULL;

  filter_name = usb_registry_is_nt() ? 
    LIBUSB_DRIVER_NAME_NT : LIBUSB_DRIVER_NAME_9X;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  dev_index = 0;
  dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, 
                                 DIGCF_ALLCLASSES | DIGCF_PRESENT);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_registry_start_filter(): getting "
                      "device info set failed");
      
      return;
    }
    while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(usb_registry_match(dev_info, &dev_info_data)
         && !usb_registry_is_service_libusb(dev_info, &dev_info_data))
        {
          if(usb_registry_insert_filter(dev_info, &dev_info_data, filter_name))
            {
              if(restart)
                usb_registry_restart_device(dev_info, &dev_info_data);
            }
        }
      dev_index++;
    }

  SetupDiDestroyDeviceInfoList(dev_info);
}


void usb_registry_stop_filter(bool_t restart)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char *filter_name = NULL;

  filter_name = usb_registry_is_nt() ? 
    LIBUSB_DRIVER_NAME_NT : LIBUSB_DRIVER_NAME_9X;
  
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
                                    filter_name))
        {
          if(restart)
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

  usb_registry_mz_string_lower(tmp);

  /* search for USB devices, skip root hubs and interfaces of composite */
  /* devices */
  if(usb_registry_mz_string_find_sub(tmp, "&mi_"))
    {
      return FALSE;
    }

  return TRUE;
}

bool_t usb_registry_match_no_root_hubs(HDEVINFO dev_info, 
                                  SP_DEVINFO_DATA *dev_info_data)
{
  char tmp[MAX_PATH];
  
  if(!usb_registry_get_property(SPDRP_HARDWAREID, dev_info, dev_info_data,
                                tmp, sizeof(tmp)))
    {
      usb_debug_error("usb_registry_match_no_hubs(): getting hardware id "
                      "failed");
      return FALSE;
    }

  usb_registry_mz_string_lower(tmp);

  /* search for USB devices, skip root hubs and interfaces of composite */
  /* devices */
  if(usb_registry_mz_string_find_sub(tmp, "&mi_")
     || usb_registry_mz_string_find_sub(tmp, "root_hub"))
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

void usb_registry_mz_string_lower(char *src)
{
  while(*src)
    {
      strlwr(src);
      src += (strlen(src) + 1);
    }
}


int usb_registry_get_num_busses(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char id[MAX_PATH];
  int ret = 0;

  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
                                 DIGCF_ALLCLASSES | DIGCF_PRESENT);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_registry_get_num_busses(): getting "
                      "device info set failed");
      return 0;
    }
  
  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(!usb_registry_get_property(SPDRP_HARDWAREID, dev_info, &dev_info_data,
                                    id, sizeof(id)))
        {
          usb_debug_error("usb_registry_get_num_busses(): getting hardware "
                          "id failed");
          dev_index++;
          continue;
        }
      
      usb_registry_mz_string_lower(id);
      
      /* search for USB root hubs */
      if(usb_registry_mz_string_find_sub(id, "root_hub"))
        {
          ret++;
        }
      dev_index++;
    }
  
  SetupDiDestroyDeviceInfoList(dev_info);

  return ret;
}

bool_t usb_registry_restart_root_hubs(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char id[MAX_PATH];

  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
                                 DIGCF_ALLCLASSES | DIGCF_PRESENT);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_registry_restart_root_hubs(): getting "
                      "device info set failed");
      return 0;
    }
  
  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(!usb_registry_get_property(SPDRP_HARDWAREID, dev_info, &dev_info_data,
                                    id, sizeof(id)))
        {
          usb_debug_error("usb_registry_restart_root_hubs(): getting hardware "
                          "id failed");
          dev_index++;
          continue;
        }
      
      usb_registry_mz_string_lower(id);
      
      /* search for USB root hubs */
      if(usb_registry_mz_string_find_sub(id, "root_hub"))
        {
          usb_registry_restart_device(dev_info, &dev_info_data);
        }
      dev_index++;
    }
  
  SetupDiDestroyDeviceInfoList(dev_info);

  return TRUE;
}
