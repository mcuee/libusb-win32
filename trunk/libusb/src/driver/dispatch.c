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

NTSTATUS DDKAPI dispatch(DEVICE_OBJECT *device_object, IRP *irp)
{
  libusb_device_extension *device_extension = 
    (libusb_device_extension *)device_object->DeviceExtension;

  if(device_extension->is_control_object)
    {
      return dispatch_control(device_object, irp);
    }

  switch(IoGetCurrentIrpStackLocation(irp)->MajorFunction) 
    {
    case IRP_MJ_PNP:
      return dispatch_pnp(device_extension, irp);
      
    case IRP_MJ_POWER:
      return dispatch_power(device_extension, irp);

    case IRP_MJ_DEVICE_CONTROL:
      return dispatch_ioctl(device_extension, irp);

    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
      return dispatch_internal_ioctl(device_extension, irp);

    default:
      IoSkipCurrentIrpStackLocation(irp);
      return IoCallDriver(device_extension->next_stack_device, irp);
    }
}


NTSTATUS dispatch_control(DEVICE_OBJECT *device_object, IRP *irp)
{
  libusb_device_extension *device_extension = 
    (libusb_device_extension *)device_object->DeviceExtension;
  libusb_device_extension *main_device_extension = NULL;

  switch(IoGetCurrentIrpStackLocation(irp)->MajorFunction)
    {
    case IRP_MJ_DEVICE_CONTROL:
      if(device_extension->main_device_object)
        return dispatch(device_extension->main_device_object, irp);
      else
        return complete_irp(irp, STATUS_DELETE_PENDING, 0);

    case IRP_MJ_CREATE:
      InterlockedIncrement(&device_extension->ref_count);
      return complete_irp(irp, STATUS_SUCCESS, 0);

    case IRP_MJ_CLOSE:
      if(!InterlockedDecrement(&device_extension->ref_count))
        {
          if(device_extension->main_device_object)
            {	      
              main_device_extension = (libusb_device_extension *)
                device_extension->main_device_object->DeviceExtension;
              release_all_interfaces(main_device_extension);
            }
          else
            {
              debug_printf(LIBUSB_DEBUG_MSG, "dispatch_control(): releasing "
                           "device id %d", device_extension->device_id);
              release_device_id(device_extension);
            }
        }
      return complete_irp(irp, STATUS_SUCCESS, 0);

    case IRP_MJ_CLEANUP:
      return complete_irp(irp, STATUS_SUCCESS, 0);

    default:
      return complete_irp(irp, STATUS_NOT_SUPPORTED, 0);
    }
}


