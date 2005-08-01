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
#include <ctype.h>
#include <cfgmgr32.h>

#include "registry.h"
#include "win_debug.h"


static const char *driver_name_nt = "libusb0";
static const char *driver_name_9x = "libusb0.sys";

static const char *class_keys_nt[] =
  {
    /* USB devices */
    "SYSTEM\\CurrentControlSet\\Control\\Class"
    "\\{36FC9E60-C465-11CF-8056-444553540000}",
    /* HID devices */
    "SYSTEM\\CurrentControlSet\\Control\\Class\\"
    "{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}",
    /* Network devices */
    "SYSTEM\\CurrentControlSet\\Control\\Class\\"
    "{4D36E972-E325-11CE-BFC1-08002BE10318}",
    /* Image devices */
    "SYSTEM\\CurrentControlSet\\Control\\Class\\"
    "{6bdd1fc6-810f-11d0-bec7-08002be2092f}",
    /* Media devices */
    "SYSTEM\\CurrentControlSet\\Control\\Class\\"
    "{4d36e96c-e325-11ce-bfc1-08002be10318}",
    /* Modem devices */
    "SYSTEM\\CurrentControlSet\\Control\\Class\\"
    "{4D36E96D-E325-11CE-BFC1-08002BE10318}",
    /* SmartCardReader devices*/
    "SYSTEM\\CurrentControlSet\\Control\\Class\\"
    "{50DD5230-BA8A-11D1-BF5D-0000F805F530}",
    NULL
  };

static const char *class_keys_9x[] =
  {
    /* USB devices */
    "SYSTEM\\CurrentControlSet\\Services\\Class\\USB",
    /* HID devices */
    "SYSTEM\\CurrentControlSet\\Services\\Class\\HID",
    /* Network devices */
    "SYSTEM\\CurrentControlSet\\Services\\Class\\Net",
    /* Image devices */
    "SYSTEM\\CurrentControlSet\\Services\\Class\\Image",
    /* Media devices */
    "SYSTEM\\CurrentControlSet\\Services\\Class\\MEDIA",
    /* Modem devices */
    "SYSTEM\\CurrentControlSet\\Services\\Class\\Modem",
    /* SmartCardReader devices, not supported? */
    /*     "SYSTEM\\CurrentControlSet\\Services\\Class\\", */
    NULL
  };

static bool_t usb_registry_set_device_state(DWORD state, HDEVINFO dev_info, 
                                            SP_DEVINFO_DATA *dev_info_data);

bool_t usb_registry_is_nt(void)
{
  return GetVersion() < 0x80000000 ? TRUE : FALSE;
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
    case SPDRP_CLASSGUID:
      val_name = "ClassGUID";
      key_type = DIREG_DEV;    
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

bool_t usb_registry_insert_filter(void)
{
  const char *driver_name;
  const char **key;
  char buf[MAX_PATH];
  
  if(usb_registry_is_nt())
    {
      driver_name = driver_name_nt;
      key = class_keys_nt;
    }
  else
    {
      driver_name = driver_name_9x;
      key = class_keys_9x;
    }

  while(*key)
    {
      memset(buf, 0, sizeof(buf));

      if(usb_registry_get_mz_value(*key, "UpperFilters", buf, sizeof(buf)))
        {
          usb_registry_mz_string_lower(buf);
      
          if(usb_registry_mz_string_find(buf, driver_name))
            {
              key++;
              continue;
            }
        }
      
      usb_registry_mz_string_insert(buf, driver_name);

      if(!usb_registry_set_mz_value(*key, "UpperFilters", buf, 
                                    usb_registry_mz_string_size(buf)))
        {
          return FALSE;
        }

      key++;
    }
 
  return TRUE;
}


bool_t usb_registry_remove_filter(void)
{
  const char *driver_name;
  const char **key;
  char buf[MAX_PATH];
  
  if(usb_registry_is_nt())
    {
      driver_name = driver_name_nt;
      key = class_keys_nt;
    }
  else
    {
      driver_name = driver_name_9x;
      key = class_keys_9x;
    }

  while(*key)
    {
      memset(buf, 0, sizeof(buf));

      if(usb_registry_get_mz_value(*key, "UpperFilters", buf, sizeof(buf)))
        {
          usb_registry_mz_string_lower(buf);
      
          if(usb_registry_mz_string_find(buf, driver_name))
            {
              usb_registry_mz_string_remove(buf, driver_name);
              
              if(!usb_registry_set_mz_value(*key, "UpperFilters", buf, 
                                            usb_registry_mz_string_size(buf)))
                {
                  usb_debug_error("usb_registry_remove_filter: unable to "
                                  "set registry value\n");
                }
            }
        }
      key++;
    }
 
  return TRUE;
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


bool_t usb_registry_match(HDEVINFO dev_info, 
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

bool_t usb_registry_get_mz_value(const char *key, const char *value, 
                                 char *buf, int size)
{
  HKEY reg_key = NULL;
  DWORD reg_type;
  DWORD reg_length = size;
  bool_t ret = FALSE;
  char *p;

  memset(buf, 0, size);
      
  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_ALL_ACCESS, &reg_key)
     == ERROR_SUCCESS)
    {
      if(RegQueryValueEx(reg_key, value, NULL, &reg_type, 
                         buf, &reg_length) == ERROR_SUCCESS)
        {
          if(reg_type == REG_SZ)
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

          ret = TRUE;
        }
    }

  if(reg_key)
    {
      RegCloseKey(reg_key);
    }

  return ret;
}


bool_t usb_registry_set_mz_value(const char *key, const char *value, 
                                 char *buf, int size)
{
  HKEY reg_key = NULL;
  bool_t ret = FALSE;
  char *p;

  /* convert REG_MULTI_SZ to REG_SZ */
  if(!usb_registry_is_nt())
    {
      p = buf;
      
      while(*p && *(p + 1))
        {
          if(*p == 0)
            {
              *p = ',';
            }
          p++;
        }
    }

  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_ALL_ACCESS, &reg_key)
     == ERROR_SUCCESS)
    {
      if(size > 2)
        {
          if(usb_registry_is_nt())
            {
              if(RegSetValueEx(reg_key, value, 0, REG_MULTI_SZ, buf, size)
                 == ERROR_SUCCESS)
                {
                  ret = TRUE;
                }
            }
          else
            {
              if(RegSetValueEx(reg_key, value, 0, REG_SZ, buf, size)
                 == ERROR_SUCCESS)
                {
                  ret = TRUE;
                }
            }
        }
      else
        {
          if(RegDeleteValue(reg_key, value) == ERROR_SUCCESS)
            {
              ret = TRUE;
            }
        }
    }
  
  if(reg_key)
    {
      RegCloseKey(reg_key);
    }

  return ret;
}

int usb_registry_mz_string_size(const char *src)
{
  char *p = (char *)src;
  
  if(!src)
    {
      return 0;
    }

  while(*p)
    {
      p += (strlen(p) + 1);
    }
  
  return p - src + 1;
}

char *usb_registry_mz_string_find_sub(const char *src, const char *str)
{
  while(*src)
    {
      if(strstr(src, str))
        {
          return (char *)src;
        }
      src += (strlen(src) + 1);
    }

  return NULL;
}

char *usb_registry_mz_string_find(const char *src, const char *str)
{
  while(*src)
    {
      if(!strcmp(src, str))
        {
          return (char *)src;
        }
      src += (strlen(src) + 1);
    }

  return NULL;
}

bool_t usb_registry_mz_string_insert(char *src, const char *str)
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

bool_t usb_registry_mz_string_remove(char *src, const char *str)
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
