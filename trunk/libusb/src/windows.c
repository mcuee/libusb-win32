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

#define WIN_DEFAULT_TIMEOUT 5000

#define USB_FEATURE_ENDPOINT_HALT 0

#define LIBUSB_DEVICE_NAME "\\\\.\\libusb0-"
#define LIBUSB_DEVICE_NAME_ZERO "\\\\.\\libusb0-0000"
#define LIBUSB_BUS_NAME "bus-0"
#define WIN_MAX_DEVICES 256

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

static const char *last_win_error();
static void output_debug_string(const char *s, ...);
static void usb_cancel_io(usb_dev_handle *dev);


typedef BOOL (* cancel_io_t)(HANDLE);

static HANDLE _kernel32_dll = NULL;
static cancel_io_t _cancel_io = NULL;


BOOL APIENTRY DllMain(HANDLE module, DWORD reason, LPVOID reserved)
{
  switch(reason)
    {
    case DLL_PROCESS_ATTACH:
      _kernel32_dll = LoadLibrary("KERNEL32.DLL");
      _cancel_io = (cancel_io_t)GetProcAddress(_kernel32_dll, "CancelIo");
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      FreeLibrary(_kernel32_dll);
      break;
    default:
      ;
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
static const char *last_win_error()
{
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 
		LANG_USER_DEFAULT, __error_buf, sizeof (__error_buf) - 1, 
		NULL);
  return __error_buf;
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

  /* build the Windows file name from the unique device name */ 
  strcpy(dev_name, dev->device->filename);

  p = strstr(dev_name, "--");

  if(!p)
    {
      USB_ERROR_STR(-1, "usb_os_open: invalid file name %s",
		    dev->device->filename);
    }
  
  *p = 0;

  dev->impl_info = CreateFile(dev_name, GENERIC_READ | GENERIC_WRITE,
			      FILE_SHARE_READ | FILE_SHARE_WRITE,
			      NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
			      NULL);
      
  if(dev->impl_info == INVALID_HANDLE_VALUE) 
    {
      USB_ERROR_STR(-1, "usb_os_open: failed to open %s: win error: %s",
		    dev->device->filename, last_win_error());
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
      USB_ERROR_STR(-1, "usb_os_close: tried to close device %s: win "
		    "error: %s", dev->device->filename, last_win_error());
    }

  dev->impl_info = INVALID_HANDLE_VALUE;

  return 0;
}

int usb_set_configuration(usb_dev_handle *dev, int configuration)
{
  int sent;
  libusb_request req;
  
  req.configuration.configuration = configuration;
  req.timeout = WIN_DEFAULT_TIMEOUT;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-1, "usb_set_configuration: error: device not open");
    }

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_CONFIGURATION, 
		      &req, sizeof(libusb_request), 
		      NULL, 0, (DWORD *)&sent, NULL))
    {
      USB_ERROR_STR(-1, "could not set config %d: win error: %s", 
		    configuration, last_win_error());
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
      USB_ERROR_STR(-1, "usb_claim_interface: error: device not open");
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
      USB_ERROR_STR(-1, "usb_release_interface: error: device not open");
    }

  if(!dev->config)
    {
      USB_ERROR_STR(-EINVAL, "could not release interface %d, invalid "
		    "configuration %d",
		    interface, dev->config);
    }

  if(interface >= dev->device->config[dev->config - 1].bNumInterfaces)
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
  int sent;
  libusb_request req;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-ENODEV, "usb_set_altinterface: error: device not open");
    }

  if(dev->interface < 0)
    {
      /* invalid interface */
      USB_ERROR_STR(-EINVAL, "could not set alt interface %d/%d: invalid "
		    "interface", dev->interface, alternate);
    }

  req.interface.interface = dev->interface;
  req.interface.altsetting = alternate;
  req.timeout = WIN_DEFAULT_TIMEOUT;
  
  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_INTERFACE, 
		      &req, sizeof(libusb_request), 
		      NULL, 0, (DWORD *)&sent, NULL))
    {
      USB_ERROR_STR(-1, "could not set alt interface %d/%d: win error: %s",
		    dev->interface, alternate, last_win_error());
    }
  
  dev->altsetting = alternate;

  return 0;
}


int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
		   int timeout)
{
  libusb_request req;
  int ret, requested, sent = 0;
  HANDLE event;
  OVERLAPPED ol;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-ENODEV, "usb_bulk_write: error: device not open");
    }

  if(dev->config <= 0)
    {
      USB_ERROR_STR(-EINVAL, "error writing to bulk endpoint 0x%x: "
		    "invalid configuration %d", ep, dev->config);
    }
  if(dev->interface < 0)
    {
      USB_ERROR_STR(-EINVAL, "error writing to bulk endpoint 0x%x: "
		    "invalid interface %d", ep, dev->interface);
    }
  
  req.endpoint.endpoint = ep;
  req.timeout = timeout;

  event = CreateEvent(NULL, TRUE, FALSE, NULL);

  ol.Offset = 0;
  ol.OffsetHigh = 0;
  ol.hEvent = event;

  timeout = timeout ? timeout : INFINITE;
  
  if(!event)
    {
      USB_ERROR_STR(-1, "error creating event: win error: %s", 
		    last_win_error());
    }

  do {
    ResetEvent(event);

    requested = size > LIBUSB_MAX_READ_WRITE ? LIBUSB_MAX_READ_WRITE : size;

    if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE, 
			&req, sizeof(libusb_request), bytes, 
			(DWORD)requested, (DWORD *)&ret, &ol))
      {
	if(GetLastError() != ERROR_IO_PENDING)
	  {
	    CloseHandle(event);
	    USB_ERROR_STR(-1, "error writing to bulk endpoint 0x%x: "
			  "win error: %s", ep, last_win_error());
	  }
      }

    if(WaitForSingleObject(event, timeout) == WAIT_TIMEOUT)
      {
	/* requested timed out */
	usb_cancel_io(dev);
	CloseHandle(event);
	USB_ERROR_STR(-1, "timeout error writing to bulk endpoint 0x%x: "
		      "win error: %s", ep, last_win_error());
      }
    if(!GetOverlappedResult(dev->impl_info, &ol, (DWORD *)&ret, FALSE))
      {
	CloseHandle(event);
	USB_ERROR_STR(-1, "error writing to bulk endpoint 0x%x: win error: %s",
		      ep, last_win_error());
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
  int ret, requested, retrieved = 0;
  HANDLE event;
  OVERLAPPED ol;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-ENODEV, "usb_bulk_read: error: device not open");
    }

  if(dev->config <= 0)
    {
      USB_ERROR_STR(-EINVAL, "error reading from bulk endpoint 0x%x: "
		    "invalid configuration %d", ep, dev->config);
    }

  if(dev->interface < 0)
    {
      USB_ERROR_STR(-EINVAL, "error reading from bulk endpoint 0x%x: "
		    "invalid interface %d", ep, dev->interface);
    }

  req.endpoint.endpoint = ep;
  req.timeout = timeout;

  event = CreateEvent(NULL, TRUE, FALSE, NULL);

  ol.Offset = 0;
  ol.OffsetHigh = 0;
  ol.hEvent = event;

  timeout = timeout ? timeout : INFINITE;

  if(!event)
    {
      USB_ERROR_STR(-1, "error creating event: win error: %s", 
		    last_win_error());
    }

  do {
    ResetEvent(event);

    requested = size > LIBUSB_MAX_READ_WRITE ? LIBUSB_MAX_READ_WRITE : size;

    if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ, 
			&req, sizeof(libusb_request), bytes, 
			(DWORD)requested, (DWORD *)&ret, &ol))
      {
	if(GetLastError() != ERROR_IO_PENDING)
	  {
	    CloseHandle(event);
	    USB_ERROR_STR(-1, "error reading from bulk endpoint 0x%x: "
			  "win error: %s", ep, last_win_error());
	  }
      }

    if(WaitForSingleObject(event, timeout) == WAIT_TIMEOUT)
      {
	/* requested timed out */
	usb_cancel_io(dev);
	CloseHandle(event);
	USB_ERROR_STR(-1, "timeout error reading from bulk endpoint "
		      "0x%x: win error: %s", ep, last_win_error());
      }

    if(!GetOverlappedResult(dev->impl_info, &ol, (DWORD *)&ret, FALSE))
      {
	CloseHandle(event);
	USB_ERROR_STR(-1, "error reading from bulk endpoint 0x%x: "
		      "win error: %s", ep, last_win_error());
      }

    retrieved += ret;
    bytes += ret;
    size -= ret;
  } while(ret > 0 && size && ret == requested);
  
  CloseHandle(event);

  return retrieved;
}


int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
		    int value, int index, char *bytes, int size, int timeout)
{
  int ret = 0;
  int error = 0;
  libusb_request req;
  req.timeout = timeout;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-ENODEV, "usb_control_msg: error: device not open");
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
			      (DWORD *)&ret, NULL))
	    {
	      error = 1;
	      break;
	    }

	  (unsigned short)*bytes = (unsigned short) req.status.status;

	  break;
      
	case USB_REQ_CLEAR_FEATURE:
	  req.feature.recipient = requesttype & 0x1F;
	  req.feature.feature = value;
	  req.feature.index = index;

	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_CLEAR_FEATURE, 
			      &req, sizeof(libusb_request), 
			      NULL, 0, (DWORD *)&ret, NULL))
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
			      NULL, 0, (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }

	  break;

	case USB_REQ_GET_DESCRIPTOR:     	  
	  req.descriptor.type = (value >> 8) & 0x000000FF;
	  req.descriptor.index = value & 0x000000FF;
	  req.descriptor.language_id = index;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_DESCRIPTOR, 
			      &req, sizeof(libusb_request), 
			      bytes, size,(DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }
	  break;
	  
	case USB_REQ_SET_DESCRIPTOR:
	  req.descriptor.type = (value >> 8) & 0x000000FF;
	  req.descriptor.index = value & 0x000000FF;
	  req.descriptor.language_id = index;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_DESCRIPTOR, 
			      &req, sizeof(libusb_request), 
			      bytes, size, (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }

	  break;
	  
	case USB_REQ_GET_CONFIGURATION:

	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_CONFIGURATION, 
			      &req, sizeof(libusb_request), 
			      &req, sizeof(libusb_request),
			      (DWORD *)&ret, NULL))
	    {
	      error = 1;
	      break;
	    }

	  *bytes = (unsigned char)req.configuration.configuration;
	  
	  break;
      
	case USB_REQ_SET_CONFIGURATION:	  
	  req.configuration.configuration = value;

	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_CONFIGURATION, 
			      &req, sizeof(libusb_request), 
			      NULL, 0, (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }

	  break;
	  
	case USB_REQ_GET_INTERFACE:
	  req.interface.interface = index;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_INTERFACE, 
			      &req, sizeof(libusb_request),
			      &req, sizeof(libusb_request),
			      (DWORD *)&ret, NULL))
	    {
	      error = 1;
	      break;
	    }

	  *bytes = (unsigned char)req.interface.altsetting;
	  
	  break;
      
	case USB_REQ_SET_INTERFACE:
	  req.interface.interface = index;
	  req.interface.altsetting = value;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_INTERFACE, 
			      &req, sizeof(libusb_request), 
			      NULL, 0, (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }
      
	  break;
	  
	default:
	  USB_ERROR_STR(-1, "usb_control_msg: invalid request %x", request);
	}
      break;

    case USB_TYPE_VENDOR:  
    case USB_TYPE_CLASS:
    
      req.vendor.request = request;
      req.vendor.value = value;
      req.vendor.index = index;

      if((unsigned char) requesttype & 0x80)
	{
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_VENDOR_READ, 
			      &req, sizeof(libusb_request), 
			      bytes, size, (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }
	}
      else
	{
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_VENDOR_WRITE, 
			      &req, sizeof(libusb_request), 
			      bytes, size, (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }
	}
      break;
    case USB_TYPE_RESERVED:
    default:
      USB_ERROR_STR(-1, "usb_control_msg: invalid or unsupported request"
		    " type: %x", requesttype);
    }
  
  if(error)
    {
      USB_ERROR_STR(-1, "error sending control message: win error: %s", 
		    last_win_error());
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

  memset((void *)bus, 0, sizeof(*bus));
  
  strcpy(bus->dirname, LIBUSB_BUS_NAME);
  
  LIST_ADD(fbus, bus);
  
  USB_MESSAGE_STR(DEBUG_MSG, "usb_os_find_busses: found %s", bus->dirname);
  
  *busses = fbus;

  return 0;
}


int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices)
{
  int i;
  struct usb_device *dev, *fdev = NULL;
  struct usb_dev_handle dev_handle;
  char dev_name[LIBUSB_PATH_MAX];
  int ret = 0;
  int usb_old_debug = usb_debug;
  libusb_request req;
  req.timeout = 0;
  req.debug.level = usb_debug;
  

  for(i = 0; i < WIN_MAX_DEVICES; i++)
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

      dev_handle.impl_info = CreateFile(dev->filename, GENERIC_READ
					| GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, 
					FILE_ATTRIBUTE_NORMAL,
					NULL);

      if(dev_handle.impl_info == INVALID_HANDLE_VALUE) 
	{
	  free(dev);
	  continue;
	}

      usb_debug = 0;
      ret = usb_control_msg(&dev_handle, USB_ENDPOINT_IN | USB_TYPE_STANDARD 
			    | USB_RECIP_DEVICE,
			    USB_REQ_GET_DESCRIPTOR, USB_DT_DEVICE << 8 , 0,
			    (char *)&(dev->descriptor), 
			    USB_DT_DEVICE_SIZE,
			    WIN_DEFAULT_TIMEOUT);

      if(ret < USB_DT_DEVICE_SIZE) 
	{
	  USB_MESSAGE_STR(DEBUG_ERR, "usb_os_find_devices: Couldn't read"
			  " device descriptor");
	  free(dev);
	  CloseHandle(dev_handle.impl_info);
	  dev_handle.impl_info = INVALID_HANDLE_VALUE;
	  usb_debug = usb_old_debug;
	  continue;
	}
      
      usb_debug = usb_old_debug;
      CloseHandle(dev_handle.impl_info);
      dev_handle.impl_info = INVALID_HANDLE_VALUE;

      /* build a unique device name, this is necessary to detect new devices */
      /* if an application calls 'usb_find_devices()' multiple times */
      snprintf(dev->filename, LIBUSB_PATH_MAX - 1, "%s--0x%04x-0x%04x", 
	       dev_name, dev->descriptor.idVendor, dev->descriptor.idProduct);

      LIST_ADD(fdev, dev);

      USB_MESSAGE_STR(DEBUG_MSG, "usb_os_find_devices: found %s on %s",
		      dev->filename, bus->dirname);
    }
  
  *devices = fdev;

  if(devices)
    {
      snprintf(dev_name, sizeof(dev_name) - 1,"%s%04d", 
	       LIBUSB_DEVICE_NAME, 0);
      
      dev_handle.impl_info = CreateFile(dev_name, GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, 
					FILE_ATTRIBUTE_NORMAL,
					NULL);
      
      if(dev_handle.impl_info != INVALID_HANDLE_VALUE) 
	{
	  /* set debug level */
	  DeviceIoControl(dev_handle.impl_info, LIBUSB_IOCTL_SET_DEBUG_LEVEL, 
			  &req, sizeof(libusb_request), 
			  NULL, 0, (DWORD *)&ret, NULL); 
	  CloseHandle(dev_handle.impl_info);
	  dev_handle.impl_info = INVALID_HANDLE_VALUE;
	}
    }

  return 0;
}


void usb_os_init(void)
{
  DWORD ret;
  HANDLE dev_0;
  libusb_request req;

  USB_MESSAGE_STR(DEBUG_MSG, "usb_os_init: dll version: %d.%d.%d.%d",
		  LIBUSB_VERSION_MAJOR, LIBUSB_VERSION_MINOR,
		  LIBUSB_VERSION_MICRO, LIBUSB_VERSION_NANO);


  dev_0 = CreateFile(LIBUSB_DEVICE_NAME_ZERO, 
		     GENERIC_READ, FILE_SHARE_READ,
		     NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
		     NULL);
      
  if(dev_0 == INVALID_HANDLE_VALUE) 
    {
      USB_MESSAGE_STR(DEBUG_ERR, "usb_os_init: open driver failed");
      return;
    }
  
  if(!DeviceIoControl(dev_0, LIBUSB_IOCTL_GET_VERSION, 
		      NULL, 0, &req, sizeof(libusb_request), 
		      (DWORD *)&ret, NULL))
    {
      USB_MESSAGE_STR(DEBUG_ERR, "usb_os_init: getting driver version failed");
    }
  else 
    {
      _usb_version.driver.major = req.version.major;
      _usb_version.driver.minor = req.version.minor;
      _usb_version.driver.micro = req.version.micro;
      _usb_version.driver.nano = req.version.nano;

      USB_MESSAGE_STR(DEBUG_MSG, "usb_os_init: driver version: "
		      "%d.%d.%d.%d",
		      req.version.major, req.version.minor, 
		      req.version.micro, req.version.nano);
    }
  CloseHandle(dev_0);
}


int usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
  int ret;
  libusb_request req;
  req.endpoint.endpoint = (int)ep;
  req.timeout = WIN_DEFAULT_TIMEOUT;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-ENODEV, "usb_resetep: error: device not open");
    }

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_ENDPOINT, 
		      &req, sizeof(libusb_request),
		      NULL, 0, (DWORD *)&ret, NULL))
    {
      USB_ERROR_STR(-1, "could not reset ep %d : win error: %s", ep, 
		    last_win_error());
    }
  
  return 0;
}

int usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
  int ret;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-ENODEV, "usb_clear_halt: error: device not open");
    }

  ret = usb_control_msg(dev, USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE,
			USB_FEATURE_ENDPOINT_HALT, (int)ep, NULL, 0, 
			WIN_DEFAULT_TIMEOUT);

  if(ret < 0)
    {
      USB_ERROR_STR(ret, "could not clear/halt ep %d: win error: %s", ep,
		    last_win_error());
    }
  return 0;
}

int usb_reset(usb_dev_handle *dev)
{
  int ret;
  libusb_request req;
  req.timeout = WIN_DEFAULT_TIMEOUT;

  if(dev->impl_info == INVALID_HANDLE_VALUE)
    {
      USB_ERROR_STR(-ENODEV, "usb_reset: error: device not open");
    }

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_DEVICE,
		      &req, sizeof(libusb_request), NULL, 0,
		      (DWORD *)&ret, NULL))
    {
      USB_ERROR_STR(-1, "could not reset: win error: %s", last_win_error());
    }
  return 0;
}

struct usb_version *usb_get_version(void)
{
  return &_usb_version;
}
