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



NTSTATUS reset_endpoint(libusb_device_extension *device_extension,
			int endpoint, int timeout)
{
  NTSTATUS status = STATUS_SUCCESS;
  URB urb;

  debug_print_nl();
  debug_printf(DEBUG_MSG, "reset_endpoint(): endpoint %02xh", endpoint);
  debug_printf(DEBUG_MSG, "reset_endpoint(): timeout %d", timeout);

  if(!device_extension->current_configuration)
    {
      debug_printf(DEBUG_ERR, "reset_endpoint(): invalid configuration 0"); 
      return STATUS_INVALID_DEVICE_STATE;
    }
  
  if(!get_pipe_handle(device_extension, endpoint, 
		      &urb.UrbPipeRequest.PipeHandle))
    {
      debug_printf(DEBUG_ERR, "reset_endpoint(): getting endpoint pipe "
		   "failed");
      return STATUS_INVALID_PARAMETER;
    }
  
  urb.UrbHeader.Length = (USHORT) sizeof(struct _URB_PIPE_REQUEST);
  urb.UrbHeader.Function = URB_FUNCTION_RESET_PIPE;

  status = call_usbd(device_extension, (void *)&urb, 
		       IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
  
  if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      debug_printf(DEBUG_ERR, "reset_endpoint(): request failed");
    }

  return status;
}
