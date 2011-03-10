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


NTSTATUS vendor_class_request(libusb_device_t *dev,
                              int type, int recipient,
                              int request, int value, int index,
                              void *buffer, int size, int direction,
                              int *ret, int timeout)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB urb;

    *ret = 0;

    memset(&urb, 0, sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST));

    switch (type)
    {
    case USB_TYPE_CLASS:
        USBMSG0("type: class\n");
        switch (recipient)
        {
        case USB_RECIP_DEVICE:
            USBMSG0("recipient: device\n");
            urb.UrbHeader.Function = URB_FUNCTION_CLASS_DEVICE;
            break;
        case USB_RECIP_INTERFACE:
            USBMSG0("recipient: interface\n");
            urb.UrbHeader.Function = URB_FUNCTION_CLASS_INTERFACE;
            break;
        case USB_RECIP_ENDPOINT:
            USBMSG0("recipient: endpoint\n");
            urb.UrbHeader.Function = URB_FUNCTION_CLASS_ENDPOINT;
            break;
        case USB_RECIP_OTHER:
            USBMSG0("recipient: other\n");
            urb.UrbHeader.Function = URB_FUNCTION_CLASS_OTHER;
            break;
        default:
            USBERR0("invalid recipient\n");
            return STATUS_INVALID_PARAMETER;
        }
        break;
    case USB_TYPE_VENDOR:
        USBMSG0("type: vendor\n");
        switch (recipient)
        {
        case USB_RECIP_DEVICE:
            USBMSG0("recipient: device\n");
            urb.UrbHeader.Function = URB_FUNCTION_VENDOR_DEVICE;
            break;
        case USB_RECIP_INTERFACE:
            USBMSG0("recipient: interface\n");
            urb.UrbHeader.Function = URB_FUNCTION_VENDOR_INTERFACE;
            break;
        case USB_RECIP_ENDPOINT:
            USBMSG0("recipient: endpoint\n");
            urb.UrbHeader.Function = URB_FUNCTION_VENDOR_ENDPOINT;
            break;
        case USB_RECIP_OTHER:
            USBMSG0("recipient: other\n");
            urb.UrbHeader.Function = URB_FUNCTION_VENDOR_OTHER;
            break;
        default:
			// [Kevin Timmerman Patch]
			USBMSG("recipient: reserved (0x%02x)\n", recipient);
			urb.UrbHeader.Function = URB_FUNCTION_VENDOR_DEVICE;
			urb.UrbControlVendorClassRequest.RequestTypeReservedBits = (UCHAR)recipient;
			break;
			/*
            USBERR0("invalid recipient\n");
            return STATUS_INVALID_PARAMETER;
			*/
        }
        break;
    default:
        USBERR0("invalid type\n");
        return STATUS_INVALID_PARAMETER;
    }

    USBMSG("request: 0x%02x\n", request);
    USBMSG("value: 0x%04x\n", value);
    USBMSG("index: 0x%04x\n", index);
    USBMSG("size: %d\n", size);

    if (direction == USBD_TRANSFER_DIRECTION_IN)
    {
        USBMSG0("direction: in\n");
    }
    else
    {
        USBMSG0("direction: out\n");
    }

    USBMSG("timeout: %d\n", timeout);

    urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST);
    urb.UrbControlVendorClassRequest.TransferFlags
    = direction | USBD_SHORT_TRANSFER_OK ;
    urb.UrbControlVendorClassRequest.TransferBufferLength = size;
    urb.UrbControlVendorClassRequest.TransferBufferMDL = NULL;
    urb.UrbControlVendorClassRequest.TransferBuffer = buffer;
    urb.UrbControlVendorClassRequest.Request = (UCHAR)request;
    urb.UrbControlVendorClassRequest.Value = (USHORT)value;
    urb.UrbControlVendorClassRequest.Index = (USHORT)index;

	// no maximum timeout check for vendor request.
    status = call_usbd_ex(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout, 0);

    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
        USBERR("request failed: status: 0x%x, urb-status: 0x%x\n", status, urb.UrbHeader.Status);
    }
    else
    {
        if (direction == USBD_TRANSFER_DIRECTION_IN)
            *ret = urb.UrbControlVendorClassRequest.TransferBufferLength;
        USBMSG("%d bytes transmitted\n",
                      urb.UrbControlVendorClassRequest.TransferBufferLength);
    }

    return status;
}

