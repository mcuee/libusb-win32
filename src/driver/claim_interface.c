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



NTSTATUS claim_interface(libusb_device_t *dev, FILE_OBJECT *file_object,
                         int interface)
{
    USBMSG("interface %d\n", interface);

    if (!dev->config.value)
    {
        USBERR0("device is not configured\n");
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (interface >= LIBUSB_MAX_NUMBER_OF_INTERFACES)
    {
        USBERR("interface number %d too high\n",
                    interface);
        return STATUS_INVALID_PARAMETER;
    }

    if (!dev->config.interfaces[interface].valid)
    {
        USBERR("interface %d does not exist\n", interface);
        return STATUS_INVALID_PARAMETER;
    }

    if (dev->config.interfaces[interface].file_object == file_object)
    {
        return STATUS_SUCCESS;
    }

    if (dev->config.interfaces[interface].file_object)
    {
        USBERR("could not claim interface %d, interface is already claimed\n", interface);
        return STATUS_DEVICE_BUSY;
    }

    dev->config.interfaces[interface].file_object = file_object;

    return STATUS_SUCCESS;
}

NTSTATUS claim_interface_ex(libusb_device_t *dev, 
							FILE_OBJECT *file_object, 
							interface_request_t* interface_request)
{
	PUSB_INTERFACE_DESCRIPTOR interface_descriptor;

	USBMSG("interface-%s=%d\n",
		interface_request->intf_use_index ? "index" : "number",
		interface_request->intf_use_index ? interface_request->interface_index : interface_request->interface_number);

    if (!dev->config.value || !dev->config.descriptor)
    {
        USBERR0("device is not configured\n");
        return STATUS_INVALID_DEVICE_STATE;
    }

	interface_request->altsetting_index=FIND_INTERFACE_INDEX_ANY;
	interface_descriptor = find_interface_desc_ex(dev->config.descriptor,dev->config.total_size,interface_request,NULL);
	if (!interface_descriptor)
	{
        return STATUS_NO_MORE_ENTRIES;
	}

	return  claim_interface(dev, file_object, interface_descriptor->bInterfaceNumber);
}
