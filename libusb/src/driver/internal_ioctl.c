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

NTSTATUS __stdcall
on_internal_ioctl_complete(DEVICE_OBJECT *device_object, IRP *irp,
                           void *context);
int block_internal_device_control(libusb_device_extension *device_extension,
                                  IRP *irp);


NTSTATUS dispatch_internal_ioctl(libusb_device_extension *device_extension, 
                                 IRP *irp)
{
  if(IoGetCurrentIrpStackLocation(irp)
     ->Parameters.DeviceIoControl.IoControlCode 
     == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
      if(block_internal_device_control(device_extension, irp))
        {
          return complete_irp(irp, STATUS_DEVICE_BUSY, 0);
        }
      
      IoCopyCurrentIrpStackLocationToNext(irp);
      IoSetCompletionRoutine(irp, on_internal_ioctl_complete, NULL, 
                             TRUE, TRUE, TRUE);
      return IoCallDriver(device_extension->next_stack_device, irp);
    }

  IoSkipCurrentIrpStackLocation(irp);
  return IoCallDriver(device_extension->next_stack_device, irp);
}

NTSTATUS __stdcall 
on_internal_ioctl_complete(DEVICE_OBJECT *device_object, IRP *irp,
                           void *context)
{
  libusb_device_extension *device_extension 
    = (libusb_device_extension *)device_object->DeviceExtension;
  URB *urb = URB_FROM_IRP(irp);
  USB_CONFIGURATION_DESCRIPTOR *configuration_descriptor = NULL;
  USBD_INTERFACE_INFORMATION *interface_info = NULL; 
  int i;

  if(irp->PendingReturned) 
    {
      IoMarkIrpPending(irp);
    }

  if(urb)
    {
      if(!USBD_SUCCESS(urb->UrbHeader.Status))
        { 
          return STATUS_SUCCESS;
        }

      switch(urb->UrbHeader.Function)
        {
        case URB_FUNCTION_SELECT_CONFIGURATION:
          configuration_descriptor 
            = urb->UrbSelectConfiguration.ConfigurationDescriptor;
          device_extension->configuration_handle =
            urb->UrbSelectConfiguration.ConfigurationHandle;

          if(configuration_descriptor)
            {
              device_extension->configuration = 
                configuration_descriptor->bConfigurationValue;
              interface_info = &(urb->UrbSelectConfiguration.Interface);
            }
          else
            {
              device_extension->configuration = 0;
              interface_info = NULL;
            }
	  
          if(interface_info)
            {
              clear_pipe_info(device_extension);

              for(i = 0; i < configuration_descriptor->bNumInterfaces; i++)
                {
                  update_pipe_info(device_extension, i, interface_info);
                  interface_info =  (USBD_INTERFACE_INFORMATION *)
                    (((char *)interface_info) + interface_info->Length); 
                }
            }
	     
          debug_printf(LIBUSB_DEBUG_MSG, "on_internal_ioctl_complete(): "
                       "current configuration is %d", 
                       device_extension->configuration);

          break;

        case URB_FUNCTION_SELECT_INTERFACE:

          interface_info = &(urb->UrbSelectInterface.Interface);

          if(interface_info)
            {
              debug_printf(LIBUSB_DEBUG_MSG, "on_internal_ioctl_complete(): "
                           "current alternate setting of interface %d is %d",
                           interface_info->InterfaceNumber,
                           interface_info->AlternateSetting);
            }

          update_pipe_info(device_extension, interface_info->InterfaceNumber, 
                           interface_info);
          break;

        default:
          ;
        }
    }

  return STATUS_SUCCESS;
}

int block_internal_device_control(libusb_device_extension *device_extension,
                                  IRP *irp)
{
  URB *urb = URB_FROM_IRP(irp);

  if(urb)
    {
      switch(urb->UrbHeader.Function)
        {
        case URB_FUNCTION_SELECT_CONFIGURATION:
          return is_any_interface_claimed(device_extension);
        case URB_FUNCTION_SELECT_INTERFACE:
          return is_interface_claimed(device_extension,
                                      urb->UrbSelectInterface.
                                      Interface.InterfaceNumber);
        case URB_FUNCTION_RESET_PIPE:
        case URB_FUNCTION_ABORT_PIPE:
          return is_endpoint_claimed(device_extension,
                                     urb->UrbPipeRequest.PipeHandle);
        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
          return is_endpoint_claimed(device_extension,
                                     urb->UrbBulkOrInterruptTransfer.
                                     PipeHandle);
        case URB_FUNCTION_ISOCH_TRANSFER:
          return is_endpoint_claimed(device_extension,
                                     urb->UrbIsochronousTransfer.PipeHandle);
        default:
          ;
        }
    }
  return FALSE;
}
