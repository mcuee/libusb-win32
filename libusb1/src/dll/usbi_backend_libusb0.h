#ifndef __USBI_BACKEND_LIBUSB0_H__
#define __USBI_BACKEND_LIBUSB0_H__

#include "usbi.h"
#include "usbi_winio.h"
#include "driver_api.h"


typedef struct libusb0_device_t {
  struct usbi_device_t base;
  winio_device_t wdev;
} *libusb0_device_t;

typedef struct libusb0_io_t {
  struct usbi_io_t base;
  winio_io_t wio;
  libusb_request *req;
} *libusb0_io_t;

USBI_DEFINE_BACKEND_INTERFACE(libusb0);

#endif
