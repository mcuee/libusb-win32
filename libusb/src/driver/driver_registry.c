#/* LIBUSB-WIN32, Generic Windows USB Driver
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

static int get_hardware_id_nt(DEVICE_OBJECT *device_object, char *data,
			      int size);
static int get_hardware_id_98(DEVICE_OBJECT *device_object, char *data,
			      int size);

int is_device_visible(libusb_device_extension *device_extension)
{
  char hardware_id[512];
  char *p;
  int i;
  libusb_device_extension *tmp_extension = NULL;


  memset(hardware_id, 0, sizeof(hardware_id));

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

  p = hardware_id;

  while(*p)
    {
      *p = (char)tolower(*p);
      p++;
    }

  /* don't filter root hubs and composite device interfaces */
  if(strstr(hardware_id, "root_hub") 
     || strstr(hardware_id, "&mi_"))
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

  memset(buf, 0, sizeof(buf));
  memset(data, 0, size);

  if(IoGetDeviceProperty(device_object,
			 DevicePropertyHardwareID,
			 sizeof(buf),
			 buf,
			 &ret) == STATUS_SUCCESS)
    {
      if((ULONG)size < (ret/2 + 1))
	return 0;

      for(i = 0; i < sizeof(buf); i++)
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

  if(IoOpenDeviceRegistryKey(device_object, PLUGPLAY_REGKEY_DEVICE,
 			     STANDARD_RIGHTS_READ, &reg_key) == STATUS_SUCCESS)
     {
       RtlInitUnicodeString(&id_key_uni, id);
       memset(buf, 0, sizeof(buf));
       memset(data, 0, size);

       if((ZwQueryValueKey(reg_key, &id_key_uni,
			   KeyValuePartialInformation,
			   buf,
			   sizeof(buf) - 1,
			   &tmp) == STATUS_SUCCESS) && tmp)
	 {
 	   length = (int)(id_info->DataLength/2);

	   if(length > size)
	     {
	       ZwClose(reg_key);
	       return 0;
	     }
	   
	   for(i = 0; i < length; i++)
	     data[i] = id_info->Data[i * 2];

	   ZwClose(reg_key);
	   return 1;
	 }
       ZwClose(reg_key);
     }
  return 0;
}

