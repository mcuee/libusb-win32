/* libusb-win32, Generic Windows USB Library
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
#include "lusb_defdi_guids.h"

/* missing in mingw's ddk headers */
#ifndef PLUGPLAY_REGKEY_DEVICE
#define PLUGPLAY_REGKEY_DEVICE  1
#endif

#define LIBUSB_REG_SURPRISE_REMOVAL_OK	L"SurpriseRemovalOK"
#define LIBUSB_REG_INITIAL_CONFIG_VALUE	L"InitialConfigValue"
#define LIBUSB_REG_DEVICE_INTERFACE_GUIDS L"DeviceInterfaceGUIDs"

static bool_t reg_get_property(DEVICE_OBJECT *physical_device_object,
                               int property, char *data, int size);

static bool_t reg_get_property(DEVICE_OBJECT *physical_device_object,
                               int property, char *data, int size)
{
    WCHAR tmp[512];
    ULONG ret;
    ULONG i;

    if (!physical_device_object || !data || !size)
    {
        return FALSE;
    }

    memset(data, 0, size);
    memset(tmp, 0, sizeof(tmp));

    if (NT_SUCCESS(IoGetDeviceProperty(physical_device_object,
                                       property,
                                       sizeof(tmp) - 2,
                                       tmp,
                                       &ret)) && ret)
    {
        /* convert unicode string to normal character string */
        for (i = 0; (i < ret/2) && (i < ((ULONG)size - 1)); i++)
        {
            data[i] = (char)tmp[i];
        }

        _strlwr(data);

        return TRUE;
    }

    return FALSE;
}


bool_t reg_get_properties(libusb_device_t *dev)
{
    HANDLE key = NULL;
	PVOID keyObject = NULL;
    NTSTATUS status;
    UNICODE_STRING surprise_removal_ok_name;
    UNICODE_STRING initial_config_value_name;
    UNICODE_STRING device_interface_guids;
    UNICODE_STRING device_interface_guid_value;
    KEY_VALUE_FULL_INFORMATION *info;
    ULONG pool_length;
    ULONG length;
	ULONG val;
	LPWSTR valW;

	if (!dev->physical_device_object)
    {
        return FALSE;
    }

    /* default settings */
    dev->surprise_removal_ok = FALSE;
    dev->is_filter = TRUE;
	dev->initial_config_value = SET_CONFIG_ACTIVE_CONFIG;

    status = IoOpenDeviceRegistryKey(dev->physical_device_object,
                                     PLUGPLAY_REGKEY_DEVICE,
                                     STANDARD_RIGHTS_ALL,
                                     &key);
    if (NT_SUCCESS(status))
    {
        RtlInitUnicodeString(&surprise_removal_ok_name, 
			LIBUSB_REG_SURPRISE_REMOVAL_OK);

        RtlInitUnicodeString(&initial_config_value_name, 
			LIBUSB_REG_INITIAL_CONFIG_VALUE);

        RtlInitUnicodeString(&device_interface_guids, 
			LIBUSB_REG_DEVICE_INTERFACE_GUIDS);
		
		pool_length = sizeof(KEY_VALUE_FULL_INFORMATION) + 512;

        info = ExAllocatePool(NonPagedPool, pool_length);
		if (!info)
		{
			ZwClose(key);
			USBERR("ExAllocatePool failed allocating %d bytes\n", pool_length);
			return FALSE;
		}


		// get surprise_removal_ok
		// get is_filter
		length = pool_length;
        memset(info, 0, length);

        status = ZwQueryValueKey(key, &surprise_removal_ok_name, 
			KeyValueFullInformation, info, length, &length);

        if (NT_SUCCESS(status) && (info->Type == REG_DWORD))
        {
            val = *((ULONG *)(((char *)info) + info->DataOffset));

            dev->surprise_removal_ok = val ? TRUE : FALSE;
            dev->is_filter = FALSE;
        }

		if (!dev->is_filter)
		{
			// get device interface guid
			length = pool_length;
			memset(info, 0, length);

			status = ZwQueryValueKey(key, &device_interface_guids, 
				KeyValueFullInformation, info, length, &length);

			if (NT_SUCCESS(status) && (info->Type == REG_MULTI_SZ))
			{
				valW = ((LPWSTR)(((char *)info) + info->DataOffset));
				RtlInitUnicodeString(&device_interface_guid_value,valW);
				status = RtlGUIDFromString(&device_interface_guid_value, &dev->device_interface_guid);
				if (NT_SUCCESS(status))
				{
					if (IsEqualGUID(&dev->device_interface_guid, &LibusbKDeviceGuid))
					{
						USBWRN0("libusbK default device DeviceInterfaceGUID found. skippng..\n");
						RtlInitUnicodeString(&device_interface_guid_value,Libusb0DeviceGuidW);
					} 
					else if (IsEqualGUID(&dev->device_interface_guid, &Libusb0FilterGuid))
					{
						USBWRN0("libusb0 filter DeviceInterfaceGUID found. skippng..\n");
						RtlInitUnicodeString(&device_interface_guid_value,Libusb0DeviceGuidW);
					}
					else if (IsEqualGUID(&dev->device_interface_guid, &Libusb0DeviceGuid))
					{
						USBWRN0("libusb0 device DeviceInterfaceGUID found. skippng..\n");
						RtlInitUnicodeString(&device_interface_guid_value,Libusb0DeviceGuidW);
					}
					else
					{
						USBMSG0("found user specified device interface guid.\n");
						dev->device_interface_in_use = TRUE;
					}
				}
				else
				{
					USBERR0("invalid user specified device interface guid.");
				}
			}
			if (!dev->device_interface_in_use)
			{
				RtlInitUnicodeString(&device_interface_guid_value,Libusb0DeviceGuidW);
			}
		}
		else
		{
			RtlInitUnicodeString(&device_interface_guid_value,Libusb0FilterGuidW);
		}

		if (!dev->device_interface_in_use)
		{
			status = RtlGUIDFromString(&device_interface_guid_value, &dev->device_interface_guid);
			if (NT_SUCCESS(status))
			{
				USBMSG0("using default device interface guid.\n");
				dev->device_interface_in_use = TRUE;
			}
			else
			{
				USBERR0("failed using default device interface guid.\n");
			}
		}

		// get initial_config_value
		length = pool_length;
        memset(info, 0, length);

        status = ZwQueryValueKey(key, &initial_config_value_name,
			KeyValueFullInformation, info, length, &length);

        if (NT_SUCCESS(status) && (info->Type == REG_DWORD))
        {
            val = *((ULONG *)(((char *)info) + info->DataOffset));
            dev->initial_config_value = (int)val;
        }

		status = ObReferenceObjectByHandle(key, KEY_READ, NULL, KernelMode, &keyObject, NULL);
		if (NT_SUCCESS(status))
		{
			length = pool_length;
			memset(info, 0, length);
			status = ObQueryNameString(keyObject, (POBJECT_NAME_INFORMATION)info, length, &length);
			if (NT_SUCCESS(status))
			{
				PWSTR nameW =((POBJECT_NAME_INFORMATION)info)->Name.Buffer;
				PSTR  nameA = dev->objname_plugplay_registry_key;

				val=0;
				while (nameW[val] && val < (length/2))
				{
					*nameA=(char)nameW[val];
					nameA++;
					val++;
				}
				*nameA='\0';

				USBDBG("reg-key-name=%s\n",dev->objname_plugplay_registry_key);
			}
			else
			{
				USBERR("ObQueryNameString failed. status=%Xh\n",status);
			}

			ObDereferenceObject(keyObject);
		}
		else
		{
			USBERR("ObReferenceObjectByHandle failed. status=%Xh\n",status);
		}

        ZwClose(key);
        ExFreePool(info);
    }

    return TRUE;
}

bool_t reg_get_hardware_id(DEVICE_OBJECT *physical_device_object,
                           char *data, int size)
{
    if (!physical_device_object || !data || !size)
    {
        return FALSE;
    }

    return reg_get_property(physical_device_object, DevicePropertyHardwareID,
                            data, size);
}

bool_t reg_get_compatible_id(DEVICE_OBJECT *physical_device_object,
                           char *data, int size)
{
    if (!physical_device_object || !data || !size)
    {
        return FALSE;
    }

    return reg_get_property(physical_device_object, DevicePropertyCompatibleIDs,
                            data, size);
}
/*
Gets a device property for the device_object.

Returns: NTSTATUS code from IoGetDeviceProperty 
         STATUS_INVALID_PARAMETER
*/
NTSTATUS reg_get_device_property(PDEVICE_OBJECT device_object,
							   int property, 
							   char* data_buffer,
							   int data_length,
							   int* actual_length)
{
	NTSTATUS status = STATUS_INVALID_PARAMETER;

    if (!device_object || !data_buffer || !data_length || !actual_length)
		return status;

	// clear data
    memset(data_buffer, 0, data_length);
	*actual_length=0;

	// get device property
    status = IoGetDeviceProperty(
		device_object, property, data_length, data_buffer, (PULONG)actual_length);

    return status;
}

/*
Gets a property from the device registry key of the device_object.

Returns: NTSTATUS from ZwQueryValueKey
         NTSTATUS from IoOpenDeviceRegistryKey if the device registry
		          key could not be opened.
*/
NTSTATUS reg_get_custom_property(PDEVICE_OBJECT device_object,
								 char *data_buffer, 
								 unsigned int data_length, 
								 unsigned int name_offset, 
								 int* actual_length)
{
    HANDLE key = NULL;
    NTSTATUS status;
    UNICODE_STRING name;
    KEY_VALUE_FULL_INFORMATION *info;
    ULONG length;

    status = IoOpenDeviceRegistryKey(device_object, PLUGPLAY_REGKEY_DEVICE, KEY_READ, &key);
    if (NT_SUCCESS(status))
    {
        RtlInitUnicodeString(&name, (WCHAR*)(&data_buffer[name_offset]));
        length = sizeof(KEY_VALUE_FULL_INFORMATION) + name.MaximumLength + data_length;
        info = ExAllocatePool(NonPagedPool, length);
        if (info)
        {
            memset(info, 0, length);
            status = ZwQueryValueKey(key, &name, KeyValueFullInformation, info, length, &length);
            if (NT_SUCCESS(status))
            {
                data_length = (info->DataLength > data_length) ? data_length : info->DataLength;
                memcpy(data_buffer, (((char *)info) + info->DataOffset),data_length);
                *actual_length=data_length;
            }
            ExFreePool(info);
        }
        ZwClose(key);
    }
    return status;
}

VOID set_filter_interface_key(libusb_device_t *dev, ULONG id)
{
	if (dev->device_interface_in_use)
	{
		HANDLE hKey=NULL;
		if (NT_SUCCESS(IoOpenDeviceInterfaceRegistryKey(&dev->device_interface_name,KEY_ALL_ACCESS,&hKey)))
		{
			UNICODE_STRING valueName;
			RtlInitUnicodeString(&valueName, L"LUsb0");

			if (NT_SUCCESS(ZwSetValueKey(hKey,&valueName, 0, REG_DWORD, &id,sizeof(ULONG))))
			{
				USBMSG("updated interface registry with LUsb0 direct-access symbolic link. id=%d\n",id);
			}
			else
			{
				USBERR0("IoOpenDeviceInterfaceRegistryKey:ZwSetValueKey failed\n");
			}
			ZwClose(hKey);
		}
		else
		{
			USBERR0("IoOpenDeviceInterfaceRegistryKey failed\n");
		}
	}
}