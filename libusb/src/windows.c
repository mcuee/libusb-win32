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
#include "driver_api.h"
#include "registry.h"



#define LIBUSB_DEFAULT_TIMEOUT 5000
#define LIBUSB_DEVICE_NAME "\\\\.\\libusb0-"
#define LIBUSB_BUS_NAME "bus-"
#define LIBUSB_MAX_DEVICES 256

#undef USB_ERROR_STR

/* Connection timed out */
#define ETIMEDOUT 116	


#define USB_ERROR_STR(x, format, args...) \
	do { \
	  usb_error_type = USB_ERROR_TYPE_STRING; \
          snprintf(usb_error_str, sizeof(usb_error_str) - 1, format, ## args); \
          if(usb_debug) \
	    { \
		fprintf(stderr, "LIBUSB_DLL error: %s\n", usb_error_str); \
	        output_debug_string("LIBUSB_DLL error: %s\n", \
				    usb_error_str); \
	    } \
	  return x; \
	} while (0)

#define USB_MESSAGE_STR(level, format, args...) \
	do { \
	  usb_error_type = USB_ERROR_TYPE_STRING; \
          snprintf(usb_error_str, sizeof(usb_error_str) - 1, format, ## args); \
          if(level <= usb_debug) \
	    { \
		fprintf(stderr, "LIBUSB_DLL: %s\n", usb_error_str); \
	        output_debug_string("LIBUSB_DLL: %s\n", usb_error_str); \
	    } \
	} while (0)


typedef struct {
  usb_dev_handle *dev;
  libusb_request req;
  char *bytes;
  int size;
  DWORD control_code;
  OVERLAPPED ol;
} usb_context;


static struct usb_version _usb_version = {
  { VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO },
  { -1, -1, -1, -1 }
};

static const char *win_error_to_string(void);
static int win_error_to_errno(void);
static void output_debug_string(const char *s, ...);

static int usb_setup_async(usb_dev_handle *dev, void **context, 
                           DWORD control_code,
                           unsigned char ep, int pktsize);
static int usb_transfer_sync(usb_dev_handle *dev, int control_code,
                             int ep, int pktsize, char *bytes, int size, 
                             int timeout);


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

/* prints a message to the Windows debug system */
static void output_debug_string(const char *s, ...)
{
  char tmp[512];
  va_list args;
  va_start(args, s);
  vsnprintf(tmp, sizeof(tmp) - 1, s, args);
  va_end(args);
  OutputDebugStringA(tmp);
}

/* returns Windows' last error in a human readable form */
static const char *win_error_to_string(void)
{
  static char error_buf[512];

  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 
                LANG_USER_DEFAULT, error_buf, 
                sizeof(error_buf) - 1, NULL);

  return error_buf;
}


static int win_error_to_errno(void)
{
  switch(GetLastError())
    {
    case ERROR_SUCCESS:
      return 0;
    case ERROR_INVALID_PARAMETER:
      return EINVAL;
    case ERROR_SEM_TIMEOUT: 
    case ERROR_OPERATION_ABORTED:
      return ETIMEDOUT;
    case ERROR_NOT_ENOUGH_MEMORY:
      return ENOMEM;
    default:
      return EIO;
    }
}

int usb_os_open(usb_dev_handle *dev)
{
  char dev_name[LIBUSB_PATH_MAX];
  char *p;

  dev->impl_info = INVALID_HANDLE_VALUE;

  if(!dev->device->filename)
    {
      USB_ERROR_STR(-ENOENT, "usb_os_open: invalid file name");
    }

  /* build the Windows file name from the unique device name */ 
  strcpy(dev_name, dev->device->filename);

  p = strstr(dev_name, "--");

  if(!p)
    {
      USB_ERROR_STR(-ENOENT, "usb_os_open: invalid file name %s",
                    dev->device->filename);
    }
  
  *p = 0;

  dev->impl_info = CreateFile(dev_name, 0, 0, NULL, OPEN_EXISTING, 
                              FILE_FLAG_OVERLAPPED, NULL);
      
  if(dev->impl_info == INVALID_HANDLE_VALUE) 
    {
      USB_ERROR_STR(-ENOENT, "usb_os_open: failed to open %s: win error: %s",
                    dev->device->filename, win_error_to_string());
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
      USB_ERROR_STR(-EINVAL, "usb_set_configuration: error: device not open");
    }

  req.configuration.configuration = configuration;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_CONFIGURATION, 
                      &req, sizeof(libusb_request), 
                      NULL, 0, &sent, NULL))
    {
      USB_ERROR_STR(-win_error_to_errno(), "could not set config %d: win "
                    "error: %s", configuration, win_error_to_string());
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
      USB_ERROR_STR(-EINVAL, "usb_claim_interface: error: device not open");
    }

  if(!dev->config)
    {
      USB_ERROR_STR(-EINVAL, "could not claim interface %d, invalid "
                    "configuration %d", interface, dev->config);
    }
  
  if(interface >= dev->device->config[dev->config - 1].bNumInterfaces)
    {
      USB_ERROR_STR(-EINVAL, "could not claim interface %d, interface "
                    "invalid", interface);
    }

  req.interface.interface = interface;

  if(!DeviceIoControl(dev->impl_info, 
                      LIBUSB_IOCTL_CLAIM_INTERFACE, 
                      &req, sizeof(libusb_request), 
                      NULL, 0, &ret, NULL))
    {
      USB_ERROR_STR(-win_error_to_errno(), "could not claim interface %d, "
                    "error: %s", interface, win_error_to_string());
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
      USB_ERROR_STR(-EINVAL, "usb_release_interface: error: device not open");
    }

  if(!dev->config)
    {
      USB_ERROR_STR(-EINVAL, "could not release interface %d, invalid "
                    "configuration %d",
                    interface, dev->config);
    }

  if((interface >= dev->device->config[dev->config - 1].bNumInterfaces)
     || (dev->interface != interface))
    {
      USB_ERROR_STR(-EINVAL, "could not release interface %d, interface "
                    "invalid", interface);
    }

  req.interface.interface = interface;

  if(!DeviceIoControl(dev->impl_info, 
                      LIBUSB_IOCTL_RELEASE_INTERFACE, 
                      &req, sizeof(libusb_request), 
                      NULL, 0, &ret, NULL))
    {
      USB_ERROR_STR(-win_error_to_errno(), "could not release interface %d, "
                    "error: %s", interface, win_error_to_string());
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
      USB_ERROR_STR(-EINVAL, "usb_set_altinterface: error: device not open");
    }

  if(dev->interface < 0)
    {
      USB_ERROR_STR(-EINVAL, "could not set alt interface %d/%d: no interface "
                    "claimed", dev->interface, alternate);
    }

  req.interface.interface = dev->interface;
  req.interface.altsetting = alternate;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;
  
  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_INTERFACE, 
                      &req, sizeof(libusb_request), 
                      NULL, 0, &sent, NULL))
    {
      USB_ERROR_STR(-win_error_to_errno(), "could not set alt interface "
                    "%d/%d: win error: %s",
                    dev->interface, alternate, win_error_to_string());
    }
  
  dev->altsetting = alternate;

  return 0;
}

static int usb_setup_async(usb_dev_handle *dev, void **context, 
                           DWORD control_code,
                           unsigned char ep, int pktsize)
{
  usb_context **c = (usb_context **)context;
  
  if(((control_code == LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE)
      || (control_code == LIBUSB_IOCTL_ISOCHRONOUS_WRITE)) 
     && (ep & USB_ENDPOINT_IN))
    {
      USB_ERROR_STR(-EINVAL, "usb_setup_async: error: "
                    "invalid endpoint 0x%02x", ep);
    }

  if(((control_code == LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ)
      || (control_code == LIBUSB_IOCTL_ISOCHRONOUS_READ))
     && !(ep & USB_ENDPOINT_IN))
    {
      USB_ERROR_STR(-EINVAL, "usb_setup_async: error: "
                    "invalid endpoint 0x%02x", ep);
    }

  *c = malloc(sizeof(usb_context));
  
  if(!*c)
    {
      USB_ERROR_STR(-EINVAL, "usb_setup_async: memory allocation "
                    "error");
    }

  memset(*c, 0, sizeof(usb_context));

  (*c)->dev = dev;
  (*c)->req.endpoint.endpoint = ep;
  (*c)->req.endpoint.packet_size = pktsize;
  (*c)->control_code = control_code;

  (*c)->ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  if(!(*c)->ol.hEvent)
    {
      free(*c);
      *c = NULL;
      USB_ERROR_STR(-win_error_to_errno(), "usb_setup_async: "
                    "creating event failed: win error: %s", 
                    win_error_to_string());
    }

  return 0;
}

int usb_submit_async(void *context, char *bytes, int size)
{
  DWORD ret;
  usb_context *c = (usb_context *)context;

  if(!c)
    {
      USB_ERROR_STR(-EINVAL, "usb_submit_async: error: "
                    "invalid context");
    }
    
  if(c->dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-EINVAL, "usb_submit_async: error: "
                    "device not open");
    }

  if(c->dev->config <= 0)
    {
      USB_ERROR_STR(-EINVAL, "usb_submit_async: error: "
                    "invalid configuration %d", c->dev->config);
    }

  if(c->dev->interface < 0)
    {
      USB_ERROR_STR(-EINVAL, "usb_submit_async: error: "
                    "invalid interface %d", c->dev->interface);
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
          USB_ERROR_STR(-win_error_to_errno(), "usb_submit_async: "
                        "error: %s", win_error_to_string());
        }
    }

  return ret;
}

int usb_reap_async(void *context, int timeout)

{
  usb_context *c = (usb_context *)context;
  ULONG ret = 0;
  timeout = timeout ? timeout : INFINITE;
    
  if(!c)
    {
      USB_ERROR_STR(-EINVAL, "usb_reap_async: error: "
                    "invalid context");
    }

  if(WaitForSingleObject(c->ol.hEvent, timeout) == WAIT_TIMEOUT)
    {
      /* request timed out */
      CancelIo(c->dev->impl_info);
      USB_ERROR_STR(-ETIMEDOUT, "usb_reap_async: timeout error");
    }
  
  if(!GetOverlappedResult(c->dev->impl_info, &c->ol, &ret, TRUE))
    {
      USB_ERROR_STR(-win_error_to_errno(), "usb_reap_async: error: "
                    "%s", win_error_to_string());
    }

  return ret;
}

int usb_free_async(void **context)
{
  usb_context **c = (usb_context **)context;

  if(!*c)
    {
      USB_ERROR_STR(-EINVAL, "usb_free_async: error: "
                    "invalid context");
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
      USB_ERROR_STR(-EINVAL, "usb_control_msg: error: device not open");
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
              USB_ERROR(-ENOMEM);
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
          USB_ERROR_STR(-EINVAL, "usb_control_msg: invalid request 0x%x",
                        request);
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
              USB_ERROR(-ENOMEM);
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
      USB_ERROR_STR(-EINVAL, "usb_control_msg: invalid or unsupported request"
                    " type: %x", requesttype);
    }
  
  if(error)
    {
      USB_ERROR_STR(-win_error_to_errno(), "error sending control message: "
                    "win error: %s", win_error_to_string());
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

  for(i = 0; i < num_busses; i++)
    {
      bus = malloc(sizeof(struct usb_bus));

      if(!bus)
        {
          USB_ERROR(-ENOMEM);
        }

      memset(bus, 0, sizeof(*bus));
      sprintf(bus->dirname, "%s%d", LIBUSB_BUS_NAME, i);
      
      bus->location = i;

      USB_MESSAGE_STR(LIBUSB_DEBUG_MSG, "usb_os_find_busses: found %s",
                      bus->dirname);

      LIST_ADD(fbus, bus);
    }
  
  *busses = fbus;

  return 0;
}

int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices)
{
  int i;
  struct usb_device *dev, *fdev = NULL;
  struct usb_dev_handle dev_handle;
  char dev_name[LIBUSB_PATH_MAX];
  DWORD ret;
  HANDLE handle;
  libusb_request req;

  for(i = 0; i < LIBUSB_MAX_DEVICES; i++)
    {
      ret = 0;

      snprintf(dev_name, sizeof(dev_name) - 1,"%s%04d", LIBUSB_DEVICE_NAME, i);

      dev = malloc(sizeof(*dev));
      
      if(!dev) 
        {
          USB_ERROR(-ENOMEM);
        }
      
      memset(dev, 0, sizeof(*dev));
      dev->bus = bus;
      dev->devnum = (unsigned char)i;
      strcpy(dev->filename, dev_name);
      dev_handle.device = dev;

      handle = CreateFile(dev->filename, 0, 0, NULL, OPEN_EXISTING, 
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
          USB_MESSAGE_STR(LIBUSB_DEBUG_ERR, "usb_os_find_devices: getting "
                          "device info failed");
          CloseHandle(handle);
          free(dev);
          continue;
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
                      &(dev->descriptor), USB_DT_DEVICE_SIZE, &ret, NULL);
      
      if(ret < USB_DT_DEVICE_SIZE) 
        {
          USB_MESSAGE_STR(LIBUSB_DEBUG_ERR, "usb_os_find_devices: couldn't "
                          "read device descriptor");
          free(dev);
          CloseHandle(handle);
          continue;
        }
      
      CloseHandle(handle);

      snprintf(dev->filename, LIBUSB_PATH_MAX - 1, "%s--0x%04x-0x%04x", 
               dev_name, dev->descriptor.idVendor, dev->descriptor.idProduct);

      LIST_ADD(fdev, dev);

      USB_MESSAGE_STR(LIBUSB_DEBUG_MSG, "usb_os_find_devices: found %s on %s",
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

  USB_MESSAGE_STR(LIBUSB_DEBUG_MSG, "usb_os_init: dll version: %d.%d.%d.%d",
                  LIBUSB_VERSION_MAJOR, LIBUSB_VERSION_MINOR,
                  LIBUSB_VERSION_MICRO, LIBUSB_VERSION_NANO);


  for(i = 0; i < LIBUSB_MAX_DEVICES; i++)
    {
      /* build the Windows file name */
      snprintf(dev_name, sizeof(dev_name) - 1,"%s%04d", LIBUSB_DEVICE_NAME, i);

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
          USB_MESSAGE_STR(LIBUSB_DEBUG_ERR, "usb_os_init: getting driver "
                          "version failed");

          CloseHandle(dev);
          continue;
        }
      else 
        {
          _usb_version.driver.major = req.version.major;
          _usb_version.driver.minor = req.version.minor;
          _usb_version.driver.micro = req.version.micro;
          _usb_version.driver.nano = req.version.nano;
	  
          USB_MESSAGE_STR(LIBUSB_DEBUG_MSG, "usb_os_init: driver version: "
                          "%d.%d.%d.%d",
                          req.version.major, req.version.minor, 
                          req.version.micro, req.version.nano);
      
          /* set debug level */
          req.timeout = 0;
          req.debug.level = usb_debug;
          
          if(!DeviceIoControl(dev, LIBUSB_IOCTL_SET_DEBUG_LEVEL, 
                              &req, sizeof(libusb_request), 
                              NULL, 0, &ret, NULL))
            {
              USB_MESSAGE_STR(LIBUSB_DEBUG_ERR, "usb_os_init: setting debug "
                              "level failed");
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
      USB_ERROR_STR(-EINVAL, "usb_resetep: error: device not open");
    }

  req.endpoint.endpoint = (int)ep;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_ABORT_ENDPOINT, &req, 
                      sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      USB_ERROR_STR(-win_error_to_errno(), "could not abort ep 0x%02x : win "
                    "error: %s", ep, win_error_to_string());
    }

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_ENDPOINT, &req, 
                      sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      USB_ERROR_STR(-win_error_to_errno(), "could not reset ep 0x%02x : win "
                    "error: %s", ep, win_error_to_string());
    }
  
  return 0;
}

int usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
  DWORD ret;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-EINVAL, "usb_clear_halt: error: device not open");
    }

  req.endpoint.endpoint = (int)ep;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_ENDPOINT, &req, 
                      sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      USB_ERROR_STR(-win_error_to_errno(), "could not clear halt, ep 0x%02x:"
                    " win error: %s", ep, win_error_to_string());
    }
  
  return 0;
}

int usb_reset(usb_dev_handle *dev)
{
  DWORD ret;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-EINVAL, "usb_reset: error: device not open");
    }

  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_DEVICE,
                      &req, sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      USB_ERROR_STR(-win_error_to_errno(), "could not reset device: win "
                    "error: %s", win_error_to_string());
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

  if(usb_debug || level)
    fprintf(stderr, "usb_set_debug: Setting debugging level to %d (%s)\n",
            level, level ? "on" : "off");

  usb_debug = level;

  /* find a valid device */
  for(i = 0; i < LIBUSB_MAX_DEVICES; i++)
    {
      /* build the Windows file name */
      snprintf(dev_name, sizeof(dev_name) - 1,"%s%04d", LIBUSB_DEVICE_NAME, i);

      dev = CreateFile(dev_name, 0, 0, NULL, OPEN_EXISTING, 
                       FILE_FLAG_OVERLAPPED, NULL);
  
      if(dev == INVALID_HANDLE_VALUE) 
        {
          continue;
        }
      
      /* set debug level */
      req.timeout = 0;
      req.debug.level = usb_debug;
      
      if(!DeviceIoControl(dev, LIBUSB_IOCTL_SET_DEBUG_LEVEL, 
                          &req, sizeof(libusb_request), 
                          NULL, 0, &ret, NULL))
        {
          USB_MESSAGE_STR(LIBUSB_DEBUG_ERR, "usb_os_init: setting debug level "
                          "failed");
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
