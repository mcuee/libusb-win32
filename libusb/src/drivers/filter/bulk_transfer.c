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



NTSTATUS bulk_transfer(libusb_device_extension *device_extension,
		       int endpoint, MDL *buffer, 
		       int size, int direction, int *sent, int timeout)
{
  NTSTATUS m_status = STATUS_SUCCESS;
  USBD_PIPE_HANDLE pipe_handle = NULL;
  URB urb;

  KdPrint(("LIBUSB_FILTER - bulk_transfer(): endpoint %02xh\n", endpoint));
  KdPrint(("LIBUSB_FILTER - bulk_transfer(): size %d\n", size));
  
  if(!device_extension->current_configuration)
    {
      KdPrint(("LIBUSB_FILTER - bulk_transfer(): invalid configuration 0")); 
      return STATUS_UNSUCCESSFUL;
    }

  if(direction == USBD_TRANSFER_DIRECTION_IN)
    KdPrint(("LIBUSB_FILTER - bulk_transfer(): direction in\n"));
  else
    KdPrint(("LIBUSB_FILTER - bulk_transfer(): direction out\n"));
  
  KdPrint(("LIBUSB_FILTER - bulk_transfer(): timeout %d\n", timeout));

  if(!get_pipe_handle(device_extension, endpoint, &pipe_handle))
    {
      KdPrint(("LIBUSB_FILTER - bulk_transfer(): getting endpoint pipe "
	       "failed\n"));
      return STATUS_UNSUCCESSFUL;
    }
      

  UsbBuildInterruptOrBulkTransferRequest
    (&urb, sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
     pipe_handle, NULL, buffer, size, 
     direction | USBD_SHORT_TRANSFER_OK, NULL);

  m_status = call_usbd(device_extension, (void *)&urb, 
		       IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
    
  if(!NT_SUCCESS(m_status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      KdPrint(("LIBUSB_FILTER - bulk_transfer(): transfer failed\n"));
      return STATUS_UNSUCCESSFUL;
    }
  
  *sent = urb.UrbBulkOrInterruptTransfer.TransferBufferLength;
  
  return m_status;
}
