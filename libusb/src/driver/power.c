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


NTSTATUS dispatch_power(libusb_device_t *dev, IRP *irp)
{
  IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);
  DEVICE_POWER_STATE device_power;
  SYSTEM_POWER_STATE system_power;
  
  switch(stack_location->MinorFunction) 
    {     
    case IRP_MN_QUERY_POWER:
      DEBUG_MESSAGE("dispatch_power(): IRP_MN_QUERY_POWER");
      break;

    case IRP_MN_SET_POWER:
      system_power = stack_location->Parameters.Power.State.SystemState
        - PowerSystemWorking;
      device_power = stack_location->Parameters.Power.State.DeviceState
        - PowerDeviceD0;

      if(stack_location->Parameters.Power.Type == SystemPowerState)
        {
          DEBUG_MESSAGE("dispatch_power(): IRP_MN_SET_POWER: S%d",
                        system_power);
        }
      else
        {
          DEBUG_MESSAGE("dispatch_power(): IRP_MN_SET_POWER: D%d", 
                        device_power);
        }

      break;

    case IRP_MN_WAIT_WAKE:
      DEBUG_MESSAGE("dispatch_power(): IRP_MN_WAIT_WAKE");
      break;

    case IRP_MN_POWER_SEQUENCE:
      DEBUG_MESSAGE("dispatch_power(): IRP_MN_POWER_SEQUENCE");
      break;
    }

  PoStartNextPowerIrp(irp);
  IoSkipCurrentIrpStackLocation(irp);

  return PoCallDriver(dev->next_stack_device, irp);
}

void DDKAPI power_set_state_complete(DEVICE_OBJECT *device_object,
                                     UCHAR minor_function,
                                     POWER_STATE power_state,
                                     void *context,
                                     IO_STATUS_BLOCK *io_status)
{
	KeSetEvent((KEVENT *)context, EVENT_INCREMENT, FALSE);
}


void power_set_device_state(libusb_device_t *dev, 
                            DEVICE_POWER_STATE device_state)
{
  NTSTATUS status;
  KEVENT event;
  POWER_STATE power_state;

  power_state.DeviceState = device_state;

  KeInitializeEvent(&event, NotificationEvent, FALSE);

  status = PoRequestPowerIrp(dev->physical_device_object, 
                             IRP_MN_SET_POWER, 
                             power_state,
                             power_set_state_complete, 
                             &event, NULL);

  if(status == STATUS_PENDING)
    {
      KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
}			

