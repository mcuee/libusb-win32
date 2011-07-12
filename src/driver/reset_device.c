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


NTSTATUS reset_device(libusb_device_t *dev, int timeout)
{
    return reset_device_ex(dev, timeout, USB_RESET_TYPE_FULL_RESET);
}

NTSTATUS reset_device_ex(libusb_device_t *dev, int timeout, unsigned int reset_type)
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    USBMSG0("resetting device\n");

    if (reset_type & USB_RESET_TYPE_RESET_PORT)
    {
        status = call_usbd(dev, NULL, IOCTL_INTERNAL_USB_RESET_PORT, timeout);

        if (!NT_SUCCESS(status))
        {
            USBERR("IOCTL_INTERNAL_USB_RESET_PORT failed: status: 0x%x\n", status);
        }
    }

    if (reset_type & USB_RESET_TYPE_CYCLE_PORT)
    {
        status = call_usbd(dev, NULL, IOCTL_INTERNAL_USB_CYCLE_PORT, timeout);

        if (!NT_SUCCESS(status))
        {
            USBERR("IOCTL_INTERNAL_USB_CYCLE_PORT failed: status: 0x%x\n", status);
        }
    }

    UpdateContextConfigDescriptor(dev, NULL, 0, 0, -1);
    return status;
}
