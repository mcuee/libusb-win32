#ifndef __USBI_BACKEND_WINUSB_H__
#define __USBI_BACKEND_WINUSB_H__

#include "usbi.h"
#include "usbi_winio.h"

#define WINUSB_MAX_INTERFACES 32
#define WINUSB_MAX_ENDPOINTS  32


typedef struct winusb_device_t {
  struct usbi_device_t base;
  winio_device_t wdev;
  struct {
    void *handle;
    int number;
    struct {
      int address;
    } endpoints[WINUSB_MAX_ENDPOINTS];
  } interfaces[WINUSB_MAX_INTERFACES];
} *winusb_device_t;

typedef struct winusb_io_t {
  struct usbi_io_t base;
  winio_io_t wio;
} *winusb_io_t;

USBI_DEFINE_BACKEND_INTERFACE(winusb);

#endif
