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



NTSTATUS get_interface(libusb_device_extension *device_extension,
			    int interface, int *altsetting, int timeout)
{
  NTSTATUS m_status = STATUS_SUCCESS;
  URB urb;
  char tmp;

  KdPrint(("LIBUSB_FILTER - get_interface(): interface %d\n", interface));

  if(!device_extension->current_configuration)
    {
      KdPrint(("LIBUSB_FILTER - get_interface(): invalid configuration 0")); 
      return STATUS_UNSUCCESSFUL;
    }
    
  urb.UrbHeader.Function = URB_FUNCTION_GET_INTERFACE;
  urb.UrbHeader.Length = 
    sizeof(struct _URB_CONTROL_GET_INTERFACE_REQUEST);
  urb.UrbControlGetInterfaceRequest.TransferBufferLength = 1;
  urb.UrbControlGetInterfaceRequest.TransferBuffer = (void *)&tmp;
  urb.UrbControlGetInterfaceRequest.TransferBufferMDL = NULL;
  urb.UrbControlGetInterfaceRequest.UrbLink = NULL;
  urb.UrbControlGetInterfaceRequest.Interface = (USHORT)interface;
  
  m_status = call_usbd(device_extension, (void *)&urb, 
		       IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
  
  if(!NT_SUCCESS(m_status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      KdPrint(("LIBUSB_FILTER - get_interface(): getting interface failed "
	       "%x %x\n",
	       m_status, urb.UrbHeader.Status));
      return STATUS_UNSUCCESSFUL;
    }
  
  *altsetting = (int)tmp;
  return m_status;
}


