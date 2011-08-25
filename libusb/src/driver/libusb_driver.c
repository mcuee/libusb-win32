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


#define __LIBUSB_DRIVER_C__

#include "libusb_driver.h"
#include "libusb-win32_version.h"

// Device objects with an attached device using the 
// driver names listed here skipped in the add_device() routine.
//
const char* attached_driver_skip_list[] = 
{
	"\\driver\\picopp",
	"\\driver\\libusb0",
	NULL
};

// Device objects with an attached device using the 
// driver names listed here have wdf below them.  Special
// restrictions apply to these devices.
//
const char* attached_driver_wdf_list[] = 
{
	"\\driver\\winusb",
	"\\driver\\wudfrd",
	NULL
};

static bool_t match_driver(PDEVICE_OBJECT deviceObject, const char* driverString);


#ifdef DBG

static void debug_show_devices(PDEVICE_OBJECT deviceObject, int index, bool_t showNextDevice)
{
	ANSI_STRING driverName;

	if (index > 16) return;

	if (deviceObject)
	{
		if (deviceObject->DriverObject)
		{
			if (NT_SUCCESS(RtlUnicodeStringToAnsiString(&driverName, &deviceObject->DriverObject->DriverName, TRUE)))
			{
				USBDBG("[%s #%d] %s\n", 
					(showNextDevice ? "NextDevice" : "AttachedDevice"),
					index, driverName.Buffer);
				RtlFreeAnsiString(&driverName);
			}
		}
		if (deviceObject->AttachedDevice && !showNextDevice)
			debug_show_devices(deviceObject->AttachedDevice, index+1, showNextDevice);
		if (deviceObject->NextDevice && showNextDevice)
			debug_show_devices(deviceObject->NextDevice, index+1, showNextDevice);
	}
}
#endif

static bool_t match_driver(PDEVICE_OBJECT deviceObject, const char* driverString)
{
	ANSI_STRING driverName;
	bool_t ret = FALSE;

	if (deviceObject)
	{
		if (deviceObject->DriverObject)
		{
			if (NT_SUCCESS(RtlUnicodeStringToAnsiString(&driverName, &deviceObject->DriverObject->DriverName, TRUE)))
			{
				_strlwr(driverName.Buffer);

				if (strstr(driverName.Buffer,driverString))
				{
					ret = TRUE;
				}
				RtlFreeAnsiString(&driverName);
			}
		}
	}

	return ret;
}

static void DDKAPI unload(DRIVER_OBJECT *driver_object);
static NTSTATUS DDKAPI on_usbd_complete(DEVICE_OBJECT *device_object,
                                        IRP *irp,
                                        void *context);

NTSTATUS DDKAPI DriverEntry(DRIVER_OBJECT *driver_object,
                            UNICODE_STRING *registry_path)
{
    int i;

 	USBMSG("[loading-driver] v%d.%d.%d.%d\n",
		VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);

	/* initialize the driver object's dispatch table */
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
        driver_object->MajorFunction[i] = dispatch;
    }

    driver_object->DriverExtension->AddDevice = add_device;
    driver_object->DriverUnload = unload;

    return STATUS_SUCCESS;
}

NTSTATUS DDKAPI add_device(DRIVER_OBJECT *driver_object,
                           DEVICE_OBJECT *physical_device_object)
{
    NTSTATUS status;
    DEVICE_OBJECT *device_object = NULL;
    libusb_device_t *dev;
    ULONG device_type;
    UNICODE_STRING nt_device_name;
    UNICODE_STRING symbolic_link_name;
    WCHAR tmp_name_0[128];
    WCHAR tmp_name_1[128];
    char id[256];
    char compat_id[256];
	int i;
	DEVICE_OBJECT* attached_device;
	bool_t has_wdf = FALSE;

    /* get the hardware ID from the registry */
    if (!reg_get_hardware_id(physical_device_object, id, sizeof(id)))
    {
        USBERR0("unable to read registry\n");
        return STATUS_SUCCESS;
    }

    /* only attach the (filter) driver to USB devices, skip hubs */
    if (!strstr(id, "usb\\") || 
		strstr(id, "hub") || 
		!strstr(id, "vid_") ||
		!strstr(id, "pid_"))
    {
		USBDBG("skipping non-usb device or hub %s\n",id);
        return STATUS_SUCCESS;
    }
    
	if (!reg_get_compatible_id(physical_device_object, compat_id, sizeof(compat_id)))
    {
        USBERR0("unable to read registry\n");
        return STATUS_SUCCESS;
    }
	// Don't attach to usb device hubs
    if (strstr(compat_id, "class_09"))
    {
		USBDBG("skipping usb device hub (%s) %s\n",compat_id, id);
        return STATUS_SUCCESS;
    }

#ifdef DBG
	debug_show_devices(physical_device_object->AttachedDevice, 0, FALSE);
#endif

	attached_device = physical_device_object->AttachedDevice;
	while (attached_device)
	{
		// make sure this device isn't already using a driver that is
		// incompatible with libusb-win32.
		for (i=0; attached_driver_skip_list[i] != NULL; i++)
		{
			if (match_driver(attached_device, attached_driver_skip_list[i]))
			{
				USBDBG("skipping device %s\n", id);
				return STATUS_SUCCESS;
			}
		}

		// look for wdf
		for (i=0; attached_driver_wdf_list[i] != NULL; i++)
		{
			if (match_driver(attached_device, attached_driver_wdf_list[i]))
			{
				has_wdf = TRUE;
			}
		}

		attached_device=attached_device->AttachedDevice;
	}

	device_object = IoGetAttachedDeviceReference(physical_device_object);
    if (device_object)
    {
        device_type = device_object->DeviceType;
        ObDereferenceObject(device_object);
    }
    else
    {
        device_type = FILE_DEVICE_UNKNOWN;
    }

    /* try to create a new device object */
    for (i = 1; i < LIBUSB_MAX_NUMBER_OF_DEVICES; i++)
    {
        /* initialize some unicode strings */
        _snwprintf(tmp_name_0, sizeof(tmp_name_0)/sizeof(WCHAR), L"%s%04d",
                   LIBUSB_NT_DEVICE_NAME, i);
        _snwprintf(tmp_name_1, sizeof(tmp_name_1)/sizeof(WCHAR), L"%s%04d",
                   LIBUSB_SYMBOLIC_LINK_NAME, i);

        RtlInitUnicodeString(&nt_device_name, tmp_name_0);
        RtlInitUnicodeString(&symbolic_link_name, tmp_name_1);

        /* create the object */
        status = IoCreateDevice(driver_object,
                                sizeof(libusb_device_t),
                                &nt_device_name, device_type, 0, FALSE,
                                &device_object);

        if (NT_SUCCESS(status))
        {
            USBMSG("device #%d created for %s\n", i, id);
            break;
        }

        device_object = NULL;

        /* continue until an unused device name is found */
    }

    if (!device_object)
    {
        USBERR0("creating device failed\n");
        return status;
    }

    status = IoCreateSymbolicLink(&symbolic_link_name, &nt_device_name);

    if (!NT_SUCCESS(status))
    {
        USBERR0("creating symbolic link failed\n");
        IoDeleteDevice(device_object);
        return status;
    }

    /* setup the "device object" */
    dev = device_object->DeviceExtension;

    memset(dev, 0, sizeof(libusb_device_t));

	// [trobinso] See patch: 2873573 (Tim Green)
	dev->self = device_object;
	dev->physical_device_object = physical_device_object;
	dev->id = i;
	
	// store the device id in the device extentions
	RtlCopyMemory(dev->device_id, id, sizeof(dev->device_id));

	/* set initial power states */
	dev->power_state.DeviceState = PowerDeviceD0;
	dev->power_state.SystemState = PowerSystemWorking;

	dev->disallow_power_control = has_wdf;

	/* get device properties from the registry */
	if (!reg_get_properties(dev))
	{
        USBERR0("getting device properties failed\n");
	    IoDeleteSymbolicLink(&symbolic_link_name);
        IoDeleteDevice(device_object);
        return STATUS_SUCCESS;
	}

	if (dev->is_filter && !physical_device_object->AttachedDevice)
	{
		USBWRN("[FILTER-MODE-MISMATCH] device is reporting itself as filter when there are no attached device(s).\n%s\n", id);
	}
	else if (!dev->is_filter && physical_device_object->AttachedDevice)
	{
		USBWRN("[FILTER-MODE-MISMATCH] device is reporting itself as normal when there are already attached device(s).\n%s\n", id);
		//dev->is_filter = TRUE;
	}

	clear_pipe_info(dev);

	remove_lock_initialize(dev);
	
	if (dev->device_interface_in_use)
	{
		status = IoRegisterDeviceInterface(physical_device_object,
			(LPGUID)&dev->device_interface_guid,
			NULL,
			&dev->device_interface_name);
		if (!NT_SUCCESS (status))
		{
			dev->device_interface_name.Buffer = NULL;

			USBERR0("creating device interface failed\n");
			IoDeleteSymbolicLink(&symbolic_link_name);
			IoDeleteDevice(device_object);
			return status;
		}
		else
		{
			set_filter_interface_key(dev,dev->id);
			if (dev->device_interface_in_use)
			{
				HANDLE hKey=NULL;
				if (NT_SUCCESS(IoOpenDeviceInterfaceRegistryKey(&dev->device_interface_name,KEY_ALL_ACCESS,&hKey)))
				{
					UNICODE_STRING valueName;
					RtlInitUnicodeString(&valueName, L"LUsb0");

					if (NT_SUCCESS(ZwSetValueKey(hKey,&valueName, 0, REG_DWORD, &dev->id,sizeof(ULONG))))
					{
						USBMSG("updated interface registry with LUsb0 direct-access symbolic link. id=%04d\n",dev->id);
					}
					else
					{
						USBERR0("IoOpenDeviceInterfaceRegistryKey:ZwSetValueKey failed\n");
					}
					ZwClose(hKey);
				}
				else
				{
					USBERR0("IoOpenDeviceInterfaceRegistryKey failed\n");
				}
			}
		}
	}
	// make sure the the devices can't be removed
	// before we are done adding it.
	if (!NT_SUCCESS(remove_lock_acquire(dev)))
	{
        USBERR0("remove_lock_acquire failed\n");
	    IoDeleteSymbolicLink(&symbolic_link_name);
        IoDeleteDevice(device_object);
        return STATUS_SUCCESS;
	}

    /* attach the newly created device object to the stack */
    dev->next_stack_device = IoAttachDeviceToDeviceStack(device_object, physical_device_object);

    if (!dev->next_stack_device)
    {
        USBERR("attaching %s to device stack failed\n", id);
        IoDeleteSymbolicLink(&symbolic_link_name);
        IoDeleteDevice(device_object);
		remove_lock_release(dev); // always release acquired locks
        return STATUS_NO_SUCH_DEVICE;
    }

	if (dev->is_filter)
    {
		USBDBG("[filter-mode] id=#%d %s\n",dev->id, dev->device_id);

        /* send all USB requests to the PDO in filter driver mode */
        dev->target_device = dev->physical_device_object;

        /* use the same flags as the underlying object */
        device_object->Flags |= dev->next_stack_device->Flags
                                & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);

		// use the same DeviceType as the underlying object
		device_object->DeviceType = dev->next_stack_device->DeviceType;

		// use the same Characteristics as the underlying object
		device_object->Characteristics = dev->next_stack_device->Characteristics;
    }
    else
    {
		USBDBG("[normal-mode] id=#%d %s\n",dev->id, dev->device_id);

		 /* send all USB requests to the lower object in device driver mode */
        dev->target_device = dev->next_stack_device;
        device_object->Flags |= DO_DIRECT_IO | DO_POWER_PAGABLE;
    }

	UpdateContextConfigDescriptor(dev,NULL,0,0,-1);

    device_object->Flags &= ~DO_DEVICE_INITIALIZING;
	remove_lock_release(dev);

    USBMSG("complete status=%08X\n",status);
	return status;
}


VOID DDKAPI unload(DRIVER_OBJECT *driver_object)
{
 	USBMSG("[unloading-driver] v%d.%d.%d.%d\n",
		VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);
}

NTSTATUS complete_irp(IRP *irp, NTSTATUS status, ULONG info)
{
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = info;
    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS call_usbd_ex(libusb_device_t *dev, void *urb, ULONG control_code,
					  int timeout, int max_timeout)
{
	KEVENT event;
	NTSTATUS status;
	IRP *irp;
	IO_STACK_LOCATION *next_irp_stack;
	LARGE_INTEGER _timeout;
	IO_STATUS_BLOCK io_status;

	if (max_timeout > 0 && timeout > max_timeout)
	{
		timeout = max_timeout;
	}
	if (timeout <= 0)
		timeout = LIBUSB_MAX_CONTROL_TRANSFER_TIMEOUT;

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	irp = IoBuildDeviceIoControlRequest(control_code, dev->target_device,
		NULL, 0, NULL, 0, TRUE,
		NULL, &io_status);

	if (!irp)
	{
		return STATUS_NO_MEMORY;
	}

	next_irp_stack = IoGetNextIrpStackLocation(irp);
	next_irp_stack->Parameters.Others.Argument1 = urb;
	next_irp_stack->Parameters.Others.Argument2 = NULL;

	IoSetCompletionRoutine(irp, on_usbd_complete, &event, TRUE, TRUE, TRUE);

	status = IoCallDriver(dev->target_device, irp);
	if(status == STATUS_PENDING)
	{
		_timeout.QuadPart = -(timeout * 10000);

		if(KeWaitForSingleObject(&event, Executive, KernelMode,
			FALSE, &_timeout) == STATUS_TIMEOUT)
		{
			USBERR0("request timed out\n");
			IoCancelIrp(irp);
		}
	}

	/* wait until completion routine is called */
	KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);

	status = irp->IoStatus.Status;

	/* complete the request */
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	
	USBDBG("status = %08Xh\n",status);
	return status;
}

static NTSTATUS DDKAPI on_usbd_complete(DEVICE_OBJECT *device_object,
                                        IRP *irp, void *context)
{
    KeSetEvent((KEVENT *) context, IO_NO_INCREMENT, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS pass_irp_down(libusb_device_t *dev, IRP *irp,
                       PIO_COMPLETION_ROUTINE completion_routine,
                       void *context)
{
    if (completion_routine)
    {
        IoCopyCurrentIrpStackLocationToNext(irp);
        IoSetCompletionRoutine(irp, completion_routine, context,
                               TRUE, TRUE, TRUE);
    }
    else
    {
        IoSkipCurrentIrpStackLocation(irp);
    }

    return IoCallDriver(dev->next_stack_device, irp);
}
/*
bool_t accept_irp(libusb_device_t *dev, IRP *irp)
{
    if (irp->Tail.Overlay.OriginalFileObject)
    {
        return irp->Tail.Overlay.OriginalFileObject->DeviceObject
               == dev->self ? TRUE : FALSE;
    }

    return FALSE;
}
*/
bool_t accept_irp(libusb_device_t *dev, IRP *irp)
{
	/* check if the IRP is sent to libusb's device object or to */
	/* the lower one. This check is necessary since the device object */
	/* might be a filter */
	if(!irp->Tail.Overlay.OriginalFileObject)
		return FALSE;
	if (irp->Tail.Overlay.OriginalFileObject->DeviceObject == dev->self)
		return TRUE;

	/* cover the cases when access is made by using device interfaces */
	if (!dev->is_filter &&
		dev->device_interface_in_use &&
		(irp->Tail.Overlay.OriginalFileObject->DeviceObject == dev->physical_device_object)) 
		return TRUE;

	return FALSE;
}

bool_t get_pipe_handle(libusb_device_t *dev, int endpoint_address,
                       USBD_PIPE_HANDLE *pipe_handle)
{
    int i, j;

    *pipe_handle = NULL;

    for (i = 0; i < LIBUSB_MAX_NUMBER_OF_INTERFACES; i++)
    {
        if (dev->config.interfaces[i].valid)
        {
            for (j = 0; j < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; j++)
            {
                if (dev->config.interfaces[i].endpoints[j].address
                        == endpoint_address)
                {
                    *pipe_handle = dev->config.interfaces[i].endpoints[j].handle;

                    return !*pipe_handle ? FALSE : TRUE;
                }
            }
        }
    }

    return FALSE;
}

bool_t get_pipe_info(libusb_device_t *dev, int endpoint_address,
                       libusb_endpoint_t** pipe_info)
{
    int i, j;

    *pipe_info = NULL;

    for (i = 0; i < LIBUSB_MAX_NUMBER_OF_INTERFACES; i++)
    {
        if (dev->config.interfaces[i].valid)
        {
            for (j = 0; j < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; j++)
            {
                if (dev->config.interfaces[i].endpoints[j].address
                        == endpoint_address)
                {
                    *pipe_info = &dev->config.interfaces[i].endpoints[j];

                    return !*pipe_info ? FALSE : TRUE;
                }
            }
        }
    }

    return FALSE;
}

void clear_pipe_info(libusb_device_t *dev)
{
    memset(dev->config.interfaces, 0 , sizeof(dev->config.interfaces));
}

bool_t update_pipe_info(libusb_device_t *dev,
                        USBD_INTERFACE_INFORMATION *interface_info)
{
    int i;
    int number;
	int maxTransferSize;
	int maxPacketSize;

    if (!interface_info)
    {
        return FALSE;
    }

    number = interface_info->InterfaceNumber;

    if (interface_info->InterfaceNumber >= LIBUSB_MAX_NUMBER_OF_INTERFACES)
    {
        return FALSE;
    }

    USBMSG("interface %d\n", number);

    dev->config.interfaces[number].valid = TRUE;

    for (i = 0; i < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; i++)
    {
        dev->config.interfaces[number].endpoints[i].address = 0;
        dev->config.interfaces[number].endpoints[i].handle = NULL;
    }

    if (interface_info)
    {
        for (i = 0; i < (int)interface_info->NumberOfPipes
                && i < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; i++)
        {
			maxPacketSize = interface_info->Pipes[i].MaximumPacketSize;
			maxTransferSize = interface_info->Pipes[i].MaximumTransferSize;

			USBMSG("EP%02Xh maximum-packet-size=%d maximum-transfer-size=%d\n",
				interface_info->Pipes[i].EndpointAddress,
				maxPacketSize,
				maxTransferSize);

			dev->config.interfaces[number].endpoints[i].handle  = interface_info->Pipes[i].PipeHandle;
            dev->config.interfaces[number].endpoints[i].address = interface_info->Pipes[i].EndpointAddress;
            dev->config.interfaces[number].endpoints[i].maximum_packet_size = maxPacketSize;
            dev->config.interfaces[number].endpoints[i].interval = interface_info->Pipes[i].Interval;
            dev->config.interfaces[number].endpoints[i].pipe_type = interface_info->Pipes[i].PipeType;
 			dev->config.interfaces[number].endpoints[i].pipe_flags = interface_info->Pipes[i].PipeFlags;
          
			if (maxPacketSize)
			{
				// set max the maximum transfer size default to an interval of max packet size.
				maxTransferSize = maxTransferSize - (maxTransferSize % maxPacketSize);
				if (maxTransferSize < maxPacketSize) 
				{
					maxTransferSize = LIBUSB_MAX_READ_WRITE;
				}
				else if (maxTransferSize > LIBUSB_MAX_READ_WRITE)
				{
					maxTransferSize = LIBUSB_MAX_READ_WRITE - (LIBUSB_MAX_READ_WRITE % maxPacketSize);
				}

				if (maxTransferSize != interface_info->Pipes[i].MaximumTransferSize)
				{
					USBWRN("overriding EP%02Xh maximum-transfer-size=%d\n",
						dev->config.interfaces[number].endpoints[i].address,
						maxTransferSize);
				}
			}
			else
			{
				if (!maxTransferSize)
				{
					// use the libusb-win32 default
					maxTransferSize = LIBUSB_MAX_READ_WRITE;
				}
			}
			dev->config.interfaces[number].endpoints[i].maximum_transfer_size = maxTransferSize;
		}
	}
    return TRUE;
}


void remove_lock_initialize(libusb_device_t *dev)
{
    KeInitializeEvent(&dev->remove_lock.event, NotificationEvent, FALSE);
    dev->remove_lock.usage_count = 1;
    dev->remove_lock.remove_pending = FALSE;
}


NTSTATUS remove_lock_acquire(libusb_device_t *dev)
{
    InterlockedIncrement(&dev->remove_lock.usage_count);

    if (dev->remove_lock.remove_pending)
    {
        if (InterlockedDecrement(&dev->remove_lock.usage_count) == 0)
        {
            KeSetEvent(&dev->remove_lock.event, 0, FALSE);
        }
        return STATUS_DELETE_PENDING;
    }
    return STATUS_SUCCESS;
}


void remove_lock_release(libusb_device_t *dev)
{
    if (InterlockedDecrement(&dev->remove_lock.usage_count) == 0)
    {
        KeSetEvent(&dev->remove_lock.event, 0, FALSE);
    }
}


void remove_lock_release_and_wait(libusb_device_t *dev)
{
    dev->remove_lock.remove_pending = TRUE;
    remove_lock_release(dev);
    remove_lock_release(dev);
    KeWaitForSingleObject(&dev->remove_lock.event, Executive, KernelMode,
                          FALSE, NULL);
}


USB_INTERFACE_DESCRIPTOR *
find_interface_desc(USB_CONFIGURATION_DESCRIPTOR *config_desc,
                    unsigned int size, int interface_number, int altsetting)
{
    usb_descriptor_header_t *desc = (usb_descriptor_header_t *)config_desc;
    char *p = (char *)desc;
    USB_INTERFACE_DESCRIPTOR *if_desc = NULL;

    if (!config_desc || (size < config_desc->wTotalLength))
        return NULL;

    while (size && desc->length <= size)
    {
        if (desc->type == USB_INTERFACE_DESCRIPTOR_TYPE)
        {
            if_desc = (USB_INTERFACE_DESCRIPTOR *)desc;

            if ((if_desc->bInterfaceNumber == (UCHAR)interface_number)
                    && (if_desc->bAlternateSetting == (UCHAR)altsetting))
            {
                return if_desc;
            }
        }

        size -= desc->length;
        p += desc->length;
        desc = (usb_descriptor_header_t *)p;
    }

    return NULL;
}

USB_INTERFACE_DESCRIPTOR* find_interface_desc_ex(USB_CONFIGURATION_DESCRIPTOR *config_desc,
												 unsigned int size,
												 interface_request_t* intf,
												 unsigned int* size_left)
{
#define INTF_FIELD 0
#define ALTF_FIELD 1

	usb_descriptor_header_t *desc = (usb_descriptor_header_t *)config_desc;
    char *p = (char *)desc;
	int lastInfNumber, lastAltNumber;
	int currentInfIndex;
	short InterfacesByIndex[LIBUSB_MAX_NUMBER_OF_INTERFACES][2];

    USB_INTERFACE_DESCRIPTOR *if_desc = NULL;

	memset(InterfacesByIndex,0xFF,sizeof(InterfacesByIndex));

    if (!config_desc)
        return NULL;

	size = size > config_desc->wTotalLength ? config_desc->wTotalLength : size;

    while (size && desc->length <= size)
    {
        if (desc->type == USB_INTERFACE_DESCRIPTOR_TYPE)
        {
			// this is a new interface or alternate interface
            if_desc = (USB_INTERFACE_DESCRIPTOR *)desc;
			for (currentInfIndex=0; currentInfIndex<LIBUSB_MAX_NUMBER_OF_INTERFACES;currentInfIndex++)
			{
				if (InterfacesByIndex[currentInfIndex][INTF_FIELD]==-1)
				{
					// this is a new interface
					InterfacesByIndex[currentInfIndex][INTF_FIELD]=if_desc->bInterfaceNumber;
					InterfacesByIndex[currentInfIndex][ALTF_FIELD]=0;
					break;
				}
				else if (InterfacesByIndex[currentInfIndex][INTF_FIELD]==if_desc->bInterfaceNumber)
				{
					// this is a new alternate interface
					InterfacesByIndex[currentInfIndex][ALTF_FIELD]++;
					break;
				}
			}

			// if the interface index is -1, then we don't care; 
			// i.e. any interface number or index
			if (intf->interface_index!=FIND_INTERFACE_INDEX_ANY)
			{
				if (intf->intf_use_index)
				{
					// looking for a particular interface index; if this is not it then continue on.
					if (intf->interface_index != currentInfIndex)
						goto NextInterface;
				}
				else
				{
					// looking for a particular interface number; if this is not it then continue on.
					if (intf->interface_number != if_desc->bInterfaceNumber)
						goto NextInterface;
				}
			}

			if (intf->altsetting_index!=FIND_INTERFACE_INDEX_ANY)
			{
				if (intf->altf_use_index)
				{
					// looking for a particular alternate interface index; if this is not it then continue on.
					if (intf->altsetting_index != InterfacesByIndex[currentInfIndex][ALTF_FIELD])
						goto NextInterface;
				}
				else
				{
					// looking for a particular alternate interface number; if this is not it then continue on.
					if (intf->altsetting_number != if_desc->bAlternateSetting)
						goto NextInterface;
				}
			}

			// found a match
			intf->interface_index = (unsigned char)currentInfIndex;
			intf->altsetting_index = (unsigned char)InterfacesByIndex[currentInfIndex][ALTF_FIELD];
			intf->interface_number = if_desc->bInterfaceNumber;
			intf->altsetting_number = if_desc->bAlternateSetting;

			if (size_left)
			{
				*size_left=size;
			}
			return if_desc;

        }

NextInterface:
        size -= desc->length;
        p += desc->length;
        desc = (usb_descriptor_header_t *)p;
    }

    return NULL;
}

USB_ENDPOINT_DESCRIPTOR *
find_endpoint_desc_by_index(USB_INTERFACE_DESCRIPTOR *interface_desc,
                    unsigned int size, int pipe_index)
{
    usb_descriptor_header_t *desc = (usb_descriptor_header_t *)interface_desc;
    char *p = (char *)desc;
	int currentPipeIndex;
	short PipesByIndex[LIBUSB_MAX_NUMBER_OF_ENDPOINTS];

    USB_ENDPOINT_DESCRIPTOR *ep_desc = NULL;
	memset(PipesByIndex,0xFF,sizeof(PipesByIndex));

	if (size && desc->length <= size)
	{
        size -= desc->length;
        p += desc->length;
        desc = (usb_descriptor_header_t *)p;
	}

    while (size && desc->length <= size)
    {
        if (desc->type == USB_ENDPOINT_DESCRIPTOR_TYPE)
        {
            ep_desc = (USB_ENDPOINT_DESCRIPTOR *)desc;
			for (currentPipeIndex=0; currentPipeIndex<LIBUSB_MAX_NUMBER_OF_ENDPOINTS;currentPipeIndex++)
			{
				if (PipesByIndex[currentPipeIndex]==-1)
				{
					PipesByIndex[currentPipeIndex]=ep_desc->bEndpointAddress;
					break;
				}
				else if (PipesByIndex[currentPipeIndex]==ep_desc->bEndpointAddress)
				{
					// the pipe is defined twice in the same interface
					USBWRN("invalid endpoint descriptor at pipe index=%d\n",currentPipeIndex);
					break;
				}
			}

			if (pipe_index == currentPipeIndex)
			{
				return ep_desc;
			}
        }
		else
		{
			break;
		}

        size -= desc->length;
        p += desc->length;
        desc = (usb_descriptor_header_t *)p;
    }

    return NULL;
}


ULONG get_current_frame(IN PDEVICE_EXTENSION deviceExtension, IN PIRP Irp)
/*++

Routine Description:

    This routine send an irp/urb pair with
    function code URB_FUNCTION_GET_CURRENT_FRAME_NUMBER
    to fetch the current frame

Arguments:

    DeviceObject - pointer to device object
    PIRP - I/O request packet

Return Value:

    Current frame

--*/
{
    KEVENT                               event;
    PIO_STACK_LOCATION                   nextStack;
    struct _URB_GET_CURRENT_FRAME_NUMBER urb;

    //
    // initialize the urb
    //

    urb.Hdr.Function = URB_FUNCTION_GET_CURRENT_FRAME_NUMBER;
    urb.Hdr.Length = sizeof(urb);
    urb.FrameNumber = (ULONG) -1;

    nextStack = IoGetNextIrpStackLocation(Irp);
    nextStack->Parameters.Others.Argument1 = (PVOID) &urb;
    nextStack->Parameters.DeviceIoControl.IoControlCode =
                                IOCTL_INTERNAL_USB_SUBMIT_URB;
    nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;

    KeInitializeEvent(&event,
                      NotificationEvent,
                      FALSE);

    IoSetCompletionRoutine(Irp,
                           on_usbd_complete,
                           &event,
                           TRUE,
                           TRUE,
                           TRUE);


    IoCallDriver(deviceExtension->target_device, Irp);

    KeWaitForSingleObject((PVOID) &event,
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL);

    return urb.FrameNumber;
}
