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

#include <wchar.h>
#include <ctype.h>
#include <string.h>

#include "libusb_driver.h"

#ifndef PLUGPLAY_REGKEY_DEVICE
#define PLUGPLAY_REGKEY_DEVICE  1
#endif
#ifndef PLUGPLAY_REGKEY_DRIVER
#define PLUGPLAY_REGKEY_DRIVER  2
#endif

static int get_hardware_id_nt(DEVICE_OBJECT *device_object, char *data,
                              int size);
static int get_hardware_id_98(DEVICE_OBJECT *device_object, char *data,
                              int size);

int is_device_visible(libusb_device_extension *device_extension)
{
  char hardware_id[512];

  /* NT system ? */
  if(IoIsWdmVersionAvailable(1, 0x10)) 
    {
      if(!get_hardware_id_nt(device_extension->physical_device_object, 
                             hardware_id, 
                             sizeof(hardware_id)))
        return 0;
    }
  else
    {
      if(!get_hardware_id_98(device_extension->physical_device_object, 
                             hardware_id, 
                             sizeof(hardware_id)))
        return 0;
    }


  /* don't filter root hubs and composite device interfaces */
  if(strstr(hardware_id, "ROOT_HUB") 
     || strstr(hardware_id, "&Mi_"))
    {
      return 0;
    }

  return 1;
}


static int get_hardware_id_nt(DEVICE_OBJECT *device_object, char *data,
                              int size)
{
  char buf[512];
  ULONG ret;
  int i;

  memset(data, 0, size);
  memset(buf, 0, sizeof(buf));

  if((IoGetDeviceProperty(device_object,
                          DevicePropertyHardwareID,
                          sizeof(buf) - 1,
                          buf,
                          &ret) == STATUS_SUCCESS) && ret)
    {
      /* convert unicode string to normal character string */
      for(i = 0; (i < sizeof(buf)) && buf[i * 2]; i++)
        {
          data[i] = buf[i * 2];
        }

      return 1;
    }

  return 0;
}

static int get_hardware_id_98(DEVICE_OBJECT *device_object, char *data, 
                              int size)
{
  char buf[512];
  HANDLE reg_key = NULL;
  UNICODE_STRING id_key_uni;
  WCHAR *id = L"HardwareID";
  ULONG tmp;
  int i, length;
  KEY_VALUE_PARTIAL_INFORMATION *id_info 
    = (KEY_VALUE_PARTIAL_INFORMATION *)buf;

  memset(data, 0, size);
  memset(buf, 0, sizeof(buf));

  if(IoOpenDeviceRegistryKey(device_object, PLUGPLAY_REGKEY_DEVICE,
                             STANDARD_RIGHTS_READ, &reg_key) == STATUS_SUCCESS)
    {
      RtlInitUnicodeString(&id_key_uni, id);

      if((ZwQueryValueKey(reg_key, &id_key_uni,
                          KeyValuePartialInformation,
                          buf,
                          sizeof(buf) - 1,
                          &tmp) == STATUS_SUCCESS) && tmp)
        {
          length = (int)(id_info->DataLength/2);

          /* convert unicode string to normal character string */
          for(i = 0; (i < length) && id_info->Data[i * 2]; i++)
            {
              data[i] = id_info->Data[i * 2];
            }

          ZwClose(reg_key);
          return 1;
        }
      ZwClose(reg_key);
    }

  return 0;
}

