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


NTSTATUS get_configuration(libusb_device_extension *device_extension,
			   int *configuration, int timeout)
{
  NTSTATUS m_status = STATUS_SUCCESS;
  URB urb;
  char tmp;

  KdPrint(("LIBUSB_FILTER - get_configuration(): timeout %d\n", timeout));

  urb.UrbHeader.Function = URB_FUNCTION_GET_CONFIGURATION;
  urb.UrbHeader.Length = 
    sizeof(struct _URB_CONTROL_GET_CONFIGURATION_REQUEST);
  urb.UrbControlGetConfigurationRequest.TransferBufferLength = 1;
  urb.UrbControlGetConfigurationRequest.TransferBuffer = (void *)&tmp;
  urb.UrbControlGetConfigurationRequest.TransferBufferMDL = NULL; 
  urb.UrbControlGetConfigurationRequest.UrbLink = NULL; 

  m_status = call_usbd(device_extension, (void *)&urb, 
		       IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

  if(!NT_SUCCESS(m_status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      KdPrint(("LIBUSB_FILTER - get_configuration(): getting configuration "
	       "failed\n"));
      return STATUS_UNSUCCESSFUL;
    }
  *configuration = (int)tmp;

  return m_status;
}
