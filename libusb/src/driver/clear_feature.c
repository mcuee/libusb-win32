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

NTSTATUS clear_feature(libusb_device_t *dev,
                       int recipient, int index, int feature, int timeout)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB urb;

	USBMSG("recipient: %02d index: %04d feature: %04d timeout: %d\n", 
		recipient,index,feature,timeout);

    memset(&urb, 0, sizeof(struct _URB_CONTROL_FEATURE_REQUEST));

    switch (recipient)
    {
    case USB_RECIP_DEVICE:
        urb.UrbHeader.Function = URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE;
        break;
    case USB_RECIP_INTERFACE:
        urb.UrbHeader.Function = URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE;
        break;
    case USB_RECIP_ENDPOINT:
        urb.UrbHeader.Function = URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT;
        break;
    case USB_RECIP_OTHER:
        urb.UrbHeader.Function = URB_FUNCTION_CLEAR_FEATURE_TO_OTHER;
        break;
    default:
        USBERR0("invalid recipient\n");
        return STATUS_INVALID_PARAMETER;
    }

    urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_FEATURE_REQUEST);
    urb.UrbControlFeatureRequest.FeatureSelector = (USHORT)feature;
    urb.UrbControlFeatureRequest.Index = (USHORT)index;

    status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
        USBERR("clearing feature failed: status: 0x%x, urb-status: 0x%x\n", status, urb.UrbHeader.Status);
    }

    return status;
}
