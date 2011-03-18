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



NTSTATUS get_interface(libusb_device_t *dev,
                       int interface, unsigned char *altsetting,
                       int *ret, int timeout)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB urb;

	USBMSG("interface: %d timeout: %d\n", interface, timeout);

    if (!dev->config.value)
    {
        USBERR0("invalid configuration 0\n");
        return STATUS_INVALID_DEVICE_STATE;
    }

    memset(&urb, 0, sizeof(URB));

    urb.UrbHeader.Function = URB_FUNCTION_GET_INTERFACE;
    urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_GET_INTERFACE_REQUEST);
    urb.UrbControlGetInterfaceRequest.TransferBufferLength = 1;
    urb.UrbControlGetInterfaceRequest.TransferBuffer = altsetting;
    urb.UrbControlGetInterfaceRequest.Interface = (USHORT)interface;

    status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
        USBERR("getting interface failed: status: 0x%x, urb-status: 0x%x\n",
                    status, urb.UrbHeader.Status);
        *ret = 0;
    }
    else
    {
        *ret = urb.UrbControlGetInterfaceRequest.TransferBufferLength;
        USBMSG("current altsetting is %d\n", *altsetting);
    }

    return status;
}

NTSTATUS interface_query_settings(libusb_device_t *dev,
								  int interface_index, 
								  int alt_index, 
								  PUSB_INTERFACE_DESCRIPTOR interface_descriptor)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB *urb;
    int i, config_size, config_index, tmp_size;

    PUSB_CONFIGURATION_DESCRIPTOR configuration_descriptor = NULL;
    PUSB_INTERFACE_DESCRIPTOR interface_descriptor_src = NULL;

    USBMSG("query interface index=%d alt-index=%d\n", 
		interface_index, alt_index);

	if (!dev->config.value)
    {
        USBERR0("device is not configured\n");
        return STATUS_INVALID_DEVICE_STATE;
    }

    configuration_descriptor = get_config_descriptor(dev, dev->config.value,
                               &config_size, &config_index);
    if (!configuration_descriptor)
    {
        USBERR0("memory_allocation error\n");
        return STATUS_NO_MEMORY;
    }

    interface_descriptor_src =
        find_interface_desc_by_index(configuration_descriptor, config_size,
                            interface_index, alt_index);

	if (!interface_descriptor_src)
	{
		status = STATUS_NO_MORE_ENTRIES;
	}
	else
	{
		RtlCopyMemory(interface_descriptor, interface_descriptor_src, sizeof(*interface_descriptor));
	}

    ExFreePool(configuration_descriptor);

	return status;

}


