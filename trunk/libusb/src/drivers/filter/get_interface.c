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


#include "libusb_filter.h"



NTSTATUS get_interface(libusb_device_extension *device_extension,
		       int interface, char *altsetting, int timeout)
{
  NTSTATUS status = STATUS_SUCCESS;
  URB urb;
  char tmp;

  debug_print_nl();
  debug_printf(LIBUSB_DEBUG_MSG, "get_interface(): interface %d\n", interface);
  debug_printf(LIBUSB_DEBUG_MSG, "get_interface(): timeout %d", timeout);

  if(!device_extension->current_configuration)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "get_interface(): invalid "
		   "configuration 0"); 
      return STATUS_INVALID_DEVICE_STATE;
    }
    
  urb.UrbHeader.Function = URB_FUNCTION_GET_INTERFACE;
  urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_GET_INTERFACE_REQUEST);
  urb.UrbControlGetInterfaceRequest.TransferBufferLength = 1;
  urb.UrbControlGetInterfaceRequest.TransferBuffer = altsetting;
  urb.UrbControlGetInterfaceRequest.TransferBufferMDL = NULL;
  urb.UrbControlGetInterfaceRequest.UrbLink = NULL;
  urb.UrbControlGetInterfaceRequest.Interface = (USHORT)interface;
  
  status = call_usbd(device_extension, &urb, 
		     IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
  
  if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "get_interface(): getting interface "
		   "failed: status: 0x%x, urb-status: 0x%x", 
		   status, urb.UrbHeader.Status);
    }

  debug_printf(LIBUSB_DEBUG_MSG, "get_interface(): current altsetting is %d",
	       *altsetting); 

  return status;
}


