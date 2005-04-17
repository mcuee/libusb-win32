/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
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

  switch(IoGetCurrentIrpStackLocation(irp)->MajorFunction) 
    {
    case IRP_MJ_PNP:
      return dispatch_pnp(device_extension, irp);
      
    case IRP_MJ_POWER:
      return dispatch_power(device_extension, irp);

    case IRP_MJ_DEVICE_CONTROL:
      if(is_irp_for_us(device_extension, irp))
        {
          return dispatch_ioctl(device_extension, irp);
        }
      break;

    case IRP_MJ_CREATE:
      if(is_irp_for_us(device_extension, irp))
        {
          InterlockedIncrement(&device_extension->ref_count);
          return complete_irp(irp, STATUS_SUCCESS, 0);
        }
      break;

    case IRP_MJ_CLOSE:
      if(is_irp_for_us(device_extension, irp))
        {
          if(!InterlockedDecrement(&device_extension->ref_count))
            {
              release_all_interfaces(device_extension);
            }
          return complete_irp(irp, STATUS_SUCCESS, 0);
        }
      break;

    case IRP_MJ_CLEANUP:

      if(is_irp_for_us(device_extension, irp))
        {
          return complete_irp(irp, STATUS_SUCCESS, 0);
        }
      break;

    default:
      if(is_irp_for_us(device_extension, irp))
        {
          return complete_irp(irp, STATUS_NOT_SUPPORTED, 0);
        }
    }

  return pass_irp_down(device_extension, irp);
}

