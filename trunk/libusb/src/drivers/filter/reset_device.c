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



NTSTATUS reset_device(libusb_device_extension *device_extension, int timeout)
{
  NTSTATUS status;
  
  debug_print_nl();
  debug_printf(LIBUSB_DEBUG_MSG, "reset_device()");
  debug_printf(LIBUSB_DEBUG_MSG, "reset_device(): timeout %d", timeout);

  status = call_usbd(device_extension, NULL, 
		     IOCTL_INTERNAL_USB_RESET_PORT, timeout);
  
  if(!NT_SUCCESS(status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "reset_device(): request failed: "
		   "status: 0x%x", status);
    }
  return status;
}
