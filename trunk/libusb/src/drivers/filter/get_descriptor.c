/* LIBUSB-WIN32, Generic Windows USB Driver
 * Copyright (C) 2002-2003 Stephan Meyer, <ste_meyer@web.de>
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


#include "libusb_filter.h"



NTSTATUS get_descriptor(libusb_device_extension *device_extension,
			void *buffer, int size, int type, 
			int index, int language_id, int *sent, int timeout)
{
  NTSTATUS status = STATUS_SUCCESS;
  URB urb;

  debug_print_nl();
  debug_printf(LIBUSB_DEBUG_MSG, "get_descriptor(): buffer size %d", size);
  debug_printf(LIBUSB_DEBUG_MSG, "get_descriptor(): type %04d", type);
  debug_printf(LIBUSB_DEBUG_MSG, "get_descriptor(): index %04d", index);
  debug_printf(LIBUSB_DEBUG_MSG, "get_descriptor(): language id %04d",
	       language_id);
  debug_printf(LIBUSB_DEBUG_MSG, "get_descriptor(): timeout %d", timeout);
  

  UsbBuildGetDescriptorRequest(&urb, 
			       sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
			       (UCHAR)type, (UCHAR)index, (USHORT)language_id,
			       buffer, NULL, size, NULL);
  
  status = call_usbd(device_extension, &urb,
		     IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

      
  if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "get_descriptor(): getting descriptor "
		   "failed: status: 0x%x, urb-status: 0x%x", 
		   status, urb.UrbHeader.Status);
      *sent = 0;
    }
  else
    {
      *sent = urb.UrbControlDescriptorRequest.TransferBufferLength;
    }
  return status;
}
