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


  if(device_extension->is_control_object)
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
            
    case IRP_MJ_DEVICE_CONTROL:
      status = dispatch_ioctl(device_extension, irp);
      break;

    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
      if(IoGetCurrentIrpStackLocation(irp)
	 ->Parameters.DeviceIoControl.IoControlCode 
	 == IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
	  IoCopyCurrentIrpStackLocationToNext(irp);
	  IoSetCompletionRoutine(irp, on_internal_ioctl_complete, NULL, 
				 TRUE, TRUE, TRUE);
	  return IoCallDriver(device_extension->next_stack_device, irp);
	}

    default:
      IoSkipCurrentIrpStackLocation(irp);
      status = IoCallDriver(device_extension->next_stack_device, irp);
    }

  return status;
}


NTSTATUS dispatch_control(DEVICE_OBJECT *device_object, IRP *irp)
{
  NTSTATUS status = STATUS_NOT_SUPPORTED;
  libusb_device_extension *device_extension = 
    (libusb_device_extension *)device_object->DeviceExtension;

  switch(IoGetCurrentIrpStackLocation(irp)->MajorFunction)
    {
    case IRP_MJ_DEVICE_CONTROL:
      if(device_extension->main_device_object)
	{
	  return dispatch(device_extension->main_device_object, irp);
	}
      else
	{
	  return complete_irp(irp, STATUS_DELETE_PENDING, 0);
	}
      break;

    case IRP_MJ_CREATE:
      debug_printf(DEBUG_MSG, "dispatch_control(): device %d: CREATE",
		   device_extension->device_id);
      InterlockedIncrement(&device_extension->ref_count);
      status = STATUS_SUCCESS;
      break;
    case IRP_MJ_CLOSE:
      debug_printf(DEBUG_MSG, "dispatch_control(): device %d: CLOSE",
		   device_extension->device_id);
      if(!InterlockedDecrement(&device_extension->ref_count))
	{
	  if(!device_extension->main_device_object)
	    {
	      debug_printf(DEBUG_MSG, "dispatch_control(): releasing device "
		       "id %d", device_extension->device_id);
	      release_device_id(device_extension);
	    }
	}
      status = STATUS_SUCCESS;
      break;
    case IRP_MJ_CLEANUP:
      debug_printf(DEBUG_MSG, "dispatch_control(): device %d: CLEANUP",
		   device_extension->device_id);
      status = STATUS_SUCCESS;
      break;

    default:
      debug_printf(DEBUG_MSG, "dispatch_control(): device %d: other IRP",
		   device_extension->device_id);
    }

  return complete_irp(irp, status, 0);
}


