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



NTSTATUS set_feature(libusb_device_extension *device_extension,
		     int recipient, int index, int feature, int timeout)
{
  NTSTATUS status = STATUS_SUCCESS;
  URB urb;

  debug_print_nl();
  debug_printf(LIBUSB_DEBUG_MSG, "set_feature(): recipient %02d", recipient);
  debug_printf(LIBUSB_DEBUG_MSG, "set_feature(): index %04d", index);
  debug_printf(LIBUSB_DEBUG_MSG, "set_feature(): feature %04d", feature);
  debug_printf(LIBUSB_DEBUG_MSG, "set_feature(): timeout %d", timeout);

  if(!device_extension->current_configuration && recipient != USB_RECIP_DEVICE)
    {
      debug_printf(LIBUSB_DEBUG_MSG, "set_feature(): invalid "
		   "configuration 0"); 
      return STATUS_INVALID_PARAMETER;
    }

  switch(recipient)
    {
    case USB_RECIP_DEVICE:
      urb.UrbHeader.Function = URB_FUNCTION_SET_FEATURE_TO_DEVICE;
      break;
    case USB_RECIP_INTERFACE:
      urb.UrbHeader.Function = URB_FUNCTION_SET_FEATURE_TO_INTERFACE;
      break;
    case USB_RECIP_ENDPOINT:
      urb.UrbHeader.Function = URB_FUNCTION_SET_FEATURE_TO_ENDPOINT;
      break;
    case USB_RECIP_OTHER:
      urb.UrbHeader.Function = URB_FUNCTION_SET_FEATURE_TO_OTHER;
      urb.UrbControlFeatureRequest.Index = 0; 
      break;
    default:
      debug_printf(LIBUSB_DEBUG_ERR, "set_feature(): invalid recipient");
      return STATUS_INVALID_PARAMETER;
    }
  
  urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_FEATURE_REQUEST);
  urb.UrbControlFeatureRequest.FeatureSelector = (USHORT)feature;
  urb.UrbControlFeatureRequest.UrbLink = NULL; 
  urb.UrbControlFeatureRequest.Index = (USHORT)index; 
  
  status = call_usbd(device_extension, &urb, 
		     IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
  
  if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_feature(): setting feature failed: "
		   "status: 0x%x, urb-status: 0x%x", 
		   status, urb.UrbHeader.Status);
    }
  
  return status;
}
