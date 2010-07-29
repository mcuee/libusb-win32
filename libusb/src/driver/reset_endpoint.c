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



NTSTATUS reset_endpoint(libusb_device_t *dev, int endpoint, int timeout)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB urb;

	USBMSG("endpoint: 0x%02x timeout: %d\n", endpoint, timeout);

	if (!dev->config.value)
    {
        USBERR0("invalid configuration 0\n");
        return STATUS_INVALID_DEVICE_STATE;
    }

    memset(&urb, 0, sizeof(struct _URB_PIPE_REQUEST));

    urb.UrbHeader.Length = (USHORT) sizeof(struct _URB_PIPE_REQUEST);
    urb.UrbHeader.Function = URB_FUNCTION_RESET_PIPE;

    if (!get_pipe_handle(dev, endpoint, &urb.UrbPipeRequest.PipeHandle))
    {
        USBERR0("getting endpoint pipe failed\n");
        return STATUS_INVALID_PARAMETER;
    }

    status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
        USBERR("request failed: status: 0x%x, urb-status: 0x%x\n", status, urb.UrbHeader.Status);
    }

    return status;
}
