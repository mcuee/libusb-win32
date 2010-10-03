/* libusb-win32, Generic Windows USB Library
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


static NTSTATUS DDKAPI
on_power_state_complete(DEVICE_OBJECT *device_object,
                        IRP           *irp,
                        void          *context);

static NTSTATUS DDKAPI
on_filter_power_state_complete(DEVICE_OBJECT *device_object,
                               IRP           *irp,
                               void          *context);

static void DDKAPI
on_power_set_device_state_complete(DEVICE_OBJECT *device_object,
                                   UCHAR minor_function,
                                   POWER_STATE power_state,
                                   void *context,
                                   IO_STATUS_BLOCK *io_status);

/* [trobinso MOD 4/16/2010]
 * If running as a filter, do not act as power policy owner.
 */
NTSTATUS dispatch_power(libusb_device_t *dev, IRP *irp)
{
    IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);
    POWER_STATE power_state;
    NTSTATUS status;

    status = remove_lock_acquire(dev);

    if (!NT_SUCCESS(status))
    {
        irp->IoStatus.Status = status;
        PoStartNextPowerIrp(irp);
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return status;
    }

    if (stack_location->MinorFunction == IRP_MN_SET_POWER)
    {
        power_state = stack_location->Parameters.Power.State;

        if (stack_location->Parameters.Power.Type == SystemPowerState)
        {
            USBMSG("IRP_MN_SET_POWER: S%d %s\n",
				power_state.SystemState - PowerSystemWorking, dev->device_id);
        }
        else
        {
            USBMSG("IRP_MN_SET_POWER: D%d %s\n",
				power_state.DeviceState - PowerDeviceD0, dev->device_id);

            if (power_state.DeviceState > dev->power_state.DeviceState)
            {
                /* device is powered down, report device state to the */
                /* Power Manager before sending the IRP down */
                /* (power up is handled by the completion routine) */
                PoSetPowerState(dev->self, DevicePowerState, power_state);
            }
        }

        /* TODO: should PoStartNextPowerIrp() be called here or from the */
        /* completion routine? */
        PoStartNextPowerIrp(irp);

        IoCopyCurrentIrpStackLocationToNext(irp);
        if (!dev->is_filter && !dev->disallow_power_control)
        {
            IoSetCompletionRoutine(irp,
                                   on_power_state_complete,
                                   dev,
                                   TRUE, /* on success */
                                   TRUE, /* on error   */
                                   TRUE);/* on cancel  */
        }
        else
        {
            IoSetCompletionRoutine(irp,
                                   on_filter_power_state_complete,
                                   dev,
                                   TRUE, /* on success */
                                   TRUE, /* on error   */
                                   TRUE);/* on cancel  */
        }

        return PoCallDriver(dev->next_stack_device, irp);
    }
    else
    {
		/* pass all other power IRPs down without setting a completion routine */
        PoStartNextPowerIrp(irp);
        IoSkipCurrentIrpStackLocation(irp);
        status = PoCallDriver(dev->next_stack_device, irp);
        remove_lock_release(dev);

        return status;
    }
}


static NTSTATUS DDKAPI
on_power_state_complete(DEVICE_OBJECT *device_object,
                        IRP           *irp,
                        void          *context)
{
    libusb_device_t *dev = context;
    IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);
    POWER_STATE power_state = stack_location->Parameters.Power.State;
    DEVICE_POWER_STATE dev_power_state;

    if (irp->PendingReturned)
    {
        IoMarkIrpPending(irp);
    }

    if (NT_SUCCESS(irp->IoStatus.Status))
    {
        if (stack_location->Parameters.Power.Type == SystemPowerState)
        {
            USBMSG("S%d %s\n",
				power_state.SystemState - PowerSystemWorking, dev->device_id);

            /* save current system state */
            dev->power_state.SystemState = power_state.SystemState;

            /* get supported device power state from the array reported by */
            /* IRP_MN_QUERY_CAPABILITIES */
            dev_power_state = dev->device_power_states[power_state.SystemState];

            /* set the device power state, but don't block the thread */
            power_set_device_state(dev, dev_power_state, FALSE);
        }
        else /* DevicePowerState */
        {
            USBMSG("D%d %s\n",
				power_state.DeviceState - PowerDeviceD0, dev->device_id);

            if (power_state.DeviceState <= dev->power_state.DeviceState)
            {
                /* device is powered up, */
                /* report device state to Power Manager */
                PoSetPowerState(dev->self, DevicePowerState, power_state);
            }
            /* save current device state */
            dev->power_state.DeviceState = power_state.DeviceState;
        }
    }
    else
    {
        USBMSG0("failed\n");
    }

    remove_lock_release(dev);

    return STATUS_SUCCESS;
}

static NTSTATUS DDKAPI
on_filter_power_state_complete(DEVICE_OBJECT *device_object,
                               IRP           *irp,
                               void          *context)
{
    libusb_device_t *dev = context;
    IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);
    POWER_STATE power_state = stack_location->Parameters.Power.State;

    if (NT_SUCCESS(irp->IoStatus.Status))
    {
        if (stack_location->Parameters.Power.Type == SystemPowerState)
        {
            USBMSG("S%d\n",
				power_state.SystemState - PowerSystemWorking);

            /* save current system state */
            dev->power_state.SystemState = power_state.SystemState;


        }
        else /* DevicePowerState */
        {
            USBMSG("D%d\n",
				power_state.DeviceState - PowerDeviceD0);

            if (power_state.DeviceState <= dev->power_state.DeviceState)
            {
                /* device is powered up, */
                /* report device state to Power Manager */
                PoSetPowerState(dev->self, DevicePowerState, power_state);
            }

            /* save current device state */
            dev->power_state.DeviceState = power_state.DeviceState;
        }
    }
    else
    {
        USBMSG0("failed\n");
    }

    remove_lock_release(dev);

    return STATUS_SUCCESS;
}

static void DDKAPI
on_power_set_device_state_complete(DEVICE_OBJECT *device_object,
                                   UCHAR minor_function,
                                   POWER_STATE power_state,
                                   void *context,
                                   IO_STATUS_BLOCK *io_status)
{
    KeSetEvent((KEVENT *)context, EVENT_INCREMENT, FALSE);
}


void power_set_device_state(libusb_device_t *dev,
                            DEVICE_POWER_STATE device_state, bool_t block)
{
    NTSTATUS status;
    KEVENT event;
    POWER_STATE power_state;

    power_state.DeviceState = device_state;

	USBMSG("setting device power state to D%d %s\n", 
		power_state.DeviceState - PowerDeviceD0, dev->device_id);

    if (block) /* wait for IRP to complete */
    {
        KeInitializeEvent(&event, NotificationEvent, FALSE);

        /* set the device power state and wait for completion */
        status = PoRequestPowerIrp(dev->physical_device_object,
                                   IRP_MN_SET_POWER,
                                   power_state,
                                   on_power_set_device_state_complete,
                                   &event, NULL);

        if (status == STATUS_PENDING)
        {
            KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        }
    }
    else
    {
        PoRequestPowerIrp(dev->physical_device_object, IRP_MN_SET_POWER,
                          power_state, NULL, NULL, NULL);
    }
}
