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


static struct {
  KMUTEX mutex;
  int is_used[LIBUSB_MAX_NUMBER_OF_DEVICES];
} device_ids;

static void DDKAPI unload(DRIVER_OBJECT *driver_object);
static NTSTATUS DDKAPI add_device(DRIVER_OBJECT *driver_object, 
                                  DEVICE_OBJECT *physical_device_object);

static NTSTATUS DDKAPI on_usbd_complete(DEVICE_OBJECT *device_object, 
                                           IRP *irp, 
                                           void *context);

NTSTATUS DDKAPI DriverEntry(DRIVER_OBJECT *driver_object,
                            UNICODE_STRING *registry_path)
{
  PDRIVER_DISPATCH *dispatch_function = 
    (PDRIVER_DISPATCH *)driver_object->MajorFunction;
  int i;

  KeInitializeMutex(&device_ids.mutex, 0);

  for(i = 0; i < LIBUSB_MAX_NUMBER_OF_DEVICES; i++)
    {
      device_ids.is_used[i] = 0;
    }

  for(i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++, dispatch_function++) 
    {
      *dispatch_function = dispatch;
    }
  
  driver_object->DriverExtension->AddDevice = add_device;
  driver_object->DriverUnload = unload;

  debug_printf(LIBUSB_DEBUG_MSG, "DriverEntry(): loading driver");

  return STATUS_SUCCESS;
}

NTSTATUS DDKAPI add_device(DRIVER_OBJECT *driver_object, 
                           DEVICE_OBJECT *physical_device_object)
{
  NTSTATUS status;
  DEVICE_OBJECT *device_object;
  libusb_device_extension *device_extension;
  ULONG device_type = FILE_DEVICE_UNKNOWN;

  UNICODE_STRING nt_device_name;
  UNICODE_STRING symbolic_link_name;
  WCHAR tmp_name_0[128];
  WCHAR tmp_name_1[128];
  int id;

  device_object = IoGetAttachedDeviceReference(physical_device_object);
  device_type = device_object->DeviceType;
  ObDereferenceObject(device_object);
  

  if(!get_device_id(&id))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "add_device(): getting device id failed");
      return STATUS_UNSUCCESSFUL;
    }

  debug_printf(LIBUSB_DEBUG_MSG, "add_device(): creating device %d", id);

  _snwprintf(tmp_name_0, sizeof(tmp_name_0)/sizeof(WCHAR), L"%s%04d", 
             LIBUSB_NT_DEVICE_NAME, id);
  
  RtlInitUnicodeString(&nt_device_name, tmp_name_0);

  _snwprintf(tmp_name_1, sizeof(tmp_name_1)/sizeof(WCHAR), L"%s%04d", 
             LIBUSB_SYMBOLIC_LINK_NAME, id);
  
  RtlInitUnicodeString(&symbolic_link_name, tmp_name_1);


  status = IoCreateDevice(driver_object, sizeof(libusb_device_extension), 
                          &nt_device_name, device_type, 0, FALSE, 
                          &device_object);


  if(!NT_SUCCESS(status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "add_device(): creating device failed");
      release_device_id(id);
      return status;
    }

  device_extension = (libusb_device_extension *)device_object->DeviceExtension;

  memset(device_extension, 0, sizeof(libusb_device_extension));

  device_extension->self = device_object;
  device_extension->physical_device_object = physical_device_object;
  device_extension->device_id = id;
      
  status = IoCreateSymbolicLink(&symbolic_link_name, &nt_device_name);
  
  if(!NT_SUCCESS(status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "add_device(): creating "
                   "symbolic link failed");
      IoDeleteDevice(device_object);
      release_device_id(id);
      return status;
    }

  clear_pipe_info(device_extension);

  device_extension->next_stack_device = 
    IoAttachDeviceToDeviceStack(device_object, physical_device_object);


  device_object->DeviceType = device_extension->next_stack_device->DeviceType;
  device_object->Characteristics =
    device_extension->next_stack_device->Characteristics;

  remove_lock_initialize(&device_extension->remove_lock);

  device_object->Flags |= device_extension->next_stack_device->Flags
    & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);

  device_object->Flags &= ~DO_DEVICE_INITIALIZING;

  return status;
}


VOID DDKAPI unload(DRIVER_OBJECT *driver_object)
{
  debug_printf(LIBUSB_DEBUG_MSG, "unload(): unloading driver");
}


NTSTATUS complete_irp(IRP *irp, NTSTATUS status, ULONG info)
{
  irp->IoStatus.Status = status;
  irp->IoStatus.Information = info;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  
  return status;
}

NTSTATUS call_usbd(libusb_device_extension *device_extension, void *urb, 
                   ULONG control_code, int timeout)
{
  IO_STATUS_BLOCK io_status;
  KEVENT event;
  NTSTATUS status = STATUS_SUCCESS;
  IRP *irp;
  IO_STACK_LOCATION *next_irp_stack;
  LARGE_INTEGER _timeout;

  KeInitializeEvent(&event, NotificationEvent, FALSE);

  irp = IoBuildDeviceIoControlRequest(control_code, 
                                      device_extension->physical_device_object,                                      
                                      NULL, 0, NULL, 0, TRUE, 
                                      NULL, &io_status);

  if(!irp)
    {
      return STATUS_INSUFFICIENT_RESOURCES;
    }

  next_irp_stack = IoGetNextIrpStackLocation(irp);
  next_irp_stack->Parameters.Others.Argument1 = urb;
  next_irp_stack->Parameters.Others.Argument2 = NULL;


  IoSetCompletionRoutine(irp, on_usbd_complete, &event, TRUE, TRUE, TRUE); 

  status = IoCallDriver(device_extension->physical_device_object, irp);
  

  if(status == STATUS_PENDING)
    {
      _timeout.QuadPart = -(timeout * 10000);
      
      if(timeout)
        {
          status = KeWaitForSingleObject(&event, Executive, KernelMode,
                                         FALSE, &_timeout);
        }
      else /* wait forever */
        {
          status = KeWaitForSingleObject(&event, Executive, KernelMode,
                                         FALSE, NULL);
        }

      if(status == STATUS_TIMEOUT)
        {
          debug_printf(LIBUSB_DEBUG_ERR, "call_usbd(): request timed out");
          IoCancelIrp(irp);
          KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
          status = STATUS_CANCELLED;
        }
    }

  KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
  IoCompleteRequest(irp, IO_NO_INCREMENT);

  return status;
}

NTSTATUS pass_irp_down(libusb_device_extension *device_extension, IRP *irp)
{
  IoSkipCurrentIrpStackLocation(irp);
  return IoCallDriver(device_extension->next_stack_device, irp);
}

BOOL is_irp_for_us(libusb_device_extension *device_extension, IRP *irp)
{
  if(irp->Tail.Overlay.OriginalFileObject)
    {
     return irp->Tail.Overlay.OriginalFileObject->DeviceObject
         == device_extension->self ? TRUE : FALSE;
    }
  return FALSE;
}

int get_pipe_handle(libusb_device_extension *device_extension, 
                    int endpoint_address, USBD_PIPE_HANDLE *pipe_handle)
{
  int i, j;

  *pipe_handle = NULL;

  for(i = 0; i < LIBUSB_MAX_NUMBER_OF_INTERFACES; i++)
    {
      for(j = 0; j < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; j++)
        {
          if(device_extension->interfaces[i].endpoints[j].address 
             == endpoint_address)
            {
              *pipe_handle 
                = device_extension->interfaces[i].endpoints[j].handle;

              if(!*pipe_handle)
                {
                  return FALSE;
                }
              else
                {
                  return TRUE;
                }
            }
        }
    }
  return FALSE;
}

void clear_pipe_info(libusb_device_extension *device_extension)
{
  int i, j;
  
  for(i = 0; i < LIBUSB_MAX_NUMBER_OF_INTERFACES; i++)
    {
      device_extension->interfaces[i].valid = FALSE;
      device_extension->interfaces[i].claimed = FALSE;

      for(j = 0; j < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; j++)
        {
          device_extension->interfaces[i].endpoints[j].address = -1;
          device_extension->interfaces[i].endpoints[j].handle = NULL;
        } 
    }
}

int update_pipe_info(libusb_device_extension *device_extension, int interface,
                     USBD_INTERFACE_INFORMATION *interface_info)
{
  int i;

  if(interface >= LIBUSB_MAX_NUMBER_OF_INTERFACES)
    {
      return FALSE;
    }

  debug_printf(LIBUSB_DEBUG_MSG, "update_pipe_info(): interface %d",
               interface);

  device_extension->interfaces[interface].valid = TRUE;
  
  for(i = 0; i < LIBUSB_MAX_NUMBER_OF_ENDPOINTS ; i++)
    {
      device_extension->interfaces[interface].endpoints[i].address = -1;
      device_extension->interfaces[interface].endpoints[i].handle = NULL;
    } 

  if(interface_info)
    {
      for(i = 0; i < (int)interface_info->NumberOfPipes
            && i < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; i++) 
        {
          debug_printf(LIBUSB_DEBUG_MSG, "update_pipe_info(): endpoint "
                       "address 0x%02x",
                       interface_info->Pipes[i].EndpointAddress);	  

          device_extension->interfaces[interface].endpoints[i].handle
            = interface_info->Pipes[i].PipeHandle;	
          device_extension->interfaces[interface].endpoints[i].address = 
            interface_info->Pipes[i].EndpointAddress;
        }
    }

  return TRUE;
}

void remove_lock_initialize(libusb_remove_lock_t *remove_lock)
{
  KeInitializeEvent(&remove_lock->event, NotificationEvent, FALSE);
  remove_lock->usage_count = 1;
  remove_lock->remove_pending = FALSE;
}


NTSTATUS remove_lock_acquire(libusb_remove_lock_t *remove_lock)
{
  InterlockedIncrement(&remove_lock->usage_count);

  if(remove_lock->remove_pending)
    {
      if(InterlockedDecrement(&remove_lock->usage_count) == 0)
        {
          KeSetEvent(&remove_lock->event, 0, FALSE);
        }      
      return STATUS_DELETE_PENDING;
    }
  return STATUS_SUCCESS;
}


void remove_lock_release(libusb_remove_lock_t *remove_lock)
{
  if(InterlockedDecrement(&remove_lock->usage_count) == 0)
    {
      KeSetEvent(&remove_lock->event, 0, FALSE);
    }
}


void remove_lock_release_and_wait(libusb_remove_lock_t *remove_lock)
{
  remove_lock->remove_pending = TRUE;
  remove_lock_release(remove_lock);
  remove_lock_release(remove_lock);
  KeWaitForSingleObject(&remove_lock->event, Executive, KernelMode,
                        FALSE, NULL);
}

int get_device_id(int *id)
{
  int ret = FALSE;
  int i;
  
  *id = -1;

  KeWaitForSingleObject(&device_ids.mutex, Executive, KernelMode,
                        FALSE, NULL);

  for(i = 0; i < LIBUSB_MAX_NUMBER_OF_DEVICES; i++)
    {
      if(!device_ids.is_used[i])
        {
          device_ids.is_used[i] = TRUE;
          *id = i;
          ret = TRUE;
          break;
        }
    }

  KeReleaseMutex(&device_ids.mutex, FALSE);
  return ret;
}

void release_device_id(int id)
{
  if(id >= 0)
    {
      KeWaitForSingleObject(&device_ids.mutex, Executive, KernelMode,
                            FALSE, NULL);
      
      if(id < LIBUSB_MAX_NUMBER_OF_DEVICES)
        {
          device_ids.is_used[id] = FALSE;
        }

      KeReleaseMutex(&device_ids.mutex, FALSE);
    }
}

static NTSTATUS DDKAPI on_usbd_complete(DEVICE_OBJECT *device_object, 
                                           IRP *irp, void *context)
{
  KeSetEvent((KEVENT *) context, IO_NO_INCREMENT, FALSE);
  return STATUS_MORE_PROCESSING_REQUIRED;
}
