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
					   int interface_number, 
					   unsigned char *altsetting,
					   int timeout)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB urb;
	PUSB_INTERFACE_DESCRIPTOR interface_descriptor;

	USBMSG("interface: %d timeout: %d\n", interface_number, timeout);

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
    urb.UrbControlGetInterfaceRequest.Interface = (USHORT)interface_number;

    status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
        USBERR("getting interface failed: status: 0x%x, urb-status: 0x%x\n",
                    status, urb.UrbHeader.Status);
    }
    else
    {
        USBMSG("current altsetting is %d\n", *altsetting);
    }

    return status;
}

NTSTATUS get_interface_ex(libusb_device_t *dev, 
					   interface_request_t* interface_request, 
                       int timeout)
{
    unsigned char alt_number;
	NTSTATUS status = STATUS_SUCCESS;

    USB_INTERFACE_DESCRIPTOR *interface_descriptor = NULL;

	USBMSG("interface-%s=%d timeout=%d\n",
		interface_request->intf_use_index ? "index" : "number",
		interface_request->intf_use_index ? interface_request->interface_index : interface_request->interface_number,
		timeout);

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

	status = get_interface(dev,interface_descriptor->bInterfaceNumber, &alt_number, timeout);
	if (!NT_SUCCESS(status))
	{
        return STATUS_NO_MORE_ENTRIES;
	}

	interface_request->interface_number=interface_descriptor->bInterfaceNumber;
	interface_request->altsetting_number=alt_number;
	interface_request->altf_use_index=0;
	interface_request->intf_use_index=0;

	interface_descriptor = find_interface_desc_ex(dev->config.descriptor,dev->config.total_size,interface_request,NULL);
	if (!interface_descriptor)
	{
        return STATUS_NO_MORE_ENTRIES;
	}

	return status;
}
