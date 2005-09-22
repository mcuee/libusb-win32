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


typedef struct {
  KEVENT event;
  NTSTATUS status;
} power_context_t;

NTSTATUS dispatch_power(libusb_device_t *dev, IRP *irp)
{
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
  power_context_t *c = (power_context_t *)context;

	KeSetEvent(&c->event, EVENT_INCREMENT, FALSE);
  c->status = io_status->Status;
}


void power_set_device_state(libusb_device_t *dev, 
                            DEVICE_POWER_STATE device_state)
{
  power_context_t context;
  POWER_STATE power_state;

  power_state.DeviceState = device_state;

  KeInitializeEvent(&context.event, NotificationEvent, FALSE);

  context.status = PoRequestPowerIrp(dev->physical_device_object, 
                                     IRP_MN_SET_POWER, 
                                     power_state,
                                     power_set_state_complete, 
                                     &context, NULL);

  if(context.status == STATUS_PENDING)
    {
			KeWaitForSingleObject(&context.event, Executive, KernelMode, 
                            FALSE, NULL);
    }
}			

