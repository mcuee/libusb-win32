/*
 * Windows USB support
 *
 * Copyright (c) 2002-2003 Stephan Meyer, <ste_meyer@web.de>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 */



#include <stdlib.h>
#include <string.h>
#include <stdio.h> 
#include <errno.h>
#include <wchar.h>
#include <ctype.h>

#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>


#include "usb.h"
#include "usbi.h"
#include "filter_api.h"

#define WIN_DEFAULT_TIMEOUT 5000

#define USB_FEATURE_ENDPOINT_HALT 0

#define LIBUSB_DEVICE_NAME "\\\\.\\libusb0-"
#define LIBUSB_BUS_NAME "bus-0"

#define WIN_MAX_DEVICES 127


static char __error_buf[512];

static const char * last_win_error();
static int usb_get_devs_to_skip(void);


static const char * last_win_error()
{
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 
		LANG_USER_DEFAULT, __error_buf, sizeof (__error_buf) - 1, 
		NULL);
  return __error_buf;
}


static int usb_get_devs_to_skip(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  int ret = 0;
  DWORD reg_type, size = 0;
  LPTSTR hardware_id = NULL;

  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_info = SetupDiGetClassDevs(NULL, "USB", 0, DIGCF_ALLCLASSES);

  if(dev_info == INVALID_HANDLE_VALUE)
    {
      return 0;
    }

  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      hardware_id = NULL;
      do
	{
	  SetupDiGetDeviceRegistryProperty(dev_info, &dev_info_data,
					   SPDRP_HARDWAREID, 
					   &reg_type, NULL, 
					   0, &size);
	  if(!size)
	    {
	      break;
	    }
	  
	  hardware_id = (LPTSTR) malloc(size + 2 * sizeof(TCHAR));
	  
	  if(!hardware_id)
	    {
	      break;
	    }
	  
	  if(!SetupDiGetDeviceRegistryProperty(dev_info, &dev_info_data,
					       SPDRP_HARDWAREID,
					       &reg_type,
					       (BYTE *)hardware_id, 
					       size, NULL))
	    {
	      break;
	    }
	} while(0);

      if(strstr(hardware_id, "ROOT_HUB"))
	ret++;
      
      if(hardware_id) 
	free(hardware_id);
      
      dev_index++;
    }

  SetupDiDestroyDeviceInfoList(dev_info);

  return ret * 2;
}


int usb_os_open(usb_dev_handle *dev)
{

  dev->impl_info = CreateFile(dev->device->filename, GENERIC_READ
			      | GENERIC_WRITE,
			      FILE_SHARE_READ | FILE_SHARE_WRITE,
			      NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
			      NULL);

  if(dev->impl_info == INVALID_HANDLE_VALUE) 
    {
      dev->impl_info = CreateFile(dev->device->filename, GENERIC_READ,
			     FILE_SHARE_READ,
				  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
			     NULL);
      
      if(dev->impl_info == INVALID_HANDLE_VALUE) 
	{
	  USB_ERROR_STR(-1, "failed to open %s: win error: %s\n",
			dev->device->filename, last_win_error());
	}
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
      USB_ERROR_STR(0, "tried to close device %s: win error: %s\n", 
		    dev->device->filename, last_win_error());
    }
  return 0;
}

int usb_set_configuration(usb_dev_handle *dev, int configuration)
{
  int sent;
  libusb_request req;
  
  req.configuration.configuration = configuration;
  req.timeout = WIN_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_CONFIGURATION, 
		      &req, sizeof(libusb_request), 
		      NULL, 0, (DWORD *)&sent, NULL))
    {
      USB_ERROR_STR(-1, "could not set config %d: win error: %s\n", 
		    configuration, last_win_error());
    }
  
  dev->config = configuration;
  
  return 0;
}

int usb_claim_interface(usb_dev_handle *dev, int interface)
{
  /* windows doesn't support this */
  dev->interface = interface;
  
  return 0;
}

int usb_release_interface(usb_dev_handle *dev, int interface)
{
  /* windows doesn't support this */
  dev->interface = -1;
  
  return 0;
}

int usb_set_altinterface(usb_dev_handle *dev, int alternate)
{
  int sent;
  libusb_request req;

  if(dev->interface < 0)
    {
      USB_ERROR(-EINVAL);
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

  ep &= ~USB_ENDPOINT_IN;
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

    requested = size > MAX_READ_WRITE ? MAX_READ_WRITE : size;

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
	CancelIo(dev->impl_info);
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

  ep |= USB_ENDPOINT_IN;
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

    requested = size > MAX_READ_WRITE ? MAX_READ_WRITE : size;

    if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ, 
			&req, sizeof(libusb_request), bytes, 
			(DWORD)requested, (DWORD *)&ret, &ol))
      {
	if(GetLastError() != ERROR_IO_PENDING)
	  {
	    CloseHandle(event);
	    USB_ERROR_STR(-1, "error reading from bulk endpoint 0x%x: "
			  "win error: %s\n", ep, last_win_error());
	  }
      }

    if(WaitForSingleObject(event, timeout) == WAIT_TIMEOUT)
      {
	CancelIo(dev->impl_info);
	CloseHandle(event);
	    USB_ERROR_STR(-1, "timeout error reading from bulk endpoint "
			  "0x%x: win error: %s\n", ep, last_win_error());
      }

    if(!GetOverlappedResult(dev->impl_info, &ol, (DWORD *)&ret, FALSE))
      {
	CloseHandle(event);
	USB_ERROR_STR(-1, "error reading from bulk endpoint 0x%x: "
		      "win error: %s\n", ep, last_win_error());
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

  bus = malloc(sizeof(*bus));
  
  if(!bus)
    {
      USB_ERROR(-ENOMEM);
    }
  
  /* windows doesn't support busses, so we just create one manualy */
  bus = malloc(sizeof(*bus));
  memset((void *)bus, 0, sizeof(*bus));
  
  strcpy(bus->dirname, LIBUSB_BUS_NAME);
  
  LIST_ADD(fbus, bus);
  
  if(usb_debug >= 2)
    {
      fprintf(stderr, "usb_os_find_busses: found %s\n", bus->dirname);
    }
  
  *busses = fbus;

  return 0;
}


int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices)
{
  int dev_number;
  struct usb_device *dev, *fdev = NULL;
  struct usb_dev_handle dev_handle;
  char dev_name[LIBUSB_PATH_MAX];
  int ret;

  for(dev_number = usb_get_devs_to_skip(); dev_number < WIN_MAX_DEVICES; 
      dev_number++)
    {
      snprintf(dev_name, sizeof(dev_name) - 1,"%s%04d", 
	       LIBUSB_DEVICE_NAME, dev_number);

      dev = malloc(sizeof(*dev));
      
      if(!dev) 
	{
	  USB_ERROR(-ENOMEM);
	}
      
      memset((void *)dev, 0, sizeof(*dev));
      
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


      ret = usb_control_msg(&dev_handle, USB_ENDPOINT_IN | USB_TYPE_STANDARD 
			    | USB_RECIP_DEVICE,
			    USB_REQ_GET_DESCRIPTOR, USB_DT_DEVICE << 8 , 0,
			    (char *)&(dev->descriptor), 
			    sizeof(struct usb_device_descriptor),
			    WIN_DEFAULT_TIMEOUT);
      if (ret < 0) 
	{
	  if (usb_debug)
	    {
	      fprintf(stderr, "usb_os_find_devices: Couldn't read"
		      " descriptor\n");
	    }
	  free(dev);
	  CloseHandle(dev_handle.impl_info);
	  continue;
	}
      
      LIST_ADD(fdev, dev);
      
      if(usb_debug >= 2)
	{
	fprintf(stderr, "usb_os_find_devices: found %s on %s\n",
		dev->filename, bus->dirname);
	}
      CloseHandle(dev_handle.impl_info);
    }
  
  *devices = fdev;

  return 0;
}


void usb_os_init(void)
{
  /* nothing to do here */
}


int usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
  int ret;
  libusb_request req;
  req.endpoint.endpoint = (int)ep;
  req.timeout = WIN_DEFAULT_TIMEOUT;

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

  ret = usb_control_msg(dev, USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE,
			USB_FEATURE_ENDPOINT_HALT, (int)ep, NULL, 0, 
			WIN_DEFAULT_TIMEOUT);

  if(ret < 0)
    {
      USB_ERROR_STR(ret, "could not clear/halt ep %d: win error: %s\n", ep,
		    last_win_error());
    }
  return 0;
}

int usb_reset(usb_dev_handle *dev)
{
  int ret;
  libusb_request req;
  req.timeout = WIN_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_DEVICE,
		      &req, sizeof(libusb_request), NULL, 0,
		      (DWORD *)&ret, NULL))
    {
      USB_ERROR_STR(-1, "could not reset: win error: %s\n", last_win_error());
    }
  return 0;
}
