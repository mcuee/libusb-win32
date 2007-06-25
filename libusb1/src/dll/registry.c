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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
#include <ddk/cfgmgr32.h>
#else
#include <cfgmgr32.h>
#define strlwr(p) _strlwr(p)
#endif

#include "registry.h"


static bool_t usb_registry_is_nt(void);
static bool_t usb_registry_get_property(DWORD which, HDEVINFO dev_info, 
                                        SP_DEVINFO_DATA *dev_info_data,
                                        char *buf, int size);

static bool_t usb_registry_is_nt(void)
{
  return GetVersion() < 0x80000000 ? TRUE : FALSE;
}

static bool_t usb_registry_get_property(DWORD which, HDEVINFO dev_info, 
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



bool_t usb_registry_get_device(usb_registry_device_t *dev, int index,
                               bool_t present_only)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  DWORD type, length;
  char tmp[1024];
  HKEY key;

  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_index = 0;

  dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
                                 present_only ?
                                 DIGCF_ALLCLASSES | DIGCF_PRESENT
                                 : DIGCF_ALLCLASSES);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      return FALSE;
    }
  
  if(!SetupDiEnumDeviceInfo(dev_info, index, &dev_info_data)) {
    SetupDiDestroyDeviceInfoList(dev_info);
    return FALSE;
  }

  memset(dev, 0, sizeof(*dev));

  if(usb_registry_get_property(SPDRP_HARDWAREID, dev_info, &dev_info_data,
                               tmp, sizeof(tmp))) {
    sscanf(tmp + sizeof("USB\\VID_") - 1, "%04X", &dev->vid);
    sscanf(tmp + sizeof("USB\\VID_XXXX&PID_") - 1, "%04X", &dev->pid);
  }
  usb_registry_get_property(SPDRP_DEVICEDESC, dev_info, &dev_info_data,
                            dev->name, sizeof(dev->name));
  usb_registry_get_property(SPDRP_MFG, dev_info, &dev_info_data,
                            dev->manufacturer, sizeof(dev->manufacturer));
  usb_registry_get_property(SPDRP_SERVICE, dev_info, &dev_info_data,
                            dev->driver, sizeof(dev->driver));
  strlwr(dev->driver);

  usb_registry_get_property(SPDRP_CLASS, dev_info, &dev_info_data,
                            dev->class, sizeof(dev->class));
  usb_registry_get_property(SPDRP_CLASSGUID, dev_info, &dev_info_data,
                            dev->class_guid, sizeof(dev->class_guid));

  key = SetupDiOpenDevRegKey(dev_info, &dev_info_data, 
                             DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
      
  if(key != INVALID_HANDLE_VALUE) {
    memset(tmp, 0, sizeof(tmp));
    length = sizeof(tmp);

    if(RegQueryValueEx(key, "DeviceInterfaceGUIDs", NULL, &type, 
                       (BYTE *)tmp, &length) == ERROR_SUCCESS) {
      strcpy(dev->winusb_guid, tmp);
    }
    RegCloseKey(key);
  }

  SetupDiDestroyDeviceInfoList(dev_info);
  return TRUE;
}
