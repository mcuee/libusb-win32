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



NTSTATUS get_status(libusb_device_extension *device_extension, int recipient,
		    int index, int *status, int timeout)
{
  NTSTATUS m_status = STATUS_SUCCESS;
  URB urb;
  short tmp;

  KdPrint(("LIBUSB_FILTER - get_status(): recipient %02d\n", recipient));
  KdPrint(("LIBUSB_FILTER - get_status(): index %04d\n", index));
  KdPrint(("LIBUSB_FILTER - get_status(): timeout %d\n", timeout));

  if(!device_extension->current_configuration 
     && (recipient != USB_RECIP_DEVICE))
    {
      KdPrint(("LIBUSB_FILTER - get_status(): invalid configuration 0")); 
      return STATUS_UNSUCCESSFUL;
    }

  switch(recipient)
    {
    case USB_RECIP_DEVICE:
      urb.UrbHeader.Function = URB_FUNCTION_GET_STATUS_FROM_DEVICE;
      break;
    case USB_RECIP_INTERFACE:
      urb.UrbHeader.Function = URB_FUNCTION_GET_STATUS_FROM_INTERFACE;
      break;
    case USB_RECIP_ENDPOINT:
      urb.UrbHeader.Function = URB_FUNCTION_GET_STATUS_FROM_ENDPOINT;
      break;
    case USB_RECIP_OTHER:
      urb.UrbHeader.Function = URB_FUNCTION_GET_STATUS_FROM_OTHER;
      break;
    default:
      KdPrint(("LIBUSB_FILTER - get_status(): invalid recipient\n"));
      return STATUS_UNSUCCESSFUL;
    }

  urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_GET_STATUS_REQUEST);
  urb.UrbControlGetStatusRequest.TransferBufferLength = 2;
  urb.UrbControlGetStatusRequest.TransferBuffer = &tmp; 
  urb.UrbControlGetStatusRequest.TransferBufferMDL = NULL;
  urb.UrbControlGetStatusRequest.UrbLink = NULL;
  urb.UrbControlGetStatusRequest.Index = (USHORT)index; 
	
  m_status = call_usbd(device_extension, &urb, 
		       IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
      
  if(!NT_SUCCESS(m_status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      KdPrint(("LIBUSB_FILTER - get_status(): getting status failed %x %x\n",
	       m_status, urb.UrbHeader.Status));
      return STATUS_UNSUCCESSFUL;
    }
  *status = (int)tmp;

  return m_status;
}

