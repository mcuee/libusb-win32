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



#include <wdm.h>
#include <wchar.h>
#include <initguid.h>


// {798DC6FE-4216-48e2-A851-D67957A05C3A}
DEFINE_GUID(LIBUSB_DEFAULT_GUID, \
0x798dc6fe, 0x4216, 0x48e2, 0xa8, 0x51, 0xd6, 0x79, 0x57, 0xa0, 0x5c, 0x3a);

#define LIBUSB_GUID_KEY L"libusb-guid"


typedef struct
{
  DEVICE_OBJECT	*self;
  DEVICE_OBJECT	*next_stack_device;
  UNICODE_STRING interface_name;
  int interface_valid;
} libusb_device_extension;


void __stdcall unload(DRIVER_OBJECT *driver_object);
NTSTATUS __stdcall add_device(DRIVER_OBJECT *driver_object, 
			      DEVICE_OBJECT *physical_device_object);
static NTSTATUS dispatch(DEVICE_OBJECT *device_object, IRP *irp);
static NTSTATUS dispatch_pnp(libusb_device_extension *device_extension, 
			     IRP *irp);
static NTSTATUS on_pnp_complete(DEVICE_OBJECT *device_object,IRP *irp,
				void *context);

NTSTATUS __stdcall DriverEntry(DRIVER_OBJECT *driver_object,
			       UNICODE_STRING *registry_path)
{
  PDRIVER_DISPATCH *dispatch_function = driver_object->MajorFunction;
  int i;
  
  for(i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++, dispatch_function++) 
    {
      *dispatch_function = dispatch;
    }
  
  driver_object->DriverExtension->AddDevice = add_device;
  driver_object->DriverUnload = unload;

  return STATUS_SUCCESS;
}

NTSTATUS __stdcall add_device(DRIVER_OBJECT *driver_object, 
			      DEVICE_OBJECT *physical_device_object)
{
  NTSTATUS status;
  DEVICE_OBJECT *device_object;
  libusb_device_extension *device_extension;
  ULONG device_type = FILE_DEVICE_UNKNOWN;
  HANDLE reg_key = NULL;
  GUID guid;
  WCHAR guid_string[256];
  KEY_VALUE_FULL_INFORMATION *guid_value;
  WCHAR guid_key[256];
  UNICODE_STRING guid_string_uni;
  UNICODE_STRING guid_key_uni;
  ULONG tmp;

  if(!IoIsWdmVersionAvailable(1, 0x20)) 
    {
      device_object = IoGetAttachedDeviceReference(physical_device_object);
      device_type = device_object->DeviceType;
      ObDereferenceObject(device_object);
    }

  status = IoCreateDevice(driver_object, sizeof(libusb_device_extension), 
			  NULL, device_type, 0, FALSE, 
			  &device_object);
  if(!NT_SUCCESS(status))
    {
      return status;
    }

  device_extension = (libusb_device_extension *)device_object->DeviceExtension;
  device_extension->self = device_object;
  device_extension->next_stack_device = 
    IoAttachDeviceToDeviceStack(device_object, physical_device_object);
  
  device_object->DeviceType = device_extension->next_stack_device->DeviceType;
  device_object->Characteristics =
                          device_extension->next_stack_device->Characteristics;

  device_extension->interface_valid = FALSE;

  if(IoOpenDeviceRegistryKey(physical_device_object, PLUGPLAY_REGKEY_DEVICE,
 			     STANDARD_RIGHTS_READ, &reg_key) == STATUS_SUCCESS)
     {
       RtlZeroMemory(guid_key, sizeof(guid_key));
       RtlZeroMemory(guid_string, sizeof(guid_string));
       
       _snwprintf(guid_key, sizeof(guid_key)/sizeof(WCHAR), L"%s", 
		  LIBUSB_GUID_KEY);
       RtlInitUnicodeString(&guid_key_uni, guid_key);
       
       if((ZwQueryValueKey(reg_key, &guid_key_uni,
			   KeyValueFullInformation,
			   guid_string,
			   sizeof(guid_string),
			   &tmp) == STATUS_SUCCESS) && tmp)
	 {
	   guid_value = (KEY_VALUE_FULL_INFORMATION *)guid_string;
	   RtlInitUnicodeString(&guid_string_uni, 
				(WCHAR *)(((char *)guid_value) 
					  + guid_value->DataOffset));
	   
	   if(RtlGUIDFromString(&guid_string_uni, &guid) == STATUS_SUCCESS)
	     {
	       if(IoRegisterDeviceInterface(physical_device_object,
					    &guid, NULL,
					    &device_extension->interface_name)
		  == STATUS_SUCCESS)
		 {
		   device_extension->interface_valid = TRUE;
		 }
	     }
	 }
       ZwClose(reg_key);
     }
  else
    {
      if(IoRegisterDeviceInterface(physical_device_object,
				   (LPGUID)&LIBUSB_DEFAULT_GUID,
				   NULL,
				   &device_extension->interface_name)
	 == STATUS_SUCCESS)
	device_extension->interface_valid = TRUE;
    }

  device_object->Flags |= DO_DIRECT_IO | DO_POWER_PAGABLE;
  device_object->Flags &= ~DO_DEVICE_INITIALIZING;

  return STATUS_SUCCESS;
}


NTSTATUS __stdcall dispatch(DEVICE_OBJECT *device_object, IRP *irp)
{
  libusb_device_extension *device_extension = 
    (libusb_device_extension *)device_object->DeviceExtension;
  NTSTATUS status = STATUS_NOT_SUPPORTED;

  switch(IoGetCurrentIrpStackLocation(irp)->MajorFunction) 
    {
    case IRP_MJ_PNP:
      return dispatch_pnp(device_extension, irp);
    case IRP_MJ_POWER:
      PoStartNextPowerIrp(irp);
      IoSkipCurrentIrpStackLocation(irp);
      return PoCallDriver(device_extension->next_stack_device, irp);
    case IRP_MJ_CLEANUP:
      status = STATUS_SUCCESS;
    default:
      ;
    }

  irp->IoStatus.Status = status;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
 
  return status;
}

NTSTATUS dispatch_pnp(libusb_device_extension *device_extension, IRP *irp)
{
  NTSTATUS status = STATUS_NOT_SUPPORTED;
  KEVENT event;
  IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);

  switch(stack_location->MinorFunction) 
    {      
    case IRP_MN_REMOVE_DEVICE:
      irp->IoStatus.Status = STATUS_SUCCESS;
      
      if(device_extension->interface_valid)
	IoSetDeviceInterfaceState(&device_extension->interface_name, FALSE);
      
      RtlFreeUnicodeString(&device_extension->interface_name);
      IoSkipCurrentIrpStackLocation(irp);
      status = IoCallDriver(device_extension->next_stack_device, irp);
      IoDetachDevice(device_extension->next_stack_device);
      IoDeleteDevice(device_extension->self);
      return status;
    case IRP_MN_QUERY_CAPABILITIES: 
      stack_location->Parameters.DeviceCapabilities.Capabilities
	->SurpriseRemovalOK = TRUE;

      if(!IoIsWdmVersionAvailable(1, 0x20))
	{
	  KeInitializeEvent(&event, NotificationEvent, FALSE);
	  IoCopyCurrentIrpStackLocationToNext(irp);
	  
	  IoSetCompletionRoutine(irp, on_pnp_complete, &event,
				 TRUE, TRUE, TRUE);
	  
	  status = IoCallDriver(device_extension->next_stack_device, irp);
	  
	  if(status == STATUS_PENDING) 
	    {
	      KeWaitForSingleObject(&event, Executive, KernelMode,
				    FALSE, NULL);
	      status = irp->IoStatus.Status;
	    }
	  
	  if(NT_SUCCESS(status))
	    {
	      stack_location->Parameters.DeviceCapabilities.Capabilities
		->SurpriseRemovalOK = TRUE;
	    }
	  return status;
	} 
      
      break;

    case IRP_MN_START_DEVICE:
      if(device_extension->interface_valid)
	IoSetDeviceInterfaceState(&device_extension->interface_name, TRUE);
      status = STATUS_SUCCESS;
      break;
    case IRP_MN_SURPRISE_REMOVAL:
    case IRP_MN_CANCEL_REMOVE_DEVICE:
    case IRP_MN_CANCEL_STOP_DEVICE:
    case IRP_MN_QUERY_STOP_DEVICE:
    case IRP_MN_STOP_DEVICE:
    case IRP_MN_QUERY_REMOVE_DEVICE:
      status = STATUS_SUCCESS;
    default:
      ;
    }
  irp->IoStatus.Status = status;
  IoSkipCurrentIrpStackLocation(irp);
  return IoCallDriver(device_extension->next_stack_device, irp);
}


VOID __stdcall unload(DRIVER_OBJECT *driver_object)
{

}

NTSTATUS on_pnp_complete(DEVICE_OBJECT *device_object,IRP *irp, void *context)
{
    KEVENT *event = (KEVENT *)context;

    if(irp->PendingReturned == TRUE) 
      {
        KeSetEvent(event, IO_NO_INCREMENT, FALSE);
      }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

