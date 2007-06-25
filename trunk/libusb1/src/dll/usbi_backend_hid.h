#ifndef __USBI_BACKEND_HID_H__
#define __USBI_BACKEND_HID_H__

#include "usbi.h"
#include "usbi_winio.h"
#include <windows.h>


typedef struct hid_device_t {
  struct usbi_device_t base;
  winio_device_t wdev;
  int vid;
  int pid;
  int config;
  int output_report_size;
  int input_report_size;
  int feature_report_size;
  wchar_t man_string[128];
  wchar_t prod_string[128];
  wchar_t ser_string[128];
} *hid_device_t;

typedef struct hid_io_t {
  struct usbi_io_t base;
  winio_io_t wio;
  int ret;
  void *buf;
} *hid_io_t;

USBI_DEFINE_BACKEND_INTERFACE(hid);

#endif
