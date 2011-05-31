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
on_start_complete(DEVICE_OBJECT *device_object, IRP *irp,
                  void *context);

static NTSTATUS DDKAPI
on_device_usage_notification_complete(DEVICE_OBJECT *device_object,
                                      IRP *irp, void *context);

static NTSTATUS DDKAPI
on_query_capabilities_complete(DEVICE_OBJECT *device_object,
                               IRP *irp, void *context);


NTSTATUS dispatch_pnp(libusb_device_t *dev, IRP *irp)
{
    NTSTATUS status = STATUS_SUCCESS;
    IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);
    UNICODE_STRING symbolic_link_name;
    WCHAR tmp_name[128];

    status = remove_lock_acquire(dev);

    if (!NT_SUCCESS(status))
    {
		USBDBG("device %s is pending removal\n",dev->device_id);
		return complete_irp(irp, status, 0);
    }

    switch (stack_location->MinorFunction)
    {
    case IRP_MN_REMOVE_DEVICE:

		USBMSG("IRP_MN_REMOVE_DEVICE: is-filter=%c %s\n",
			dev->is_filter ? 'Y' : 'N',
			dev->device_id);

		if (dev->device_interface_in_use && dev->is_started)
		{
			set_filter_interface_key(dev,(ULONG)-1);

			status = IoSetDeviceInterfaceState(&dev->device_interface_name, FALSE);
			if (!NT_SUCCESS(status))
			{
				USBERR0("IRP_MN_REMOVE_DEVICE: disabling device interface failed\n");
			}
		}

		dev->is_started = FALSE;

		/* wait until all outstanding requests are finished */
        remove_lock_release_and_wait(dev);

        status = pass_irp_down(dev, irp, NULL, NULL);

		USBMSG("deleting device #%d %s\n", dev->id, dev->device_id);

       _snwprintf(tmp_name, sizeof(tmp_name)/sizeof(WCHAR), L"%s%04d",
                   LIBUSB_SYMBOLIC_LINK_NAME, dev->id);

     /* delete the symbolic link */
        RtlInitUnicodeString(&symbolic_link_name, tmp_name);
        IoDeleteSymbolicLink(&symbolic_link_name);

		if (dev->device_interface_in_use && dev->device_interface_name.Buffer)
		{
			RtlFreeUnicodeString(&dev->device_interface_name);
		}
		UpdateContextConfigDescriptor(dev,NULL,0,0,-1);

        /* delete the device object */
        IoDetachDevice(dev->next_stack_device);
        IoDeleteDevice(dev->self);

        return status;

    case IRP_MN_SURPRISE_REMOVAL:

        dev->is_started = FALSE;
		USBMSG("IRP_MN_SURPRISE_REMOVAL: is-filter=%c %s\n",
			dev->is_filter ? 'Y' : 'N',
			dev->device_id);

		if (dev->device_interface_in_use) 
		{
			set_filter_interface_key(dev,(ULONG)-1);
			IoSetDeviceInterfaceState(&dev->device_interface_name, FALSE);
			dev->device_interface_in_use=FALSE;
			if (dev->device_interface_name.Buffer)
			{
				RtlFreeUnicodeString(&dev->device_interface_name);
				dev->device_interface_name.Buffer = NULL;
			}
		}
		UpdateContextConfigDescriptor(dev,NULL,0,0,-1);
		status = STATUS_SUCCESS;

		break;

    case IRP_MN_START_DEVICE:
		USBMSG("IRP_MN_START_DEVICE: is-filter=%c %s\n",
			dev->is_filter ? 'Y' : 'N',
			dev->device_id);

		// A driver calls this routine after receiving a device set-power 
		// request and before calling PoStartNextPowerIrp. When handling a 
		// PnP IRP_MN_START_DEVICE request, the driver should call 
		// PoSetPowerState to notify the power manager that the device is 
		// in the D0 state.
		//
		PoSetPowerState(dev->self, DevicePowerState, dev->power_state);
		if (dev->device_interface_in_use)
		{
			status = IoSetDeviceInterfaceState(&dev->device_interface_name, TRUE);
			if (!NT_SUCCESS(status))
			{
				USBERR0("IRP_MN_START_DEVICE: enabling device interface failed\n");
			}
		}
        return pass_irp_down(dev, irp, on_start_complete, NULL);

    case IRP_MN_STOP_DEVICE:
        dev->is_started = FALSE;
		USBDBG("IRP_MN_STOP_DEVICE: is-filter=%c %s\n",
			dev->is_filter ? 'Y' : 'N',
			dev->device_id);
        break;

    case IRP_MN_DEVICE_USAGE_NOTIFICATION:
		USBDBG("IRP_MN_DEVICE_USAGE_NOTIFICATION: is-filter=%c %s\n",
			dev->is_filter ? 'Y' : 'N',
			dev->device_id);

        if (!dev->self->AttachedDevice
                || (dev->self->AttachedDevice->Flags & DO_POWER_PAGABLE))
        {
            dev->self->Flags |= DO_POWER_PAGABLE;
        }

        return pass_irp_down(dev, irp, on_device_usage_notification_complete, NULL);

    case IRP_MN_QUERY_CAPABILITIES:
		USBDBG("IRP_MN_QUERY_CAPABILITIES: is-filter=%c %s\n",
			dev->is_filter ? 'Y' : 'N',
			dev->device_id);

		if (!dev->is_filter)
        {
            /* apply registry setting */
            stack_location->Parameters.DeviceCapabilities.Capabilities->SurpriseRemovalOK = dev->surprise_removal_ok;
        }

        return pass_irp_down(dev, irp, on_query_capabilities_complete,  NULL);

    default:
		break;

    }

    remove_lock_release(dev);
    return pass_irp_down(dev, irp, NULL, NULL);
}

static NTSTATUS DDKAPI
on_start_complete(DEVICE_OBJECT *device_object, IRP *irp, void *context)
{
    libusb_device_t *dev = device_object->DeviceExtension;

	if (irp->PendingReturned)
    {
        IoMarkIrpPending(irp);
    }

	USBDBG("is-filter=%c %s\n",
		dev->is_filter ? 'Y' : 'N',
		dev->device_id);

    if (dev->next_stack_device->Characteristics & FILE_REMOVABLE_MEDIA)
    {
        device_object->Characteristics |= FILE_REMOVABLE_MEDIA;
    }
#ifndef SKIP_CONFIGURE_NORMAL_DEVICES
	// select initial configuration if not a filter
	if (!dev->is_filter && !dev->is_started)
	{
		// optionally, the initial configuration value can be specified
		// in the inf file. See reg_get_properties()
		// HKR,,"InitialConfigValue",0x00010001,<your config value>

		// If initial_config_value is negative, the configuration will
		// only be set if the device is not already configured.
		if (dev->initial_config_value)
		{
			if (dev->initial_config_value == SET_CONFIG_ACTIVE_CONFIG)
			{
				USBDBG("applying active configuration for %s\n",
					dev->device_id);
			}
			else
			{
				USBDBG("applying InitialConfigValue %d for %s\n",
					dev->initial_config_value, dev->device_id);
			}

			if(!NT_SUCCESS(set_configuration(dev, dev->initial_config_value, LIBUSB_DEFAULT_TIMEOUT)))
			{
				// we should always be able to apply the active configuration,
				// even in the case of composite devices.
				if (dev->initial_config_value == SET_CONFIG_ACTIVE_CONFIG)
				{
					USBERR("failed applying active configuration for %s\n",
						dev->device_id);
				}
				else
				{
					USBERR("failed applying InitialConfigValue %d for %s\n",
						dev->initial_config_value, dev->device_id);
				}
			}
		}
	}
#endif
	dev->is_started = TRUE;
    remove_lock_release(dev);

    return STATUS_SUCCESS;
}

static NTSTATUS DDKAPI
on_device_usage_notification_complete(DEVICE_OBJECT *device_object,
                                      IRP *irp, void *context)
{
    libusb_device_t *dev = device_object->DeviceExtension;

    if (irp->PendingReturned)
    {
        IoMarkIrpPending(irp);
    }

    if (!(dev->next_stack_device->Flags & DO_POWER_PAGABLE))
    {
        device_object->Flags &= ~DO_POWER_PAGABLE;
    }

    remove_lock_release(dev);

    return STATUS_SUCCESS;
}

static NTSTATUS DDKAPI
on_query_capabilities_complete(DEVICE_OBJECT *device_object,
                               IRP *irp, void *context)
{
    libusb_device_t *dev = device_object->DeviceExtension;
    IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);

    if (irp->PendingReturned)
    {
        IoMarkIrpPending(irp);
    }

    if (NT_SUCCESS(irp->IoStatus.Status))
    {
        if (!dev->is_filter)
        {
            /* apply registry setting */
            stack_location->Parameters.DeviceCapabilities.Capabilities
            ->SurpriseRemovalOK = dev->surprise_removal_ok;
        }

        /* save supported device power states */
        memcpy(dev->device_power_states, stack_location
               ->Parameters.DeviceCapabilities.Capabilities->DeviceState,
               sizeof(dev->device_power_states));
    }

    remove_lock_release(dev);

    return STATUS_SUCCESS;
}
