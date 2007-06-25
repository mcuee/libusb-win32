#include <stddef.h>
#include <stdio.h>
#include <windows.h>
#include <winioctl.h>
#include "usbi_backend_libusb0.h"
#include "driver_api.h"


#define LIBUSB0_DEVICE_PREFIX "\\\\.\\libusb0-"

static int _libusb0_abort_ep(libusb0_device_t dev, int endpoint);

static int _libusb0_abort_ep(libusb0_device_t dev, int endpoint)
{
  libusb_request req;

  memset(&req, 0, sizeof(req));

  req.endpoint.endpoint = endpoint;
  req.timeout = USBI_DEFAULT_TIMEOUT;

  return winio_ioctl_sync(dev->wdev, LIBUSB_IOCTL_ABORT_ENDPOINT, 
                          &req, sizeof(libusb_request), NULL, 0, -1);
}


int libusb0_init(void)
{
  return USBI_STATUS_SUCCESS;
}

int libusb0_deinit(void)
{
  return USBI_STATUS_SUCCESS;
}

int libusb0_set_debug(usbi_debug_level_t level)
{
  return USBI_STATUS_SUCCESS;
}

int libusb0_get_name(int index, char *name, int size)
{
  int i;
  char tname[512];
  int ret;
  winio_device_t dev;
  
  for(i = 0; i < LIBUSB_MAX_NUMBER_OF_DEVICES; i++) {
    snprintf(tname, sizeof(tname) - 1, "%s%04d", LIBUSB0_DEVICE_PREFIX, i);
    if((ret = winio_open_name(&dev, tname)) >= 0) {
      if(!index) {
        strcpy(name, tname);
        winio_close(dev);
        return USBI_STATUS_SUCCESS;
      }
      winio_close(dev);
      index--;
    }
  }

  return USBI_STATUS_NODEV;
}

int libusb0_open(libusb0_device_t dev, const char *name)
{
  return winio_open_name(&dev->wdev, name);
}

int libusb0_close(libusb0_device_t dev)
{
  return winio_close(dev->wdev);
}

int libusb0_reset(libusb0_device_t dev)
{
  libusb_request req;

  memset(&req, 0, sizeof(req));
  req.timeout = USBI_DEFAULT_TIMEOUT;

  return winio_ioctl_sync(dev->wdev, LIBUSB_IOCTL_RESET_DEVICE, 
                          &req, sizeof(libusb_request), NULL, 0, -1);
}

int libusb0_reset_endpoint(libusb0_device_t dev, int endpoint)
{
  libusb_request req;

  memset(&req, 0, sizeof(req));

  req.endpoint.endpoint = endpoint;
  req.timeout = USBI_DEFAULT_TIMEOUT;

  return winio_ioctl_sync(dev->wdev, LIBUSB_IOCTL_RESET_ENDPOINT, 
                         &req, sizeof(libusb_request), NULL, 0, -1);
}

int libusb0_set_configuration(libusb0_device_t dev, int value)
{
  libusb_request req;

  memset(&req, 0, sizeof(req));
  req.configuration.configuration = value;
  req.timeout = USBI_DEFAULT_TIMEOUT;

  return winio_ioctl_sync(dev->wdev, LIBUSB_IOCTL_SET_CONFIGURATION, 
                          &req, sizeof(libusb_request), NULL, 0, -1);
}

int libusb0_set_interface(libusb0_device_t dev, int interface, int altsetting)
{
  libusb_request req;

  memset(&req, 0, sizeof(req));
  req.interface.interface = interface;
  req.interface.altsetting = altsetting;
  req.timeout = USBI_DEFAULT_TIMEOUT;

  return winio_ioctl_sync(dev->wdev, LIBUSB_IOCTL_SET_INTERFACE, 
                          &req, sizeof(libusb_request), NULL, 0, -1);
}

int libusb0_claim_interface(libusb0_device_t dev, int interface)
{
  libusb_request req;

  memset(&req, 0, sizeof(req));
  req.interface.interface = interface;

  return winio_ioctl_sync(dev->wdev, LIBUSB_IOCTL_CLAIM_INTERFACE, 
                          &req, sizeof(libusb_request), NULL, 0, -1);
}

int libusb0_release_interface(libusb0_device_t dev, int interface)
{
  libusb_request req;

  memset(&req, 0, sizeof(req));
  req.interface.interface = interface;

  return winio_ioctl_sync(dev->wdev, LIBUSB_IOCTL_RELEASE_INTERFACE, 
                          &req, sizeof(libusb_request), NULL, 0, -1);
}

int libusb0_control_msg(libusb0_device_t dev, int request_type, int request, 
                        int value, int index, void *data, int size, 
                        libusb0_io_t io)
{
  int ret;
  int code;
  int req_size;
  libusb_request *req;

  io->req = NULL;

  req_size = USBI_REQ_OUT(request_type) ? sizeof(libusb_request) + size
    : sizeof(libusb_request);
  
  if(!(req = malloc(req_size)))
    return USBI_STATUS_NOMEM;

  if(USBI_REQ_OUT(request_type))
    memcpy((char *)req + sizeof(libusb_request), data, size);

  req->timeout = USBI_DEFAULT_TIMEOUT;

  switch(USBI_REQ_TYPE(request_type))
    {
    case USBI_TYPE_STANDARD:      
      switch(request)
        {
        case USBI_REQ_GET_STATUS: 
          req->status.recipient = USBI_REQ_RECIPIENT(request_type);
          req->status.index = index;
          code = LIBUSB_IOCTL_GET_STATUS;
          break;
      
        case USBI_REQ_CLEAR_FEATURE:
          req->feature.recipient = USBI_REQ_RECIPIENT(request_type);
          req->feature.feature = value;
          req->feature.index = index;
          code = LIBUSB_IOCTL_CLEAR_FEATURE;
          break;
	  
        case USBI_REQ_SET_FEATURE:
          req->feature.recipient = USBI_REQ_RECIPIENT(request_type);
          req->feature.feature = value;
          req->feature.index = index;
          code = LIBUSB_IOCTL_SET_FEATURE;
          break;

        case USBI_REQ_GET_DESCRIPTOR:
          req->descriptor.recipient = USBI_REQ_RECIPIENT(request_type);
          req->descriptor.type = (value >> 8) & 0xFF;
          req->descriptor.index = value & 0xFF;
          req->descriptor.language_id = index;
          code = LIBUSB_IOCTL_GET_DESCRIPTOR;
          break;
	  
        case USBI_REQ_SET_DESCRIPTOR:
          req->descriptor.recipient = USBI_REQ_RECIPIENT(request_type);
          req->descriptor.type = (value >> 8) & 0xFF;
          req->descriptor.index = value & 0xFF;
          req->descriptor.language_id = index;
          code = LIBUSB_IOCTL_SET_DESCRIPTOR;
          break;
	  
        case USBI_REQ_GET_CONFIGURATION:
          code = LIBUSB_IOCTL_GET_CONFIGURATION;
          break;
      
        case USBI_REQ_SET_CONFIGURATION:	  
          req->configuration.configuration = value;
          code = LIBUSB_IOCTL_SET_CONFIGURATION;
          break;
	  
        case USBI_REQ_GET_INTERFACE:
          req->interface.interface = index;
          code = LIBUSB_IOCTL_GET_INTERFACE;	  
          break;
      
        case USBI_REQ_SET_INTERFACE:
          req->interface.interface = index;
          req->interface.altsetting = value;
          code = LIBUSB_IOCTL_SET_INTERFACE;	  
          break;
	  
        default:
          free(req);
          return USBI_STATUS_PARAM;
        }
      break;

    case USBI_TYPE_VENDOR:  
    case USBI_TYPE_CLASS:

      req->vendor.type = USBI_REQ_TYPE(request_type) >> 5;
      req->vendor.recipient = USBI_REQ_RECIPIENT(request_type);
      req->vendor.request = request;
      req->vendor.value = value;
      req->vendor.index = index;

      if(USBI_REQ_IN(request_type))
        code = LIBUSB_IOCTL_VENDOR_READ;
      else
        code = LIBUSB_IOCTL_VENDOR_WRITE;
      break;

    default:
      free(req);
      return USBI_STATUS_PARAM;
    }

  if(USBI_REQ_OUT(request_type)) {
    data = NULL;
    size = 0;
  }

  io->req = req;

  if((ret = winio_ioctl_async(dev->wdev, code, req, req_size, 
                              data, size, &io->wio)) < 0) {
    free(req);
    io->req = NULL;
  }

  return ret;
}

int libusb0_transfer(libusb0_device_t dev, int endpoint, usbi_transfer_t type,
                     void *data, int size, int packet_size, libusb0_io_t io)
{
  int ret;
  int code;
  libusb_request *req;

  io->req = NULL;

  if(!(req = malloc(sizeof(libusb_request))))
    return USBI_STATUS_NOMEM;
  
  req->endpoint.endpoint = endpoint;
  req->endpoint.packet_size = packet_size;

  switch(type) {
  case USBI_TRANSFER_BULK:
  case USBI_TRANSFER_INTERRUPT:
    code = USBI_ENDPOINT_IN(endpoint) ? LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ 
      : LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE;
    break;
  case USBI_TRANSFER_ISOCHRONOUS:
    code = USBI_ENDPOINT_IN(endpoint) ? LIBUSB_IOCTL_ISOCHRONOUS_READ
      : LIBUSB_IOCTL_ISOCHRONOUS_WRITE;
    break;
  default:
    free(req);
    return USBI_STATUS_PARAM;
  }

  io->req = req;

  if((ret = winio_ioctl_async(dev->wdev, code, req, sizeof(libusb_request), 
                              data, size, &io->wio) < 0)) {
    free(req);
    io->req = NULL;
  }

  return ret;
}

int libusb0_wait(libusb0_device_t dev, libusb0_io_t io, int timeout)
{
  int ret;
  ret = winio_wait(dev->wdev, io->wio, timeout);
  if(io->req)
    free(io->req);
  if(ret >= 0 && io->base.type == USBI_TRANSFER_CONTROL 
     && io->base.direction == USBI_DIRECTION_OUT)
    ret = io->base.size;
  return ret;
}

int libusb0_poll(libusb0_device_t dev, libusb0_io_t io)
{
  int ret;
  if((ret = winio_poll(dev->wdev, io->wio)) != USBI_STATUS_PENDING)
  if(io->req)
    free(io->req);
  if(ret >= 0 && io->base.type == USBI_TRANSFER_CONTROL 
     && io->base.direction == USBI_DIRECTION_OUT)
    ret = io->base.size;
  return ret;
}

int libusb0_cancel(libusb0_device_t dev, libusb0_io_t io)
{
  _libusb0_abort_ep(dev->wdev, io->base.endpoint);
  libusb0_wait(dev, io, INFINITE);
  return USBI_STATUS_SUCCESS;
}
