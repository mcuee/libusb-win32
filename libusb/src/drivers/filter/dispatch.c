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

NTSTATUS __stdcall dispatch(DEVICE_OBJECT *device_object, IRP *irp)
{
  NTSTATUS status = STATUS_SUCCESS;
  libusb_device_extension *device_extension = 
    (libusb_device_extension *)device_object->DeviceExtension;


  if(!device_extension->control_device_object)
    {
      return dispatch_control(device_object, irp);
    }

  switch(IoGetCurrentIrpStackLocation(irp)->MajorFunction) 
    {
    case IRP_MJ_PNP:
      status = dispatch_pnp(device_extension, irp);
      break;
      
    case IRP_MJ_POWER:
      PoStartNextPowerIrp(irp);
      IoSkipCurrentIrpStackLocation(irp);
      return PoCallDriver(device_extension->next_stack_device, irp);
      
    case IRP_MJ_CREATE:      
    case IRP_MJ_CLOSE:
      status = complete_irp(irp, STATUS_SUCCESS,0);
      break;
      
    case IRP_MJ_DEVICE_CONTROL:
      status = dispatch_ioctl(device_extension, irp);
      break;
    
    default:
      IoSkipCurrentIrpStackLocation(irp);
      status = IoCallDriver(device_extension->next_stack_device, irp);
    }

  return status;
}


NTSTATUS dispatch_control(DEVICE_OBJECT *device_object, IRP *irp)
{
  NTSTATUS status = STATUS_SUCCESS;
  libusb_device_extension *device_extension = 
    (libusb_device_extension *)device_object->DeviceExtension;

  if(IoGetCurrentIrpStackLocation(irp)->MajorFunction 
     == IRP_MJ_DEVICE_CONTROL) 
    {
      if(device_extension->main_device_object)
	status = dispatch(device_extension->main_device_object, irp);
      else
	status = complete_irp(irp, STATUS_DELETE_PENDING, 0);
    }
  else 
    {
      status = complete_irp(irp, status, 0);
    }
  return status;
}


