/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2004 Stephan Meyer <ste_meyer@web.de>
 * Copyright (c) 2000-2004 Johannes Erdfelt <johannes@erdfelt.com>
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
#include "filter_api.h"


#define USB_FEATURE_ENDPOINT_HALT 0

#define LIBUSB_DEFAULT_TIMEOUT 1000
#define LIBUSB_DEVICE_NAME "\\\\.\\libusb0-"
#define LIBUSB_DEVICE_NAME_ZERO "\\\\.\\libusb0-0000"
#define LIBUSB_BUS_NAME "bus-0"
#define LIBUSB_MAX_DEVICES 256

#ifdef USB_ERROR_STR
#undef USB_ERROR_STR
#endif

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



static char __error_buf[512];

static struct usb_version _usb_version= {
  {
    VERSION_MAJOR,
    VERSION_MINOR,
    VERSION_MICRO,
    VERSION_NANO
  },
  {
    -1,
    -1,
    -1,
    -1
  }
};

static const char *win_error_to_string();
static int win_error_to_errno();
static void output_debug_string(const char *s, ...);
static void usb_cancel_io(usb_dev_handle *dev);


typedef BOOL (* cancel_io_t)(HANDLE);

static HANDLE _kernel32_dll = NULL;
static cancel_io_t _cancel_io = NULL;


BOOL WINAPI DllMain(HANDLE module, DWORD reason, LPVOID reserved)
{
  switch(reason)
    {
    case DLL_PROCESS_ATTACH:
      _kernel32_dll = LoadLibrary("KERNEL32.DLL");
      if(!_kernel32_dll)
	{ 
	  return FALSE;
	}
      _cancel_io = (cancel_io_t)GetProcAddress(_kernel32_dll, "CancelIo");
      break;
    case DLL_PROCESS_DETACH:
      FreeLibrary(_kernel32_dll);
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

/* returns Windows last error in a human readable form */
static const char *win_error_to_string()
{
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 
		LANG_USER_DEFAULT, __error_buf, 
		sizeof (__error_buf) - 1, 
		NULL);
  return __error_buf;
}

#define ETIMEDOUT 116		/* Connection timed out */

static int win_error_to_errno()
{
  switch(GetLastError())
    {
    case ERROR_SUCCESS:
      return 0; break;
    case ERROR_INVALID_PARAMETER:
      return EINVAL; break;
    case ERROR_SEM_TIMEOUT: 
    case ERROR_OPERATION_ABORTED:
      return ETIMEDOUT; break;
    case ERROR_NOT_ENOUGH_MEMORY:
      return ENOMEM; break;
    default:
      return EIO; break;
    }
}

static void usb_cancel_io(usb_dev_handle *dev)
{
  if(!_cancel_io)
    {
      /* Win95 compatibility (wincore 4) */
      usb_os_close(dev);
      usb_os_open(dev);
    } 
  else
    {
      /* wincore 5 or greater... */
      _cancel_io(dev->impl_info);
    }
}

int usb_os_open(usb_dev_handle *dev)
{
  char dev_name[LIBUSB_PATH_MAX];
  char *p;

  if(!dev->device->filename)
    {
      USB_ERROR_STR(-ENOENT, "usb_os_open: invalid file name NULL");
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

  dev->impl_info = CreateFile(dev_name, GENERIC_READ | GENERIC_WRITE,
			      FILE_SHARE_READ | FILE_SHARE_WRITE,
			      NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
			      NULL);
      
  if(dev->impl_info == INVALID_HANDLE_VALUE) 
    {
      USB_ERROR_STR(-ENOENT, "usb_os_open: failed to open %s: win error: %s",
		    dev->device->filename, win_error_to_string());
    }
  
  return 0;
}

int usb_os_close(usb_dev_handle *dev)
{
  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      return 0;
    }
  
  if(!CloseHandle(dev->impl_info)) 
    {
      USB_ERROR_STR(-ENOENT, "usb_os_close: tried to close device %s: win "
		    "error: %s", dev->device->filename, win_error_to_string());
    }

  dev->impl_info = INVALID_HANDLE_VALUE;

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

  dev->interface = interface;
  dev->altsetting = 0;

  return 0;
}

int usb_release_interface(usb_dev_handle *dev, int interface)
{
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

  dev->interface = -1;
  dev->altsetting = -1;
  
  return 0;
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
      /* invalid interface */
      USB_ERROR_STR(-EINVAL, "could not set alt interface %d/%d: invalid "
		    "interface", dev->interface, alternate);
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


int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
		   int timeout)
{
  libusb_request req;
  DWORD ret, requested;
  int sent = 0;
  HANDLE event;
  OVERLAPPED ol;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-EINVAL, "usb_bulk_write/usb_interrupt_write: error: "
		    "device not open");
    }

  if(dev->config <= 0)
    {
      USB_ERROR_STR(-EINVAL, "error writing to bulk or interrupt endpoint "
		    "0x%02x: invalid configuration %d", ep, dev->config);
    }

  if(dev->interface < 0)
    {
      USB_ERROR_STR(-EINVAL, "error writing to bulk or interrupt endpoint "
		    "0x%02x: invalid interface %d", ep, dev->interface);
    }
  
  if(ep & USB_ENDPOINT_IN)
    {
      USB_ERROR_STR(-EINVAL, "error writing to bulk or interrupt endpoint "
		    "0x02%x: invalid endpoint", ep);
    }

  event = CreateEvent(NULL, TRUE, FALSE, NULL);

  if(!event)
    {
      USB_ERROR_STR(-win_error_to_errno(), "creating event failed: win "
		    "error: %s", win_error_to_string());
    }

  req.endpoint.endpoint = ep;
  req.timeout = timeout;

  ol.Offset = 0;
  ol.OffsetHigh = 0;
  ol.hEvent = event;

  timeout = timeout ? timeout : INFINITE;
  
  do {
    ResetEvent(event);
    requested = size > LIBUSB_MAX_READ_WRITE ? LIBUSB_MAX_READ_WRITE : size;

    if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE, 
		    	&req, sizeof(libusb_request), bytes, 
			requested, &ret, &ol))
      {
	if(GetLastError() != ERROR_IO_PENDING)
	  {
	    CloseHandle(event);
	    USB_ERROR_STR(-win_error_to_errno(), "error writing to bulk or "
			  "interrupt endpoint 0x%02x: win error: %s",
			  ep, win_error_to_string());
	  }
      }

    if(WaitForSingleObject(event, timeout) == WAIT_TIMEOUT)
      {
	/* request timed out */
	usb_cancel_io(dev);
	CloseHandle(event);
	USB_ERROR_STR(-ETIMEDOUT, "timeout error writing to bulk or interrupt "
		      "endpoint 0x%02x: win error: %s", 
		      ep, win_error_to_string());
      }

    if(!GetOverlappedResult(dev->impl_info, &ol, &ret, FALSE))
      {
	CloseHandle(event);
	USB_ERROR_STR(-win_error_to_errno(), "error writing to bulk or "
		      "interrupt endpoint 0x%02x: win error: %s", 
		      ep, win_error_to_string());
      }

    sent += ret;
    bytes += ret;
    size -= ret;
  } while(ret > 0 && size);
  
  CloseHandle(event);

  return sent;
}

int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size,
		  int timeout)
{
  libusb_request req;
  DWORD ret, requested;
  int retrieved = 0;
  HANDLE event;
  OVERLAPPED ol;


  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-EINVAL, "usb_bulk_read/usb_interrupt_read: error: device "
		    "not open");
    }

  if(dev->config <= 0)
    {
      USB_ERROR_STR(-EINVAL, "error reading from bulk or interrupt endpoint "
		    "0x%02x: invalid configuration %d", ep, dev->config);
    }

  if(dev->interface < 0)
    {
      USB_ERROR_STR(-EINVAL, "error reading from bulk endpoint or interrupt "
		    "0x%02x: invalid interface %d", ep, dev->interface);
    }

  if(!(ep & USB_ENDPOINT_IN))
    {
      USB_ERROR_STR(-EINVAL, "error reading from bulk or interrupt endpoint "
		    "0x%02x: invalid endpoint", ep);
    }

  event = CreateEvent(NULL, TRUE, FALSE, NULL);

  if(!event)
    {
      USB_ERROR_STR(-win_error_to_errno(), "creating event failed: win error: "
		    "%s", win_error_to_string());
    }

  req.endpoint.endpoint = ep;
  req.timeout = timeout;

  ol.Offset = 0;
  ol.OffsetHigh = 0;
  ol.hEvent = event;

  timeout = timeout ? timeout : INFINITE;

  do {
    ResetEvent(event);
    requested = size > LIBUSB_MAX_READ_WRITE ? LIBUSB_MAX_READ_WRITE : size;

    if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ, 
			&req, sizeof(libusb_request), bytes, 
			requested, &ret, &ol))
      {
	if(GetLastError() != ERROR_IO_PENDING)
	  {
	    CloseHandle(event);
	    USB_ERROR_STR(-win_error_to_errno(), "error reading from bulk or "
			  "interrupt endpoint 0x%02x: win error: %s",
			  ep, win_error_to_string());
	  }
      }

    if(WaitForSingleObject(event, timeout) == WAIT_TIMEOUT)
      {
	/* request timed out */
	usb_cancel_io(dev);
	CloseHandle(event);
	USB_ERROR_STR(-ETIMEDOUT, "timeout error reading from bulk or "
		      "interrupt endpoint 0x%02x: win error: %s",
		      ep, win_error_to_string());
      }

    if(!GetOverlappedResult(dev->impl_info, &ol, &ret, FALSE))
      {
	CloseHandle(event);
	USB_ERROR_STR(-win_error_to_errno(), "error reading from bulk or "
		      "interrupt endpoint 0x%02x: win error: %s",
		      ep, win_error_to_string());
      }

    retrieved += ret;
    bytes += ret;
    size -= ret;
  } while(ret > 0 && size && ret == requested);
  
  CloseHandle(event);

  return retrieved;
}

int usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, int size,
			int timeout)
{
  return usb_bulk_write(dev, ep, bytes, size, timeout);
}

int usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, int size,
		       int timeout)
{
  return usb_bulk_read(dev, ep, bytes, size, timeout);
}

int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
		    int value, int index, char *bytes, int size, int timeout)
{
  DWORD ret = 0;
  int error = 0;
  char *tmp = NULL;
  libusb_request req;
  req.timeout = timeout;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-EINVAL, "usb_control_msg: error: device not open");
    }

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
			      &req, sizeof(libusb_request), 
			      &ret, NULL))
	    {
	      error = 1;
	      break;
	    }

	  *((unsigned short *)bytes) = (unsigned short) req.status.status;

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
			      &req, sizeof(libusb_request),
			      &ret, NULL))
	    {
	      error = 1;
	      break;
	    }

	  *bytes = (char)req.configuration.configuration;
	  
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
			      &req, sizeof(libusb_request),
			      &ret, NULL))
	    {
	      error = 1;
	      break;
	    }

	  *bytes = (char)req.interface.altsetting;
	  
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
  struct usb_bus *bus;
  
  /* windows doesn't support busses, so we just create one manually */
  bus = malloc(sizeof(struct usb_bus));

  if(!bus)
    {
      USB_ERROR(-ENOMEM);
    }

  memset(bus, 0, sizeof(*bus));
  strcpy(bus->dirname, LIBUSB_BUS_NAME);
  LIST_ADD(fbus, bus);
  *busses = fbus;

  USB_MESSAGE_STR(LIBUSB_DEBUG_MSG, "usb_os_find_busses: found %s",
		  bus->dirname);

  return 0;
}


int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices)
{
  int i;
  struct usb_device *dev, *fdev = NULL;
  struct usb_dev_handle dev_handle;
  char dev_name[LIBUSB_PATH_MAX];
  DWORD ret = 0;

  for(i = 0; i < LIBUSB_MAX_DEVICES; i++)
    {
      /* build the Windows file name */
      snprintf(dev_name, sizeof(dev_name) - 1,"%s%04d", 
	       LIBUSB_DEVICE_NAME, i);

      dev = malloc(sizeof(*dev));
      
      if(!dev) 
	{
	  USB_ERROR(-ENOMEM);
	}
      
      memset(dev, 0, sizeof(*dev));
      dev->bus = bus;
      strcpy(dev->filename, dev_name);
      dev_handle.device = dev;

      dev_handle.impl_info = CreateFile(dev->filename, 
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, 
					FILE_ATTRIBUTE_NORMAL,
					NULL);

      if(dev_handle.impl_info == INVALID_HANDLE_VALUE) 
	{
	  free(dev);
	  continue;
	}

      ret = usb_control_msg(&dev_handle, USB_ENDPOINT_IN | USB_TYPE_STANDARD 
			    | USB_RECIP_DEVICE,
			    USB_REQ_GET_DESCRIPTOR, USB_DT_DEVICE << 8 , 0,
			    (char *)&(dev->descriptor), 
			    USB_DT_DEVICE_SIZE,
			    LIBUSB_DEFAULT_TIMEOUT);

      if(ret < USB_DT_DEVICE_SIZE) 
	{
	  USB_MESSAGE_STR(LIBUSB_DEBUG_ERR, "usb_os_find_devices: couldn't "
			  "read device descriptor");
	  free(dev);
	  CloseHandle(dev_handle.impl_info);
	  dev_handle.impl_info = INVALID_HANDLE_VALUE;
	  continue;
	}
      
      CloseHandle(dev_handle.impl_info);
      dev_handle.impl_info = INVALID_HANDLE_VALUE;

      /* build a unique device name, this is necessary to detect new devices */
      /* if an application calls 'usb_find_devices()' multiple times */
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

  USB_MESSAGE_STR(LIBUSB_DEBUG_MSG, "usb_os_init: dll version: %d.%d.%d.%d",
		  LIBUSB_VERSION_MAJOR, LIBUSB_VERSION_MINOR,
		  LIBUSB_VERSION_MICRO, LIBUSB_VERSION_NANO);

  dev = CreateFile(LIBUSB_DEVICE_NAME_ZERO, 
		   GENERIC_READ, FILE_SHARE_READ,
		   NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
		   NULL);
  
  if(dev == INVALID_HANDLE_VALUE) 
    {
      USB_MESSAGE_STR(LIBUSB_DEBUG_ERR, "usb_os_init: open driver failed");
      return;
    }
  
  if(!DeviceIoControl(dev, LIBUSB_IOCTL_GET_VERSION, &req, 
		      sizeof(libusb_request), &req, sizeof(libusb_request), 
		      &ret, NULL))
    {
      USB_MESSAGE_STR(LIBUSB_DEBUG_ERR, "usb_os_init: getting driver "
		      "version failed");
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
}


int usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
  DWORD ret;
  libusb_request req;
  req.endpoint.endpoint = (int)ep;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-EINVAL, "usb_resetep: error: device not open");
    }

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_ENDPOINT, &req, 
		      sizeof(libusb_request), NULL, 0, &ret, NULL))
    {
      USB_ERROR_STR(-win_error_to_errno(), "could not reset ep 0x%x : win "
		    "error: %s", ep, win_error_to_string());
    }
  
  return 0;
}

int usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
  int ret;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-EINVAL, "usb_clear_halt: error: device not open");
    }

  ret = usb_control_msg(dev, USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE,
			USB_FEATURE_ENDPOINT_HALT, (int)ep, NULL, 0, 
			LIBUSB_DEFAULT_TIMEOUT);

  if(ret < 0)
    {
      USB_ERROR_STR(ret, "could not clear/halt ep 0x%x: win error: %s", ep,
		    win_error_to_string());
    }

  return 0;
}

int usb_reset(usb_dev_handle *dev)
{
  DWORD ret;
  libusb_request req;
  req.timeout = LIBUSB_DEFAULT_TIMEOUT;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-EINVAL, "usb_reset: error: device not open");
    }

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
