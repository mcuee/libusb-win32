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

#define WIN_DEFAULT_TIMEOUT             5000

#define USB_FEATURE_ENDPOINT_HALT         0

#define LIBUSB_DEVICE_NAME "\\\\.\\libusb-"
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
			      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
			      NULL);

  if(dev->impl_info == INVALID_HANDLE_VALUE) 
    {
      dev->impl_info = CreateFile(dev->device->filename, GENERIC_READ,
			     FILE_SHARE_READ,
			     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
			     NULL);
      
      if(dev->impl_info == INVALID_HANDLE_VALUE) 
	{
	  USB_ERROR_STR(-1, "failed to open %s: %s\n",
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
      USB_ERROR_STR(0, "tried to close device %s: %s\n", 
		    dev->device->filename, last_win_error());
    }
  return 0;
}

int usb_set_configuration(usb_dev_handle *dev, int configuration)
{
  int sent;
  usb_configuration_request configuration_request;
  
  configuration_request.configuration = configuration;
  configuration_request.timeout = WIN_DEFAULT_TIMEOUT;

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_CONFIGURATION, 
		      (void *)&configuration_request, 
		      sizeof(usb_configuration_request), 
		      NULL, 0, (DWORD *)&sent, NULL))
    {
      USB_ERROR_STR(-1, "could not set config %d: %s\n", configuration,
		    last_win_error());
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
  usb_interface_request m_request;

  if(dev->interface < 0)
    {
      USB_ERROR(-EINVAL);
    }

  m_request.interface = dev->interface;
  m_request.altsetting = alternate;
  m_request.timeout = WIN_DEFAULT_TIMEOUT;
  
  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_INTERFACE, 
		      (void *)&m_request, sizeof(usb_interface_request), 
		      NULL, 0,
		      (DWORD *)&sent, NULL))
    {
      USB_ERROR_STR(-1, "could not set alt intf %d/%d: %s",
		    dev->interface, alternate, last_win_error());
    }
  
  dev->altsetting = alternate;

  return 0;
}


int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout)
{
  usb_bulk_transfer bulk;
  int ret, sent = 0;

  ep &= ~USB_ENDPOINT_IN;
  
  do {
    bulk.endpoint = ep;
    bulk.len = size - sent;
    
    if(bulk.len > MAX_READ_WRITE)
      {
	bulk.len = MAX_READ_WRITE;
      }
    
    bulk.timeout = timeout;
    bulk.data = (unsigned char *)bytes + sent;
    
    if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE, 
			&bulk, sizeof(bulk), (void *)bulk.data, 
			(DWORD)bulk.len, (DWORD *)&ret, NULL))
      {
	USB_ERROR_STR(-1, "error writing to bulk endpoint 0x%x: %s",
		      ep, last_win_error());
      }
    
    sent += ret;
  } while(ret > 0 && sent < size);
  
  return sent;
}

int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout)
{
  usb_bulk_transfer bulk;
  int ret, retrieved = 0, requested;

  ep |= USB_ENDPOINT_IN;

  do {
    bulk.endpoint = ep;
    requested = size - retrieved;
    if(requested > MAX_READ_WRITE)
      {
	requested = MAX_READ_WRITE;
      }
    bulk.len = requested;
    bulk.timeout = timeout;
    bulk.data = (unsigned char *)bytes + retrieved;

    if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ, 
			&bulk, sizeof(bulk), (void *)bulk.data, 
			(DWORD)bulk.len, (DWORD *)&ret, NULL))
      {
	USB_ERROR_STR(-1, "error reading from bulk endpoint 0x%x: %s\n",
		      ep, last_win_error());
	
      }
    
    retrieved += ret;
    
  } while(ret > 0 && retrieved < size && ret == requested);
  
  return retrieved;
}


int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
	int value, int index, char *bytes, int size, int timeout)
{
  int ret = 0;
  int error = 0;
  usb_status_request status_request;
  usb_feature_request feature_request;
  usb_descriptor_request descriptor_request;
  usb_configuration_request configuration_request;
  usb_interface_request interface_request;
  usb_vendor_request vendor_request;

  switch(requesttype & (0x03 << 5))
    {
    case USB_TYPE_STANDARD:      
      switch(request)
	{
	case USB_REQ_GET_STATUS: 
	  status_request.recipient = requesttype & 0x1F;
	  status_request.index = index;
	  status_request.timeout = timeout;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_STATUS, 
			      (void *)&status_request, 
			      sizeof(usb_status_request), 
			      (void *)&status_request, 
			      sizeof(usb_status_request), 
			      (DWORD *)&ret, NULL))
	    {
	      error = 1;
	      break;
	    }

	  (unsigned short)*bytes = (unsigned short) status_request.status;

	  break;
      
	case USB_REQ_CLEAR_FEATURE:
	  feature_request.recipient = requesttype & 0x1F;
	  feature_request.feature = value;
	  feature_request.index = index;
	  feature_request.timeout = timeout;

	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_CLEAR_FEATURE, 
			      (void *)&feature_request, 
			      sizeof(usb_feature_request), 
			      NULL, 0, (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }

	  break;
	  
	case USB_REQ_SET_FEATURE:
	  feature_request.recipient = requesttype & 0x1F;
	  feature_request.feature = value;
	  feature_request.index = index;
	  feature_request.timeout = timeout;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_FEATURE, 
			      (void *)&feature_request, 
			      sizeof(usb_feature_request), 
			      NULL, 0, (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }

	  break;

	case USB_REQ_GET_DESCRIPTOR:     	  
	  descriptor_request.type = value >> 8;
	  descriptor_request.index = value & 0x00FF;
	  descriptor_request.language_id = index;
	  descriptor_request.timeout = timeout;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_DESCRIPTOR, 
			      (void *)&descriptor_request, 
			      sizeof(usb_descriptor_request), 
			      (void *)bytes, size, 
			      (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }
	  break;
	  
	case USB_REQ_SET_DESCRIPTOR:
	  descriptor_request.type = value >> 8;
	  descriptor_request.index = value & 0x00FF;
	  descriptor_request.language_id = index;
	  descriptor_request.timeout = timeout;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_DESCRIPTOR, 
			      (void *)&descriptor_request, 
			      sizeof(usb_descriptor_request), 
			      (void *)bytes, size, 
			      (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }

	  break;
	  
	case USB_REQ_GET_CONFIGURATION:
	  configuration_request.timeout = timeout;

	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_CONFIGURATION, 
			      (void *)&configuration_request, 
			      sizeof(usb_configuration_request), 
			      (void *)&configuration_request, 
			      sizeof(usb_configuration_request), 
			      (DWORD *)&ret, NULL))
	    {
	      error = 1;
	      break;
	    }

	  *bytes = (unsigned char) configuration_request.configuration;
	  
	  break;
      
	case USB_REQ_SET_CONFIGURATION:	  
	  configuration_request.configuration = value;
	  configuration_request.timeout = timeout;

	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_CONFIGURATION, 
			      (void *)&configuration_request, 
			      sizeof(usb_configuration_request), 
			      NULL, 0, (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }

	  break;
	  
	case USB_REQ_GET_INTERFACE:
	  interface_request.interface = index;
	  interface_request.timeout = timeout;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_GET_INTERFACE, 
			      (void *)&interface_request, 
			      sizeof(usb_interface_request), 
			      (void *)&interface_request, 
			      sizeof(usb_interface_request), 
			      (DWORD *)&ret, NULL))
	    {
	      error = 1;
	      break;
	    }

	  *bytes = (unsigned char) interface_request.altsetting;
	  
	  break;
      
	case USB_REQ_SET_INTERFACE:
	  interface_request.interface = index;
	  interface_request.altsetting = value;
	  interface_request.timeout = timeout;
	  
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_SET_INTERFACE, 
			      (void *)&interface_request, 
			      sizeof(usb_interface_request), 
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
    
      vendor_request.request = request;
      vendor_request.value = value;
      vendor_request.index = index;
      vendor_request.timeout = timeout;

      if((unsigned char) requesttype & 0x80)
	{
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_VENDOR_READ, 
			      (void *)&vendor_request, 
			      sizeof(usb_vendor_request), 
			      (void *)bytes, size, 
			      (DWORD *)&ret, NULL))
	    {
	      error = 1;
	    }
	}
      else
	{
	  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_VENDOR_WRITE, 
			      (void *)&vendor_request, 
			      sizeof(usb_vendor_request), 
			      (void *)bytes, size, 
			      (DWORD *)&ret, NULL))
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
      USB_ERROR_STR(-1, "error sending control message: %s", last_win_error());
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
  usb_pipe_request request = { (int)ep, WIN_DEFAULT_TIMEOUT };

  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_ENDPOINT, 
		      &request, sizeof(usb_pipe_request),
		      NULL, 0, (DWORD *)&ret, NULL))
    {
      USB_ERROR_STR(-1, "could not reset ep %d : %s", ep, 
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
      USB_ERROR_STR(ret, "could not clear/halt ep %d : %s\n", ep,
		    last_win_error());
    }
  return 0;
}

int usb_reset(usb_dev_handle *dev)
{
  int ret;
  usb_device_request request = { WIN_DEFAULT_TIMEOUT };
  
  if(!DeviceIoControl(dev->impl_info, LIBUSB_IOCTL_RESET_DEVICE,
		      &request, sizeof(usb_device_request), NULL, 0,
		      (DWORD *)&ret, NULL))
    {
      USB_ERROR_STR(-1, "could not reset : %s\n", last_win_error());
    }
  return 0;
}
