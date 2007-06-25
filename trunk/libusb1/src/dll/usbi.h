#ifndef __BACKEND_H__
#define __BACKEND_H__

#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifdef interface
#undef interface
#endif

/* standard definitions from the USB spec */

/* descriptor types */
#define USBI_DESC_TYPE_DEVICE		 0x01
#define USBI_DESC_TYPE_CONFIG		 0x02
#define USBI_DESC_TYPE_STRING		 0x03
#define USBI_DESC_TYPE_INTERFACE 0x04
#define USBI_DESC_TYPE_ENDPOINT	 0x05
#define USBI_DESC_TYPE_HID		   0x21
#define USBI_DESC_TYPE_REPORT		 0x22
#define USBI_DESC_TYPE_PHYSICAL	 0x23
#define USBI_DESC_TYPE_HUB		   0x29

#define USBI_DESC_LEN_DEVICE		18
#define USBI_DESC_LEN_CONFIG		9
#define USBI_DESC_LEN_INTERFACE 9
#define USBI_DESC_LEN_ENDPOINT	7
#define USBI_DESC_LEN_HID	      9

/* endpoint */
#define USBI_ENDPOINT_TYPE_CONTROL	   0
#define USBI_ENDPOINT_TYPE_ISOCHRONOUS 1
#define USBI_ENDPOINT_TYPE_BULK		     2
#define USBI_ENDPOINT_TYPE_INTERRUPT	 3


/* standard device requests */
#define USBI_REQ_GET_STATUS		     0x00
#define USBI_REQ_CLEAR_FEATURE		 0x01
/* 0x02 is reserved */
#define USBI_REQ_SET_FEATURE		   0x03
/* 0x04 is reserved */
#define USBI_REQ_SET_ADDRESS		   0x05
#define USBI_REQ_GET_DESCRIPTOR		 0x06
#define USBI_REQ_SET_DESCRIPTOR		 0x07
#define USBI_REQ_GET_CONFIGURATION 0x08
#define USBI_REQ_SET_CONFIGURATION 0x09
#define USBI_REQ_GET_INTERFACE		 0x0A
#define USBI_REQ_SET_INTERFACE		 0x0B
#define USBI_REQ_SYNCH_FRAME       0x0C

#define USBI_TYPE_STANDARD   (0x00 << 5)
#define USBI_TYPE_CLASS		   (0x01 << 5)
#define USBI_TYPE_VENDOR	   (0x02 << 5)
#define USBI_TYPE_RESERVED   (0x03 << 5)

#define USBI_RECIP_DEVICE		 0x00
#define USBI_RECIP_INTERFACE 0x01
#define USBI_RECIP_ENDPOINT	 0x02
#define USBI_RECIP_OTHER		 0x03
#define USBI_DIRECTION_IN    0x80
#define USBI_DIRECTION_OUT   0x00

#define USBI_ENDPOINT_IN(ep) ((ep) & USBI_DIRECTION_IN)
#define USBI_ENDPOINT_OUT(ep) (!USBI_ENDPOINT_IN(ep))

#define USBI_REQ_TYPE(request_type) ((request_type) & (0x03 << 5))
#define USBI_REQ_RECIPIENT(request_type) ((request_type) & 0x1F)
#define USBI_REQ_IN(request_type) ((request_type) & 0x80)
#define USBI_REQ_OUT(request_type) (!USBI_REQ_IN(request_type))


#define USBI_DEBUG_ERROR(format, ...)                          \
  _usbi_debug_printf(stderr, USBI_DEBUG_LEVEL_ERROR,           \
                     "LIBUSB-DLL - error: %s(): " format "\n", \
                     __FUNCTION__, ##__VA_ARGS__)

#define USBI_DEBUG_TRACE(format, ...)                          \
  _usbi_debug_printf(stdout, USBI_DEBUG_LEVEL_TRACE,           \
                     "LIBUSB-DLL - trace: %s(): " format "\n", \
                      __FUNCTION__, ##__VA_ARGS__)
     
#define USBI_DEBUG_ASSERT(condition, message, ret) \
  if(!(condition)) { USBI_DEBUG_ERROR("%s", message); return ret; }

#define USBI_DEBUG_ASSERT_PARAM(condition, param, ret) \
  USBI_DEBUG_ASSERT(condition, "invalid parameter '" #param "'", ret);




#define USBI_DEFAULT_TIMEOUT 1000

#include <pshpack1.h> /* ensure bype packed structures */

/* descriptors */
typedef struct {
  uint8_t bLength;
  uint8_t bDescriptorType;
} usbi_descriptor_header_t;

typedef struct {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint16_t bcdUSB;
  uint8_t  bDeviceClass;
  uint8_t  bDeviceSubClass;
  uint8_t  bDeviceProtocol;
  uint8_t  bMaxPacketSize0;
  uint16_t idVendor;
  uint16_t idProduct;
  uint16_t bcdDevice;
  uint8_t  iManufacturer;
  uint8_t  iProduct;
  uint8_t  iSerialNumber;
  uint8_t  bNumConfigurations;
} usbi_device_descriptor_t;

typedef struct {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint16_t wTotalLength;
  uint8_t  bNumInterfaces;
  uint8_t  bConfigurationValue;
  uint8_t  iConfiguration;
  uint8_t  bmAttributes;
  uint8_t  bMaxPower;
} usbi_config_descriptor_t;

typedef struct {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bInterfaceNumber;
  uint8_t bAlternateSetting;
  uint8_t bNumEndpoints;
  uint8_t bInterfaceClass;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;
  uint8_t iInterface;
} usbi_interface_descriptor_t;

typedef struct {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bEndpointAddress;
  uint8_t  bmAttributes;
  uint16_t wMaxPacketSize;
  uint8_t  bInterval;
} usbi_endpoint_descriptor_t;

typedef struct {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint16_t bcdHID;
  uint8_t  bCountryCode;
  uint8_t  bNumDescriptors;
  uint8_t  bClassDescriptorType;
  uint16_t wClassDescriptorLength;
} usbi_hid_descriptor_t;

typedef struct {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  wchar_t wData[1];
} usbi_string_descriptor_t;

#include <poppack.h> /* restore old structure packing */


/* usbi API */

/* usbi error/status codes */
#define USBI_STATUS_SUCCESS        0 /* request completed successfully */
#define USBI_STATUS_PENDING       -1 /* request is still pending */
#define USBI_STATUS_PARAM         -2 /* invalid parameter */
#define USBI_STATUS_BUSY          -3 /* device or resource busy */
#define USBI_STATUS_NOMEM         -4 /* memory allocation failed */ 
#define USBI_STATUS_TIMEOUT       -5 /* request timed out */
#define USBI_STATUS_NODEV         -6 /* no such device */
#define USBI_STATUS_STATE         -7 /* invalid device state */
#define USBI_STATUS_NOT_SUPPORTED -8 /* feature not supported */
#define USBI_STATUS_UNKNOWN       -9 /* unknown error */

#define USBI_SUCCESS(ret) (ret >= USBI_STATUS_SUCCESS)

#define USBI_FIRST_ID 2


typedef enum {
  USBI_TRANSFER_CONTROL,
  USBI_TRANSFER_BULK,
  USBI_TRANSFER_INTERRUPT,
  USBI_TRANSFER_ISOCHRONOUS
} usbi_transfer_t;

typedef enum {
  USBI_DEBUG_LEVEL_NONE,
  USBI_DEBUG_LEVEL_ERROR,
  USBI_DEBUG_LEVEL_TRACE,
  USBI_DEBUG_LEVEL_ALL
} usbi_debug_level_t;

#define USBI_DEVICE_NAME_SIZE 512

typedef struct usbi_device_t {
  int driver; 
  int id;
  char name[USBI_DEVICE_NAME_SIZE];
  struct { 
    int value;
    int desc_index;
    int desc_size; 
    void *desc; 
  } config;
  struct {
    int value;
    HANDLE mutex;
  } interface;
} *usbi_device_t;

typedef struct usbi_io_t {
  usbi_device_t dev;
  int endpoint;
  usbi_transfer_t type;
  int direction; 
  int size;
} *usbi_io_t;

/* backend API */

#define USBI_DEFINE_BACKEND_INTERFACE(backend)                             \
  int backend##_init(void);                                                \
  int backend##_deinit(void);                                              \
  int backend##_set_debug(usbi_debug_level_t level);                       \
  int backend##_get_name(int index, char *name, int size);                 \
  int backend##_open(backend##_device_t dev, const char *name);            \
  int backend##_close(backend##_device_t dev);                             \
  int backend##_reset(backend##_device_t dev);                             \
  int backend##_reset_endpoint(backend##_device_t dev, int endpoint);      \
  int backend##_set_configuration(backend##_device_t dev, int value);      \
  int backend##_set_interface(backend##_device_t dev, int interface,       \
                              int altsetting);                             \
  int backend##_claim_interface(backend##_device_t dev, int interface);    \
  int backend##_release_interface(backend##_device_t dev, int interface);  \
  int backend##_control_msg(backend##_device_t dev, int request_type,      \
                            int request, int value, int index, void *data, \
                            int size, backend##_io_t io);                  \
  int backend##_transfer(backend##_device_t dev, int endpoint,             \
                         usbi_transfer_t type, void *data, int size,       \
                         int packet_size, backend##_io_t io);              \
  int backend##_wait(backend##_device_t dev, backend##_io_t io,            \
                     int timeout);                                         \
  int backend##_poll(backend##_device_t dev, backend##_io_t io);           \
  int backend##_cancel(backend##_device_t dev, backend##_io_t io)



/* initializes the backend */
/* params: none */
/* return: status code */
int usbi_init(void);

/* deinitializes the backend */
/* params: none */
/* return: status code */
int usbi_deinit(void);

/* sets the backend's debug level */
/* params: debug level to set */
/* return: status code */
int usbi_set_debug(usbi_debug_level_t level);
void _usbi_debug_printf(FILE *stream, usbi_debug_level_t level, 
                        const char *format, ...);


void usbi_refresh_ids(void);
int usbi_get_first_id(void);
int usbi_get_next_id(int id);
int usbi_get_prev_id(int id);
int usbi_open(int id, usbi_device_t *dev);

int usbi_get_id(usbi_device_t dev, int *id);


/* closes a device */
/* params: dev: device to close */
/* return: status code */
int usbi_close(usbi_device_t dev);

/* resets a device, performs a bus reset */
/* params: dev: device to reset */
/* return: status code */
int usbi_reset(usbi_device_t dev);

/* resets an endpoint, clears stall condition, resets data toggle */
/* params: dev: device handle */
/*         endpoint: endpoint to reset */
/* return: status code */
int usbi_reset_endpoint(usbi_device_t dev, int endpoint);

/* selects a configuration */
/* params: dev:   device handle */
/*         value: the descriptor's bConfigurationValue */
/* return: status code */
int usbi_set_configuration(usbi_device_t dev, int value);

/* selects an alternate setting, */
/* params: dev:   device handle */
/*         interface: the descriptor's bInterfaceNumber value */
/*         altsetting: the descriptor's bAlternateSetting value */
/* return: status code */
int usbi_set_interface(usbi_device_t dev, int interface, int altsetting);

/* claims an interface */
/* params: dev:   device handle */
/*         interface: the descriptor's bInterfaceNumber value */
/* return: status code */
int usbi_claim_interface(usbi_device_t dev, int interface);

/* releases an interface */
/* params: dev:   device handle */
/*         interface: the descriptor's bInterfaceNumber value */
/* return: status code */
int usbi_release_interface(usbi_device_t dev, int interface);

/* sends a control message to the device */
/* params: dev:  device handle */
/*         request_type: bmRequestType */
/*         request: bRequest */
/*         value:   wValue */
/*         index:   wIndex */
/*         data: data to read/write */
/*         size: size of data (wLength) */
/*         io: pointer to an IO handle (return value) */
/* return: status code */
int usbi_control_msg(usbi_device_t dev, int request_type, int request, 
                     int value, int index, void *data, int size, 
                     usbi_io_t *io);

/* sends a bulk, interrupt, or isochronous read/write request */
/* params: dev:   device handle */
/*         ep:    endpoint */
/*         data:  data to read/write */
/*         size:  size of data */
/*         packet_size: isochronous only, packet size to use */
/*         io: pointer to an io handle (return value) */
/* return: status code */
int usbi_transfer(usbi_device_t dev, int endpoint, usbi_transfer_t type,
                  void *data, int size, int packet_size, usbi_io_t *io);

/* waits for an IO request to complete, cancels and frees the request after */
/* 'timeout' expires */
/* params: io:  handle to IO request*/
/*         timeout: timeout value */
/* return: error/status code or the number of bytes transferred */
int usbi_wait(usbi_io_t io, int timeout);

/* polls an IO request's status and frees the request if it is completed */
/* params: io: IO request */
/* return: error/status code or the number of bytes transferred */
/*         returns USBI_STATUS_PENDING if the request is still pending */
int usbi_poll(usbi_io_t io);

/* cancels and frees an IO request */
/* params: io:  IO request to cancel */
/* return: error/status code */
int usbi_cancel(usbi_io_t io);


/* miscellaneous, driver independent functions */
void usbi_unicode_to_ansi(wchar_t *in, char *out, int outlen);
int usbi_control_msg_sync(usbi_device_t dev, int request_type, int request, 
                          int value, int index, void *data, int size,
                          int timeout);
int usbi_transfer_sync(usbi_device_t dev, int endpoint, usbi_transfer_t type,
                       void *data, int size, int packet_size, int timeout);

int usbi_get_configuration(usbi_device_t dev, int *config);
int usbi_get_interface(usbi_device_t dev, int interface, int *alt_setting);
int usbi_get_device_descriptor(usbi_device_t dev, void *descriptor, int size);
int usbi_get_config_descriptor(usbi_device_t dev, int config, 
                               void *descriptor, int size);
int usbi_get_interface_descriptor(usbi_device_t dev, int config, 
                                  int interface, int alt_setting,
                                  void *descriptor, int size);
int usbi_get_endpoint_descriptor(usbi_device_t dev, int config, 
                                 int interface, int alt_setting,
                                 int endpoint, void *descriptor, int size);
int usbi_get_string_descriptor(usbi_device_t dev, int index, int lang_id,
                               void *descriptor, int size);
int usbi_get_string(usbi_device_t dev, int index, char *string, int size);
int usbi_clear_halt(usbi_device_t dev, int endpoint);

int usbi_claim_interface_simple(usbi_device_t dev, int interface);
int usbi_release_interface_simple(usbi_device_t dev, int interface);
int usbi_get_claimed_interface(usbi_device_t dev, int *interface);

/* install.c */
int usbi_install_inf_file(const char *inf_file);
int usbi_install_touch_inf_file(const char *inf_file);
int usbi_install_needs_restart(void);

#endif
