#/* LIBUSB-WIN32, Generic Windows USB Driver
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


NTSTATUS dispatch_pnp(libusb_device_extension *device_extension, IRP *irp)
{
  NTSTATUS status = STATUS_SUCCESS;
  
  status = remove_lock_acquire(&device_extension->remove_lock);
  

  switch(IoGetCurrentIrpStackLocation(irp)->MinorFunction) 
    {     

    case IRP_MN_REMOVE_DEVICE:
      KdPrint(("LIBUSB_FILTER - dispatch_pnp(): IRP_MN_REMOVE_DEVICE\n"));
      remove_lock_release_and_wait(&device_extension->remove_lock);

      irp->IoStatus.Status = STATUS_SUCCESS;
      IoSkipCurrentIrpStackLocation(irp);
      status = IoCallDriver(device_extension->next_stack_device, irp);

      if(!NT_SUCCESS(status))
	{ 
	  KdPrint(("LIBUSB_FILTER - dispatch_pnp(): calling lower "
		   "driver failed\n"));
	  remove_lock_release(&device_extension->remove_lock);
	  return status;
	}

      control_object_delete(device_extension);
      IoDetachDevice(device_extension->next_stack_device);
      IoDeleteDevice(device_extension->self);
      return status;

    case IRP_MN_SURPRISE_REMOVAL:
      KdPrint(("LIBUSB_FILTER - dispatch_pnp(): IRP_MN_SURPRISE_REMOVAL\n"));
      irp->IoStatus.Status = STATUS_SUCCESS;
      break;

    case IRP_MN_START_DEVICE:
      KdPrint(("LIBUSB_FILTER - dispatch_pnp(): IRP_MN_START_DEVICE\n"));
      break;
    case IRP_MN_CANCEL_REMOVE_DEVICE:
      KdPrint(("LIBUSB_FILTER - dispatch_pnp(): IRP_MN_CANCEL_REMOVE_DEVICE\n"));
      break;
    case IRP_MN_CANCEL_STOP_DEVICE:
      KdPrint(("LIBUSB_FILTER - dispatch_pnp(): IRP_MN_CANCEL_STOP_DEVICE\n"));
      break;
    case IRP_MN_QUERY_STOP_DEVICE:
      KdPrint(("LIBUSB_FILTER - dispatch_pnp(): IRP_MN_QUERY_STOP_DEVICE\n"));
      break;
    case IRP_MN_STOP_DEVICE:
      KdPrint(("LIBUSB_FILTER - dispatch_pnp(): IRP_MN_STOP_DEVICE\n"));
      break;
    case IRP_MN_QUERY_REMOVE_DEVICE:
      KdPrint(("LIBUSB_FILTER - dispatch_pnp(): IRP_MN_QUERY_REMOVE_DEVICE\n"));
      break;
    default:
      ;
    }
  
  IoSkipCurrentIrpStackLocation(irp);
  status = IoCallDriver(device_extension->next_stack_device, irp);
  remove_lock_release(&device_extension->remove_lock);

  return status;
}

