/* LIBUSB-WIN32, Generic Windows USB Driver
 * Copyright (C) 2002-2004 Stephan Meyer, <ste_meyer@web.de>
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



NTSTATUS set_descriptor(libusb_device_extension *device_extension,
                        void *buffer, int size, int type, 
                        int index, int language_id, int *sent, int timeout)
{
  NTSTATUS status = STATUS_SUCCESS;
  URB urb;

  debug_print_nl();
  debug_printf(LIBUSB_DEBUG_MSG, "set_descriptor(): buffer size %d", size);
  debug_printf(LIBUSB_DEBUG_MSG, "set_descriptor(): type %04d", type);
  debug_printf(LIBUSB_DEBUG_MSG, "set_descriptor(): index %04d", index);
  debug_printf(LIBUSB_DEBUG_MSG, "set_descriptor(): language id %04d",
               language_id);
  debug_printf(LIBUSB_DEBUG_MSG, "set_descriptor(): timeout %d", timeout);

  memset(&urb, 0, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST));

  urb.UrbHeader.Function =  URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE;
  urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);
  urb.UrbControlDescriptorRequest.TransferBufferLength = size;
  urb.UrbControlDescriptorRequest.TransferBuffer = buffer;
  urb.UrbControlDescriptorRequest.DescriptorType = (UCHAR)type;
  urb.UrbControlDescriptorRequest.Index = (UCHAR)index;
  urb.UrbControlDescriptorRequest.LanguageId = (USHORT)language_id;
	
  status = call_usbd(device_extension, &urb, 
                     IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
  
  if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_descriptor(): setting descriptor "
                   "failed: status: 0x%x, urb-status: 0x%x", 
                   status, urb.UrbHeader.Status);
    }
  else
    {
      *sent = urb.UrbControlDescriptorRequest.TransferBufferLength;
    }
  return status;
}
