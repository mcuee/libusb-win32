/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
 * Copyright (c) 2000-2005 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h> 
#include <errno.h>
#include <ctype.h>
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>

#include "usb.h"
#include "error.h"
#include "usbi.h"
#include "driver/driver_api.h"
#include "registry.h"



#define LIBUSB_DEFAULT_TIMEOUT 5000
#define LIBUSB_DEVICE_NAME "\\\\.\\libusb0-"
#define LIBUSB_BUS_NAME "bus-"
#define LIBUSB_MAX_DEVICES 256

extern int __usb_debug;

typedef struct {
  usb_dev_handle *dev;
  libusb_request req;
  char *bytes;
  int size;
  DWORD control_code;
  OVERLAPPED ol;
} usb_context_t;


static struct usb_version _usb_version = {
  { LIBUSB_VERSION_MAJOR, 
    LIBUSB_VERSION_MINOR, 
    LIBUSB_VERSION_MICRO, 
    LIBUSB_VERSION_NANO },
  { -1, -1, -1, -1 }
};


static int usb_setup_async(usb_dev_handle *dev, void **context, 
                           DWORD control_code,
                           unsigned char ep, int pktsize);
static int usb_transfer_sync(usb_dev_handle *dev, int control_code,
                             int ep, int pktsize, char *bytes, int size, 
                             int timeout);

static int usb_get_configuration(usb_dev_handle *dev);
static int usb_cancel_io(usb_context_t *context);
static int usb_abort_ep(usb_dev_handle *dev, unsigned int ep);


/* DLL main entry point */
BOOL WINAPI DllMain(HANDLE module, DWORD reason, LPVOID reserved)
{
  switch(reason)
    {
    case DLL_PROCESS_ATTACH:
      break;
    case DLL_PROCESS_DETACH:
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    default:
      break;
    }
  return TRUE;
}

static int usb_get_configuration(usb_dev_handle *dev)
{
  int ret;
  char config;

  ret = usb_control_msg(dev, USB_RECIP_DEVICE | USB_ENDPOINT_IN, 
                        USB_REQ_GET_CONFIGURATION ,
                        0, 0, &config, 1, LIBUSB_DEFAULT_TIMEOUT);
  
  if(ret >= 0)
    {
      return config;
    }

  return ret;
}

int usb_os_open(usb_dev_handle *dev)
{
  char dev_name[LIBUSB_PATH_MAX];
  char *p;
  int config;

  dev->impl_info = INVALID_HANDLE_VALUE;
  dev->config = 0;
  dev->interface = -1;
  dev->altsetting = -1;

  if(!dev->device->filename)
    {
       usb_error("usb_os_open: invalid file name");
       return -ENOENT;
    }

  /* build the Windows file name from the unique device name */ 
  strcpy(dev_name, dev->device->filename);

  p = strstr(dev_name, "--");

  if(!p)
    {
      usb_error("usb_os_open: invalid file name %s", dev->device->filename);
      return -ENOENT;
    }
  
  *p = 0;

  dev->impl_info = CreateFile(dev_name, 0, 0, NULL, OPEN_EXISTING, 
                              FILE_FLAG_OVERLAPPED, NULL);
      
  if(dev->impl_info == INVALID_HANDLE_VALUE) 
    {
      usb_error("usb_os_open: failed to open %s: win error: %s",
                dev->device->filename, usb_win_error_to_string());
      return -ENOENT;
    }
  
  config = usb_get_configuration(dev);

  if(config > 0)
    {
      dev->config = config;
    }

  return 0;
}

int usb_os_close(usb_dev_handle *dev)
{
  if(dev->impl_info != INVALID_HANDLE_VALUE)
    {
      if(dev->interface >= 0)
        {
          usb_release_interface(dev, dev->interface);
        }

      CloseHandle(dev->impl_info);
      dev->impl_info = INVALID_HANDLE_VALUE;
      dev->interface = -1;
      dev->altsetting = -1;
    }

  return 0;
}

int usb_set_configuration(usb_dev_handle *dev, int configuration)
{
  DWORD sent;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      usb_error("usb_set_configuration: error: device not open");
      return -EINVAL;
    }

  req.configuration.configuration = configuration;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_CONFIGURATION, 
                      &req, sizeof(libusb_request), 
                      NULL, 0, &sent, NULL))
    {
      usb_error("usb_set_configuration: could not set config %d: "
                "win error: %s", configuration, usb_win_error_to_string());
      return -usb_win_error_to_errno();
    }
  
  dev->config = configuration;
  dev->interface = -1;
  dev->altsetting = -1;
  
  return 0;
}

int usb_claim_interface(usb_dev_handle *dev, int interface)
{
  DWORD ret;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
       usb_error("usb_claim_interface: device not open");
       return -EINVAL;
    }

  if(!dev->config)
    {
       usb_error("usb_claim_interface: could not claim interface %d, invalid "
                 "configuration %d", interface, dev->config);
       return -EINVAL;
    }
  
  if(interface >= dev->device->config[dev->config - 1].bNumInterfaces)
    {
      usb_error("usb_claim_interface: could not claim interface %d, "
                "interface is invalid", interface);
      return -EINVAL;
    }

  req.interface.interface = interface;

  if(!DeviceIoControl(dev->impl_info, 
                      LIBUSB_IOCTL_CLAIM_INTERFACE, 
                      &req, sizeof(libusb_request), 
                      NULL, 0, &ret, NULL))
    {
       usb_error("usb_claim_interface: could not claim interface %d, "
                 "win error: %s", interface, usb_win_error_to_string());
       return -usb_win_error_to_errno();
    }
  else
    {
       dev->interface = interface;
       dev->altsetting = 0;
       return 0;
    }
}

int usb_release_interface(usb_dev_handle *dev, int interface)
{
  DWORD ret;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      usb_error("usb_release_interface: device not open");
      return -EINVAL;
    }

  if(!dev->config)
    {
       usb_error("usb_release_interface: could not release interface %d, "
                 "invalid configuration %d", interface, dev->config);
       return -EINVAL;
    }

  req.interface.interface = interface;

  if(!DeviceIoControl(dev->impl_info, 
                      LIBUSB_IOCTL_RELEASE_INTERFACE, 
                      &req, sizeof(libusb_request), 
                      NULL, 0, &ret, NULL))
    {
       usb_error("usb_release_interface: could not release interface %d, "
                 "win error: %s", interface, usb_win_error_to_string());
       return -usb_win_error_to_errno();
    }
  else
    {
      dev->interface = -1;
      dev->altsetting = -1;
      
      return 0;
    }
}

int usb_set_altinterface(usb_dev_handle *dev, int alternate)
{
  DWORD sent;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
       usb_error("usb_set_altinterface: device not open");
       return -EINVAL;
    }

  if(dev->interface < 0)
    {
       usb_error("usb_set_altinterface: could not set alt interface %d/%d: "
                 "no interface claimed", dev->interface, alternate);
       return -EINVAL;
    }

  req.interface.interface = dev->interface;
  req.interface.altsetting = alternate;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;
  
  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_INTERFACE, 
                      &req, sizeof(libusb_request), 
                      NULL, 0, &sent, NULL))
    {
       usb_error("usb_set_altinterface: could not set alt interface "
                 "%d/%d: win error: %s",
                 dev->interface, alternate, usb_win_error_to_string());
       return -usb_win_error_to_errno();
    }
  
  dev->altsetting = alternate;

  return 0;
}

static int usb_setup_async(usb_dev_handle *dev, void **context, 
                           DWORD control_code,
                           unsigned char ep, int pktsize)
{
  usb_context_t **c = (usb_context_t **)context;
  
  if(((control_code == LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE)
      || (control_code == LIBUSB_IOCTL_ISOCHRONOUS_WRITE)) 
     && (ep & USB_ENDPOINT_IN))
    {
      usb_error("usb_setup_async: invalid endpoint 0x%02x", ep);
      return -EINVAL;
    }

  if(((control_code == LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ)
      || (control_code == LIBUSB_IOCTL_ISOCHRONOUS_READ))
     && !(ep & USB_ENDPOINT_IN))
    {
      usb_error("usb_setup_async: invalid endpoint 0x%02x", ep);
      return -EINVAL;
    }

  *c = malloc(sizeof(usb_context_t));
  
  if(!*c)
    {
      usb_error("usb_setup_async: memory allocation error");
      return -ENOMEM;
    }

  memset(*c, 0, sizeof(usb_context_t));

  (*c)->dev = dev;
  (*c)->req.endpoint.endpoint = ep;
  (*c)->req.endpoint.packet_size = pktsize;
  (*c)->control_code = control_code;

  (*c)->ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  if(!(*c)->ol.hEvent)
    {
       free(*c);
       *c = NULL;
       usb_error("usb_setup_async: creating event failed: win error: %s", 
                 usb_win_error_to_string());
       return -usb_win_error_to_errno();
    }

  return 0;
}

int usb_submit_async(void *context, char *bytes, int size)
{
  DWORD ret;
  usb_context_t *c = (usb_context_t *)context;

  if(!c)
    {
      usb_error("usb_submit_async: invalid context");
      return -EINVAL;
    }
    
  if(c->dev->impl_info == INVALID_HANDLE_VALUE)
    {
      usb_error("usb_submit_async: device not open");
      return -EINVAL;
    }

  if(c->dev->config <= 0)
    {
      usb_error("usb_submit_async: invalid configuration %d", c->dev->config);
      return -EINVAL;
    }

  if(c->dev->interface < 0)
    {
      usb_error("usb_submit_async: invalid interface %d", c->dev->interface);
      return -EINVAL;
    }
  
  
  c->ol.Offset = 0;
  c->ol.OffsetHigh = 0;
  c->bytes = bytes;
  c->size = size;

  ResetEvent(c->ol.hEvent);
  
  if(!DeviceIoControl(c->dev->impl_info, 
                      c->control_code, 
                      &c->req, sizeof(libusb_request), 
                      c->bytes, 
                      c->size, &ret, &c->ol))
    {
      if(GetLastError() != ERROR_IO_PENDING)
        {
          usb_error("usb_submit_async: submitting request failed, "
                    "win error: %s", usb_win_error_to_string());
           return -usb_win_error_to_errno();
        }
    }

  return ret;
}

int usb_reap_async(void *context, int timeout)

{
  usb_context_t *c = (usb_context_t *)context;
  ULONG ret = 0;
    
  if(!c)
    {
      usb_error("usb_reap_async: invalid context");
      return -EINVAL;
    }

  if(WaitForSingleObject(c->ol.hEvent, timeout) == WAIT_TIMEOUT)
    {
       /* request timed out */
       usb_cancel_io(c);
       usb_error("usb_reap_async: timeout error");
       return -ETIMEDOUT;
    }
  
  if(!GetOverlappedResult(c->dev->impl_info, &c->ol, &ret, TRUE))
    {
      usb_error("usb_reap_async: reaping request failed, win error: %s", 
                usb_win_error_to_string());
      return -usb_win_error_to_errno();
    }

  return ret;
}

int usb_reap_async_nocancel(void *context, int timeout)

{
  usb_context_t *c = (usb_context_t *)context;
  ULONG ret = 0;
    
  if(!c)
    {
      usb_error("usb_reap_async_nocancel: invalid context");
      return -EINVAL;
    }

  if(WaitForSingleObject(c->ol.hEvent, timeout) == WAIT_TIMEOUT)
    {
       /* request timed out */
       usb_cancel_io(c);
       usb_error("usb_reap_async_nocancel: timeout error");
       return -ETIMEDOUT;
    }
  
  if(!GetOverlappedResult(c->dev->impl_info, &c->ol, &ret, TRUE))
    {
      usb_error("usb_reap_async_nocancel: reaping request failed, "
                "win error: %s",  usb_win_error_to_string());
      return -usb_win_error_to_errno();
    }

  return ret;
}


int usb_cancel_async(void *context)
{
  /* NOTE that this function will cancel all pending URBs */
  /* on the same endpoint as this particular context, or even */
  /* all pending URBs for this particular device. */
  
  usb_context_t *c = (usb_context_t *)context;
  
  if(!c)
    {
       usb_error("usb_cancel_async: invalid context");
       return -EINVAL;
    }

  if(c->dev->impl_info == INVALID_HANDLE_VALUE)
    {
       usb_error("usb_cancel_async: device not open");
       return -EINVAL;
    }
  
  usb_cancel_io(c);

  return 0;
}

int usb_free_async(void **context)
{
  usb_context_t **c = (usb_context_t **)context;

  if(!*c)
    {
       usb_error("usb_free_async: invalid context");
       return -EINVAL;
    }

  CloseHandle((*c)->ol.hEvent);

  free(*c);
  *c = NULL;

  return 0;
}

static int usb_transfer_sync(usb_dev_handle *dev, int control_code,
                             int ep, int pktsize, char *bytes, int size,
                             int timeout)
{
  void *context = NULL;
  int transmitted = 0;
  int ret;
  int requested;

  ret = usb_setup_async(dev, &context, control_code, ep, pktsize);

  if(ret < 0)
    {
      return ret;
    }

  do {
    requested = size > LIBUSB_MAX_READ_WRITE ? LIBUSB_MAX_READ_WRITE : size;

    ret = usb_submit_async(context, bytes, requested);
    
    if(ret < 0)
      {
        transmitted = ret;
        break;
      }

    ret = usb_reap_async(context, timeout);

    if(ret < 0)
      {
        transmitted = ret;
        break;
      }

    transmitted += ret;
    bytes += ret;
    size -= ret;
  } while(size > 0 && ret == requested);
  
  usb_free_async(&context);

  return transmitted;
}

int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
                   int timeout)
{
  return usb_transfer_sync(dev, LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE,
                           ep, 0, bytes, size, timeout);
}

int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size,
                  int timeout)
{
  return usb_transfer_sync(dev, LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ,
                           ep, 0, bytes, size, timeout);
}

int usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, int size,
                        int timeout)
{
  return usb_transfer_sync(dev, LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE,
                           ep, 0, bytes, size, timeout);
}

int usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, int size,
                       int timeout)
{
  return usb_transfer_sync(dev, LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ,
                           ep, 0, bytes, size, timeout);
}

int usb_isochronous_setup_async(usb_dev_handle *dev, void **context, 
                                unsigned char ep, int pktsize)
{
  if(ep & 0x80)
    return usb_setup_async(dev, context, LIBUSB_IOCTL_ISOCHRONOUS_READ,
                           ep, pktsize);
  else
    return usb_setup_async(dev, context, LIBUSB_IOCTL_ISOCHRONOUS_WRITE,
                           ep, pktsize);    
}

int usb_bulk_setup_async(usb_dev_handle *dev, void **context, unsigned char ep)
{
  if(ep & 0x80)
    return usb_setup_async(dev, context, LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ,
                           ep, 0);
  else
    return usb_setup_async(dev, context, LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE,
                           ep, 0);    
}

int usb_interrupt_setup_async(usb_dev_handle *dev, void **context, 
                              unsigned char ep)
{
  if(ep & 0x80)
    return usb_setup_async(dev, context, LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ,
                           ep, 0);
  else
    return usb_setup_async(dev, context, LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE,
                           ep, 0);    
}

int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
                    int value, int index, char *bytes, int size, int timeout)
{
  DWORD ret = 0;
  int error = 0;
  char *tmp = NULL;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      usb_error("usb_control_msg: device not open");
      return -EINVAL;
    }

  req.timeout = timeout;

  /* windows doesn't support generic control messages, so it needs to be */
  /* split up */ 
  switch(requesttype & (0x03 << 5))
    {
    case USB_TYPE_STANDARD:      
      switch(request)
        {
        case USB_REQ_GET_STATUS: 
          req.status.recipient = requesttype & 0x1F;
          req.status.index = index;
	  
          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_STATUS, 
                              &req, sizeof(libusb_request), 
                              bytes, size, &ret, NULL))
            {
              error = 1;
              break;
            }

          break;
      
        case USB_REQ_CLEAR_FEATURE:
          req.feature.recipient = requesttype & 0x1F;
          req.feature.feature = value;
          req.feature.index = index;

          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_CLEAR_FEATURE, 
                              &req, sizeof(libusb_request), 
                              NULL, 0, &ret, NULL))
            {
              error = 1;
            }

          break;
	  
        case USB_REQ_SET_FEATURE:
          req.feature.recipient = requesttype & 0x1F;
          req.feature.feature = value;
          req.feature.index = index;
	  
          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_FEATURE, 
                              &req, sizeof(libusb_request), 
                              NULL, 0, &ret, NULL))
            {
              error = 1;
            }

          break;

        case USB_REQ_GET_DESCRIPTOR:     	  
          req.descriptor.type = (value >> 8) & 0xFF;
          req.descriptor.index = value & 0xFF;
          req.descriptor.language_id = index;
	  
          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_DESCRIPTOR, 
                              &req, sizeof(libusb_request), 
                              bytes, size, &ret, NULL))
            {
              error = 1;
            }
          break;
	  
        case USB_REQ_SET_DESCRIPTOR:
          req.descriptor.type = (value >> 8) & 0xFF;
          req.descriptor.index = value & 0xFF;
          req.descriptor.language_id = index;
	  
          tmp = malloc(sizeof(libusb_request) + size);

          if(!tmp)
            {
              usb_error("usb_control_msg: memory allocation failed");
              return -ENOMEM;
            }

          memcpy(tmp, &req, sizeof(libusb_request));
          memcpy(tmp + sizeof(libusb_request), bytes, size);


          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_DESCRIPTOR, 
                              tmp, sizeof(libusb_request) + size, 
                              NULL, 0, &ret, NULL))
            {
              error = 1;
            }

          free(tmp);

          break;
	  
        case USB_REQ_GET_CONFIGURATION:

          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_CONFIGURATION, 
                              &req, sizeof(libusb_request), 
                              bytes, size, &ret, NULL))
            {
              error = 1;
              break;
            }
	  
          break;
      
        case USB_REQ_SET_CONFIGURATION:	  
          req.configuration.configuration = value;

          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_CONFIGURATION, 
                              &req, sizeof(libusb_request), 
                              NULL, 0, &ret, NULL))
            {
              error = 1;
            }

          break;
	  
        case USB_REQ_GET_INTERFACE:
          req.interface.interface = index;
	  
          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_INTERFACE, 
                              &req, sizeof(libusb_request),
                              bytes, 1, &ret, NULL))
            {
              error = 1;
              break;
            }
	  
          break;
      
        case USB_REQ_SET_INTERFACE:
          req.interface.interface = index;
          req.interface.altsetting = value;
	  
          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_INTERFACE, 
                              &req, sizeof(libusb_request), 
                              NULL, 0, &ret, NULL))
            {
              error = 1;
            }
      
          break;
	  
        default:
          usb_error("usb_control_msg: invalid request 0x%x", request);
          return -EINVAL;
        }
      break;

    case USB_TYPE_VENDOR:  
    case USB_TYPE_CLASS:

      req.vendor.type = (requesttype >> 5) & 0x03;
      req.vendor.recipient = requesttype & 0x1F;
      req.vendor.request = request;
      req.vendor.value = value;
      req.vendor.index = index;

      if(requesttype & 0x80)
        {
          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_VENDOR_READ, 
                              &req, sizeof(libusb_request), 
                              bytes, size, &ret, NULL))
            {
              error = 1;
            }
        }
      else
        {
          tmp = malloc(sizeof(libusb_request) + size);

          if(!tmp)
            {
              usb_error("usb_control_msg: memory allocation failed"); 
              return -ENOMEM;
            }

          memcpy(tmp, &req, sizeof(libusb_request));
          memcpy(tmp + sizeof(libusb_request), bytes, size);

          if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_VENDOR_WRITE, 
                              tmp, sizeof(libusb_request) + size, 
                              NULL, 0, &ret, NULL))
            {
              error = 1;
            }

          free(tmp);
        }
      break;
    case USB_TYPE_RESERVED:
    default:
      usb_error("usb_control_msg: invalid or unsupported request type: %x", 
                requesttype);
      return -EINVAL;
    }
  
  if(error)
    {
      usb_error("usb_control_msg: sending control message failed, "
                "win error: %s", usb_win_error_to_string());
      return -usb_win_error_to_errno();
    }

  return ret;
}


int usb_os_find_busses(struct usb_bus **busses)
{
  struct usb_bus *fbus = NULL;
  struct usb_bus *bus = NULL;
  int num_busses;
  int i;

  num_busses = usb_registry_get_num_busses();

  for(i = 1; i <= num_busses; i++)
    {
      bus = malloc(sizeof(struct usb_bus));

      if(!bus)
        {
          usb_error("usb_os_find_busses: memory allocation failed");
          return -ENOMEM;
        }

      memset(bus, 0, sizeof(*bus));
      sprintf(bus->dirname, "%s%d", LIBUSB_BUS_NAME, i);
      
      bus->location = i;

      usb_message("usb_os_find_busses: found %s", bus->dirname);

      LIST_ADD(fbus, bus);
    }
  
  *busses = fbus;

  return 0;
}

int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices)
{
  int i;
  int num_busses;
  struct usb_device *dev, *fdev = NULL;
  char dev_name[LIBUSB_PATH_MAX];
  DWORD ret;
  HANDLE handle;
  libusb_request req;

  num_busses = usb_registry_get_num_busses();

  for(i = 1; i < LIBUSB_MAX_DEVICES; i++)
    {
      ret = 0;

      _snprintf(dev_name, sizeof(dev_name) - 1,"%s%04d", 
                LIBUSB_DEVICE_NAME, i);

      dev = malloc(sizeof(*dev));
      
      if(!dev) 
        {
          usb_error("usb_os_find_devices: memory allocation failed");
          return -ENOMEM;
        }
      
      memset(dev, 0, sizeof(*dev));
      dev->bus = bus;
      dev->devnum = (unsigned char)i;

      handle = CreateFile(dev_name, 0, 0, NULL, OPEN_EXISTING, 
                          FILE_ATTRIBUTE_NORMAL, NULL);

      if(handle == INVALID_HANDLE_VALUE) 
        {
          free(dev);
          continue;
        }

      if(!DeviceIoControl(handle, LIBUSB_IOCTL_GET_DEVICE_INFO, 
                          &req, sizeof(libusb_request), 
                          &req, sizeof(libusb_request), 
                          &ret, NULL))
        {
          usb_error("usb_os_find_devices: getting device info failed");
          CloseHandle(handle);
          free(dev);
          continue;
        }

      /* make sure that the bus info is valid, otherwise connect this device */
      /* to bus 1 */
      if((req.device_info.bus > num_busses)
         || !req.device_info.bus)
        {
          req.device_info.bus = 1;
        }

      if(req.device_info.bus != bus->location)
        {
          CloseHandle(handle);
          free(dev);
          continue;
        }

      req.descriptor.type = USB_DT_DEVICE;
      req.descriptor.index = 0;
      req.descriptor.language_id = 0;
      req.timeout = LIBUSB_DEFAULT_TIMEOUT;
      
      DeviceIoControl(handle, LIBUSB_IOCTL_GET_DESCRIPTOR, 
                      &req, sizeof(libusb_request), 
                      &dev->descriptor, USB_DT_DEVICE_SIZE, &ret, NULL);
      
      if(ret < USB_DT_DEVICE_SIZE) 
        {
          usb_error("usb_os_find_devices: couldn't read device descriptor");
          free(dev);
          CloseHandle(handle);
          continue;
        }
      
      _snprintf(dev->filename, LIBUSB_PATH_MAX - 1, "%s--0x%04x-0x%04x", 
                dev_name, dev->descriptor.idVendor, dev->descriptor.idProduct);

      CloseHandle(handle);

      LIST_ADD(fdev, dev);

      usb_message("usb_os_find_devices: found %s on %s",
                  dev->filename, bus->dirname);
    }
  
  *devices = fdev;

  return 0;
}


void usb_os_init(void)
{
  DWORD ret;
  HANDLE dev;
  libusb_request req;
  int i;
  char dev_name[LIBUSB_PATH_MAX];

  usb_message("usb_os_init: dll version: %d.%d.%d.%d",
              LIBUSB_VERSION_MAJOR, LIBUSB_VERSION_MINOR,
              LIBUSB_VERSION_MICRO, LIBUSB_VERSION_NANO);


  for(i = 1; i < LIBUSB_MAX_DEVICES; i++)
    {
      /* build the Windows file name */
      _snprintf(dev_name, sizeof(dev_name) - 1,"%s%04d", 
                LIBUSB_DEVICE_NAME, i);

      dev = CreateFile(dev_name, 0, 0, NULL, OPEN_EXISTING, 
                       FILE_FLAG_OVERLAPPED, NULL);
  
      if(dev == INVALID_HANDLE_VALUE) 
        {
          continue;
        }
      
      if(!DeviceIoControl(dev, LIBUSB_IOCTL_GET_VERSION, 
                          &req, sizeof(libusb_request), 
                          &req, sizeof(libusb_request), 
                          &ret, NULL))
        {
          usb_error("usb_os_init: getting driver version failed");
          CloseHandle(dev);
          continue;
        }
      else 
        {
          _usb_version.driver.major = req.version.major;
          _usb_version.driver.minor = req.version.minor;
          _usb_version.driver.micro = req.version.micro;
          _usb_version.driver.nano = req.version.nano;
	  
          usb_message("usb_os_init: driver version: %d.%d.%d.%d",
                          req.version.major, req.version.minor, 
                          req.version.micro, req.version.nano);
      
          /* set debug level */
          req.timeout = 0;
          req.debug.level = __usb_debug;
          
          if(!DeviceIoControl(dev, LIBUSB_IOCTL_SET_DEBUG_LEVEL, 
                              &req, sizeof(libusb_request), 
                              NULL, 0, &ret, NULL))
            {
              usb_error("usb_os_init: setting debug level failed");
            }
          
          CloseHandle(dev);
          break;
        }
    }
}


int usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
  DWORD ret;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      usb_error("usb_resetep: device not open");
      return -EINVAL;
    }

  req.endpoint.endpoint = (int)ep;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_ABORT_ENDPOINT, &req, 
                      sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      usb_error("usb_resetep: could not abort ep 0x%02x, win error: %s", 
                ep, usb_win_error_to_string());
       return -usb_win_error_to_errno();
    }

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_ENDPOINT, &req, 
                      sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      usb_error("usb_resetep: could not reset ep 0x%02x, win error: %s", 
                ep, usb_win_error_to_string());
      return -usb_win_error_to_errno();
    }
  
  return 0;
}

int usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
  DWORD ret;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      usb_error("usb_clear_halt: device not open");
      return -EINVAL;
    }

  req.endpoint.endpoint = (int)ep;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_ENDPOINT, &req, 
                      sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      usb_error("usb_clear_halt: could not clear halt, ep 0x%02x, "
                "win error: %s", ep, usb_win_error_to_string());
       return -usb_win_error_to_errno();
    }
  
  return 0;
}

int usb_reset(usb_dev_handle *dev)
{
  DWORD ret;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      usb_error("usb_reset: device not open");
      return -EINVAL;
    }

  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_DEVICE,
                      &req, sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      usb_error("usb_reset: could not reset device, win error: %s", 
                usb_win_error_to_string());
      return -usb_win_error_to_errno();
    }

  return 0;
}

const struct usb_version *usb_get_version(void)
{
  return &_usb_version;
}

void usb_set_debug(int level)
{
  DWORD ret;
  HANDLE dev;
  libusb_request req;
  int i;
  char dev_name[LIBUSB_PATH_MAX];

  if(__usb_debug || level)
    fprintf(stderr, "usb_set_debug: setting debugging level to %d (%s)\n",
            level, level ? "on" : "off");

  printf("usb_set_debug: setting debugging level to %d (%s)\n",
         level, level ? "on" : "off");

  __usb_debug = level;

  /* find a valid device */
  for(i = 1; i < LIBUSB_MAX_DEVICES; i++)
    {
      /* build the Windows file name */
      _snprintf(dev_name, sizeof(dev_name) - 1,"%s%04d", 
                LIBUSB_DEVICE_NAME, i);

      dev = CreateFile(dev_name, 0, 0, NULL, OPEN_EXISTING, 
                       FILE_FLAG_OVERLAPPED, NULL);
  
      if(dev == INVALID_HANDLE_VALUE) 
        {
          continue;
        }
      
      /* set debug level */
      req.timeout = 0;
      req.debug.level = __usb_debug;
      
      if(!DeviceIoControl(dev, LIBUSB_IOCTL_SET_DEBUG_LEVEL, 
                          &req, sizeof(libusb_request), 
                          NULL, 0, &ret, NULL))
        {
          usb_error("usb_os_init: setting debug level failed");
        }
      
      CloseHandle(dev);

      break;
    }
}

int usb_os_determine_children(struct usb_bus *bus)
{
  libusb_request req;
  struct usb_device *dev;
  int i = 0, j = 0, dev_count = 0;
  DWORD ret;
  usb_dev_handle *hdev;

  struct {
    struct usb_device *device;
    unsigned long id;
    unsigned long parent_id;
    int child_index;
  } dev_info[256];

  memset(dev_info, 0, sizeof(dev_info));

  /* get device info for all devices */
  for(dev = bus->devices; dev; dev = dev->next, i++)
    {
      hdev = usb_open(dev);

      if(!hdev)
        continue;

      if(!DeviceIoControl(hdev->impl_info, LIBUSB_IOCTL_GET_DEVICE_INFO, 
                          &req, sizeof(libusb_request), 
                          &req, sizeof(libusb_request), 
                          &ret, NULL))
        {
          usb_close(hdev);
          continue;
        }

      dev_info[i].device = dev;
      dev_info[i].id = req.device_info.id;
      dev_info[i].parent_id = req.device_info.parent_id;
      dev_info[i].device->num_children = 0;
      dev_info[i].child_index = 0;
      
      dev_count++;

      usb_close(hdev);
    }

  /* determine the number of children */
  for(i = 0; i < dev_count; i++)
    {
      for(j = 0; j < dev_count; j++)
        {
          if(dev_info[i].id == dev_info[j].parent_id)
            dev_info[i].device->num_children++;
        }
    }

  /* allocate memory for child device pointers */
  for(i = 0; i < dev_count; i++)
    {
      if(dev_info[i].device->num_children)
        dev_info[i].device->children = malloc(dev_info[i].device->num_children
                                              * sizeof(struct usb_device *));
    }

  /* find the children */
  for(i = 0; i < dev_count; i++)
    {
      for(j = 0; j < dev_count; j++)
        {
          if(dev_info[i].id == dev_info[j].parent_id)
            {
              dev_info[i].device->children[dev_info[i].child_index++]
                = dev_info[j].device;
            }
        }
    }


  /* search for the root device */
  for(i = 0; i < dev_count; i++)
    {
      if(!dev_info[i].parent_id)
        {
          bus->root_dev = dev_info[i].device;
          break;
        }
    }

  return 0;
}

static int usb_cancel_io(usb_context_t *context)
{
  return usb_abort_ep(context->dev, context->req.endpoint.endpoint);
  
  //return CancelIo(context->dev->impl_info) ? 0 : -1;
}

static int usb_abort_ep(usb_dev_handle *dev, unsigned int ep)
{
  DWORD ret;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
       usb_error("usb_abort_ep: device not open");
       return -EINVAL;
    }

  req.endpoint.endpoint = (int)ep;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_ABORT_ENDPOINT, &req, 
                      sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      usb_error("usb_abort_ep: could not abort ep 0x%02x, win error: %s", 
                ep, usb_win_error_to_string());
       return -usb_win_error_to_errno();
    }
  
  return 0;
}
