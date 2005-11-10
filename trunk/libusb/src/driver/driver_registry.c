/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
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


#include "libusb_driver.h"

/* missing in mingw's ddk headers */
#define PLUGPLAY_REGKEY_DEVICE  1
#define PLUGPLAY_REGKEY_DRIVER  2
#define PLUGPLAY_REGKEY_CURRENT_HWPROFILE 4

#define LIBUSB_REG_IS_DEVICE_DRIVER L"libusb_is_device_driver"


static int reg_get_property(DEVICE_OBJECT *physical_device_object, 
                            int property, char *data, int size);

static int reg_get_property(DEVICE_OBJECT *physical_device_object,
                            int property, char *data, int size)
{
  WCHAR tmp[512];
  ULONG ret;
  ULONG i;

  if(!physical_device_object || !data || !size)
    {
      return FALSE;
    }

  memset(data, 0, size);
  memset(tmp, 0, sizeof(tmp));

  if(NT_SUCCESS(IoGetDeviceProperty(physical_device_object,
                                    property,
                                    sizeof(tmp) - 2,
                                    tmp,
                                    &ret)) && ret)
    {
      /* convert unicode string to normal character string */
      for(i = 0; (i < ret/2) && (i < ((ULONG)size - 1)); i++)
        {
          data[i] = (char)tmp[i];
        }
      
      _strlwr(data);

      return TRUE;
    }

  return FALSE;
}

int reg_is_usb_device(DEVICE_OBJECT *physical_device_object)
{
  char tmp[256];

  if(!physical_device_object)
    {
      return FALSE;
    }

  if(reg_get_property(physical_device_object, DevicePropertyHardwareID, 
                      tmp, sizeof(tmp)))
    {
      if(strstr(tmp, "usb\\"))
        {
          return TRUE;
        }
    }
  
  return FALSE;
}

int reg_is_root_hub(DEVICE_OBJECT *physical_device_object)
{
  char tmp[256];

  if(!physical_device_object)
    {
      return FALSE;
    }

  if(reg_get_property(physical_device_object, DevicePropertyHardwareID,
                      tmp, sizeof(tmp)))
    {
      if(strstr(tmp, "root_hub"))
        {
          return TRUE;
        }
    }
  
  return FALSE;
}

int reg_is_hub(DEVICE_OBJECT *physical_device_object)
{
  char tmp[256];

  if(!physical_device_object)
    {
      return FALSE;
    }

  if(reg_get_property(physical_device_object, DevicePropertyHardwareID,
                      tmp, sizeof(tmp)))
    {
      if(strstr(tmp, "hub"))
        {
          return TRUE;
        }
    }
  
  return FALSE;
}

int reg_is_composite_interface(DEVICE_OBJECT *physical_device_object)
{
  char tmp[256];

  if(!physical_device_object)
    {
      return FALSE;
    }

  if(reg_get_property(physical_device_object, DevicePropertyHardwareID, 
                      tmp, sizeof(tmp)))
    {
      if(strstr(tmp, "&mi_"))
        {
          return TRUE;
        }
    }
  
  return FALSE;
}

int reg_is_filter_driver(DEVICE_OBJECT *physical_device_object)
{
  HANDLE key = NULL;
  NTSTATUS status;
  UNICODE_STRING name;
  KEY_VALUE_FULL_INFORMATION *info;
  ULONG length;
  int ret = TRUE;

  if(!physical_device_object)
    {
      return FALSE;
    }

  status = IoOpenDeviceRegistryKey(physical_device_object,
                                   PLUGPLAY_REGKEY_DEVICE,
                                   STANDARD_RIGHTS_ALL,
                                   &key);
  if(NT_SUCCESS(status)) 
    {
      RtlInitUnicodeString(&name, LIBUSB_REG_IS_DEVICE_DRIVER);
      
      length = sizeof(KEY_VALUE_FULL_INFORMATION) + name.MaximumLength
        + sizeof(ULONG);

      info = ExAllocatePool(NonPagedPool, length);
      
      if(info) 
        {
          status = ZwQueryValueKey(key, &name, KeyValueFullInformation,
                                   info, length, &length);
          
          if(NT_SUCCESS(status)) 
            {
              ret = FALSE;
            }
          
          ExFreePool(info);
        }
      
      ZwClose(key);
    }

  return ret;
}

int reg_get_id(DEVICE_OBJECT *physical_device_object, char *data, int size)
{
  if(!physical_device_object || !data || !size)
    {
      return FALSE;
    }

  return reg_get_property(physical_device_object, DevicePropertyHardwareID, 
                          data, size);
}
