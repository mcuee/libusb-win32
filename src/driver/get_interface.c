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
                       bool_t use_index,
					   int interface_index, 
					   int interface_number, 
					   unsigned char *altsetting,
                       int *ret,
					   int timeout)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB urb;
	PUSB_INTERFACE_DESCRIPTOR interface_descriptor;

	if (use_index)
	{
		interface_descriptor = find_interface_desc_by_index(dev->config.descriptor, dev->config.total_size, interface_index, 0, NULL);
		if (!interface_descriptor)
		{
			return STATUS_NO_MORE_ENTRIES;
		}
		interface_number=interface_descriptor->bInterfaceNumber;
	}

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
        *ret = 0;
    }
    else
    {
        *ret = urb.UrbControlGetInterfaceRequest.TransferBufferLength;
        USBMSG("current altsetting is %d\n", *altsetting);
    }

    return status;
}
