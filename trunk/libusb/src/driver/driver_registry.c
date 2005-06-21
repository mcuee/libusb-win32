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

static int reg_get_property(DEVICE_OBJECT *physical_device_object, 
                            int property, char *data, int size);

static int reg_get_property(DEVICE_OBJECT *physical_device_object,
                            int property, char *data, int size)
{
  WCHAR tmp[512];
  ULONG ret;
  int i;

  memset(data, 0, size);
  memset(tmp, 0, sizeof(tmp));

  if(NT_SUCCESS(IoGetDeviceProperty(physical_device_object,
                                    property,
                                    sizeof(tmp) - 2,
                                    tmp,
                                    &ret)) && ret)
    {
      /* convert unicode string to normal character string */
      for(i = 0; (i < ret/2) && (i < size); i++)
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
  char tmp[512];

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
  char tmp[512];

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
  char tmp[512];

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
  char tmp[512];

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
