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


#define __LIBUSB_DRIVER_C__

#include "libusb_driver.h"

int bus_index;
mutex_t device_list_mutex;

static void DDKAPI unload(DRIVER_OBJECT *driver_object);
static NTSTATUS DDKAPI add_device(DRIVER_OBJECT *driver_object, 
                                  DEVICE_OBJECT *physical_device_object);

static NTSTATUS DDKAPI on_usbd_complete(DEVICE_OBJECT *device_object, 
                                        IRP *irp, 
                                        void *context);

NTSTATUS DDKAPI DriverEntry(DRIVER_OBJECT *driver_object,
                            UNICODE_STRING *registry_path)
{
  int i;

  DEBUG_MESSAGE("DriverEntry(): loading driver");

  /* initialize global variables */
  bus_index = 1;
  debug_level = LIBUSB_DEBUG_MSG;
  mutex_init(&device_list_mutex);

  /* initialize the driver object's dispatch table */
  for(i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) 
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
  ULONG device_type = FILE_DEVICE_UNKNOWN;

  UNICODE_STRING nt_device_name;
  UNICODE_STRING symbolic_link_name;
  WCHAR tmp_name_0[128];
  WCHAR tmp_name_1[128];
  int i;

  /* only attach the class filter to USB devices, and don't attach it to 
   composite device interfaces */
  if(!reg_is_usb_device(physical_device_object)
     || reg_is_composite_interface(physical_device_object))
    {
      return STATUS_SUCCESS;
    }

  /* retrieve the device type of the lower device object */
  device_object = IoGetAttachedDeviceReference(physical_device_object);
  device_type = device_object->DeviceType;
  ObDereferenceObject(device_object);
  

  /* try to create a new device object */
  for(i = 1; i < LIBUSB_MAX_NUMBER_OF_DEVICES; i++)
    {
      device_object = NULL;

      _snwprintf(tmp_name_0, sizeof(tmp_name_0)/sizeof(WCHAR), L"%s%04d", 
                 LIBUSB_NT_DEVICE_NAME, i);
      _snwprintf(tmp_name_1, sizeof(tmp_name_1)/sizeof(WCHAR), L"%s%04d", 
                 LIBUSB_SYMBOLIC_LINK_NAME, i);
    
      RtlInitUnicodeString(&nt_device_name, tmp_name_0);  
      RtlInitUnicodeString(&symbolic_link_name, tmp_name_1);

      mutex_lock(&device_list_mutex);

      status = IoCreateDevice(driver_object, 
                              sizeof(libusb_device_t), 
                              &nt_device_name, device_type, 0, FALSE, 
                              &device_object);

      mutex_release(&device_list_mutex);

      if(NT_SUCCESS(status))
        {
          DEBUG_MESSAGE("add_device(): device %d created", i);
          break;
        }
    }

  if(!device_object)
    {
      DEBUG_ERROR("add_device(): creating device failed");
      return status;
    }
      
  status = IoCreateSymbolicLink(&symbolic_link_name, &nt_device_name);
  
  if(!NT_SUCCESS(status))
    {
      DEBUG_ERROR("add_device(): creating symbolic link failed");
      IoDeleteDevice(device_object);
      return status;
    }

  dev = (libusb_device_t *)device_object->DeviceExtension;

  memset(dev, 0, sizeof(libusb_device_t));

  /* attach the newly created device object to the stack */
  dev->next_stack_device = 
    IoAttachDeviceToDeviceStack(device_object, physical_device_object);

  device_object->DeviceType = dev->next_stack_device->DeviceType;
  device_object->Characteristics = dev->next_stack_device->Characteristics;

  dev->self = device_object;
  dev->physical_device_object = physical_device_object;
  dev->id = i;
  dev->is_filter = reg_is_filter_driver(physical_device_object);

  if(dev->is_filter)
    {
      /* send all IRP's to the PDO in filter driver mode */
      dev->next_device = physical_device_object;
    }
  else
    {
      /* send all IRP's to the next stack device in device driver mode */
      dev->next_device = dev->next_stack_device;
    }

  clear_pipe_info(dev);

  remove_lock_initialize(&dev->remove_lock);

  device_object->Flags |= dev->next_stack_device->Flags
    & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);

  device_object->Flags &= ~DO_DEVICE_INITIALIZING;

  return status;
}


VOID DDKAPI unload(DRIVER_OBJECT *driver_object)
{
  DEBUG_MESSAGE("unload(): unloading driver");
}


NTSTATUS complete_irp(IRP *irp, NTSTATUS status, ULONG info)
{
  irp->IoStatus.Status = status;
  irp->IoStatus.Information = info;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  
  return status;
}

NTSTATUS call_usbd(libusb_device_t *dev, void *urb, 
                   ULONG control_code, int timeout)
{
  IO_STATUS_BLOCK io_status;
  KEVENT event;
  NTSTATUS status = STATUS_SUCCESS;
  IRP *irp;
  IO_STACK_LOCATION *next_irp_stack;
  LARGE_INTEGER _timeout;

  KeInitializeEvent(&event, NotificationEvent, FALSE);

  irp = IoBuildDeviceIoControlRequest(control_code, dev->next_device,
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

  status = IoCallDriver(dev->next_device, irp);
  

  if(status == STATUS_PENDING)
    {
      _timeout.QuadPart = -(timeout * 10000);
      
      status = KeWaitForSingleObject(&event, Executive, KernelMode,
                                     FALSE, &_timeout);
      
      if(status == STATUS_TIMEOUT)
        {
          DEBUG_ERROR("call_usbd(): request timed out");
          IoCancelIrp(irp);
          KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
          status = STATUS_CANCELLED;
        }
    }

  KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
  IoCompleteRequest(irp, IO_NO_INCREMENT);

  return status;
}


static NTSTATUS DDKAPI on_usbd_complete(DEVICE_OBJECT *device_object, 
                                        IRP *irp, void *context)
{
  KeSetEvent((KEVENT *) context, IO_NO_INCREMENT, FALSE);
  return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS pass_irp_down(libusb_device_t *dev, IRP *irp)
{
  IoSkipCurrentIrpStackLocation(irp);
  return IoCallDriver(dev->next_stack_device, irp);
}

BOOL accept_irp(libusb_device_t *dev, IRP *irp)
{
  if(irp->Tail.Overlay.OriginalFileObject)
    {
     return irp->Tail.Overlay.OriginalFileObject->DeviceObject
         == dev->self ? TRUE : FALSE;
    }
  return FALSE;
}

int get_pipe_handle(libusb_device_t *dev, int endpoint_address, 
                    USBD_PIPE_HANDLE *pipe_handle)
{
  int i, j;

  *pipe_handle = NULL;

  for(i = 0; i < LIBUSB_MAX_NUMBER_OF_INTERFACES; i++)
    {
      for(j = 0; j < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; j++)
        {
          if(dev->interfaces[i].endpoints[j].address == endpoint_address)
            {
              *pipe_handle = dev->interfaces[i].endpoints[j].handle;

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

void clear_pipe_info(libusb_device_t *dev)
{
  int i, j;
  
  for(i = 0; i < LIBUSB_MAX_NUMBER_OF_INTERFACES; i++)
    {
      dev->interfaces[i].valid = FALSE;
      dev->interfaces[i].claimed = FALSE;

      for(j = 0; j < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; j++)
        {
          dev->interfaces[i].endpoints[j].address = -1;
          dev->interfaces[i].endpoints[j].handle = NULL;
        } 
    }
}

int update_pipe_info(libusb_device_t *dev, int interface,
                     USBD_INTERFACE_INFORMATION *interface_info)
{
  int i;

  if(interface >= LIBUSB_MAX_NUMBER_OF_INTERFACES)
    {
      return FALSE;
    }

  DEBUG_MESSAGE("update_pipe_info(): interface %d", interface);

  dev->interfaces[interface].valid = TRUE;
  
  for(i = 0; i < LIBUSB_MAX_NUMBER_OF_ENDPOINTS ; i++)
    {
      dev->interfaces[interface].endpoints[i].address = -1;
      dev->interfaces[interface].endpoints[i].handle = NULL;
    } 

  if(interface_info)
    {
      for(i = 0; i < (int)interface_info->NumberOfPipes
            && i < LIBUSB_MAX_NUMBER_OF_ENDPOINTS; i++) 
        {
          DEBUG_MESSAGE("update_pipe_info(): endpoint address 0x%02x",
                        interface_info->Pipes[i].EndpointAddress);	  

          dev->interfaces[interface].endpoints[i].handle
            = interface_info->Pipes[i].PipeHandle;	
          dev->interfaces[interface].endpoints[i].address = 
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

void update_device_info(libusb_device_t *dev)
{
  int i;
  libusb_device_t *child_dev = NULL;

  dev->topology_info.num_children = 0;

  memset(&dev->topology_info.children, 0, sizeof(dev->topology_info.children));

  mutex_lock(&device_list_mutex);

  for(i = 0; i < dev->topology_info.num_child_pdos; i++)
    {
      child_dev = device_list_find(dev, dev->topology_info.child_pdos[i]);

      if(child_dev)
        {
          child_dev->topology_info.parent = dev->id;
          child_dev->topology_info.bus = dev->topology_info.bus;

          if(dev->topology_info.num_children < LIBUSB_MAX_NUMBER_OF_CHILDREN)
            {
              dev->topology_info.children[dev->topology_info.num_children].id 
                = child_dev->id;

              dev->topology_info.children[dev->topology_info.num_children].port
                = child_dev->topology_info.port;

              dev->topology_info.num_children++;
            }
        }
    }

  mutex_release(&device_list_mutex);
}

NTSTATUS get_device_info(libusb_device_t *dev, libusb_request *request, 
                         int *ret)
{
  int i;

  if(dev->topology_info.update_children)
    {
      update_device_info(dev);
      dev->topology_info.update_children = 0;
    }

  DEBUG_MESSAGE("get_device_info(): id: 0x%x", dev->id);
  DEBUG_MESSAGE("get_device_info(): port: 0x%x", dev->topology_info.port);
  DEBUG_MESSAGE("get_device_info(): parent: 0x%x", dev->topology_info.parent);
  DEBUG_MESSAGE("get_device_info(): bus: 0x%x", dev->topology_info.bus);
  DEBUG_MESSAGE("get_device_info(): num_children: 0x%x", 
                dev->topology_info.num_children);

  for(i = 0; i < dev->topology_info.num_children; i++)
    {
      DEBUG_MESSAGE("get_device_info(): child #%d: id: 0x%x, port: 0x%x",
                    i, 
                    dev->topology_info.children[i].id,
                    dev->topology_info.children[i].port);  
    }

  DEBUG_PRINT_NL();


  memset(request, 0, sizeof(libusb_request));

  request->device_info.port = dev->topology_info.port;
  request->device_info.parent_id = dev->topology_info.parent;
  request->device_info.bus = dev->topology_info.bus;
  request->device_info.id = dev->id;

  *ret = sizeof(libusb_request);

  return STATUS_SUCCESS;
}


libusb_device_t *device_list_find(libusb_device_t *dev, 
                                  DEVICE_OBJECT *physical_device_object)
{
  DEVICE_OBJECT *device_object;
  libusb_device_t *fdev = NULL;

  device_object = dev->self->DriverObject->DeviceObject;

  while(device_object)
    {
      fdev = (libusb_device_t *)device_object->DeviceExtension;

      if(fdev->physical_device_object == physical_device_object)
        {
          return fdev;
        }

      device_object = device_object->NextDevice;
    }

  return NULL;
}

void mutex_init(mutex_t *mutex)
{
  KeInitializeSpinLock(&mutex->lock);
}

void mutex_lock(mutex_t *mutex)
{
  KeAcquireSpinLock(&mutex->lock, &mutex->irq_level);
}

void mutex_release(mutex_t *mutex)
{
  KeReleaseSpinLock(&mutex->lock, mutex->irq_level);
}
