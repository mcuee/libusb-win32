#include "usb.h"
#include "types.h"
#include "fw_descriptors.h"
#include <stddef.h>

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b) 
#define ALIGN_16(a) \
  const uint8_t CONCAT(align, __LINE__)[sizeof(a) & 1 ? 1 : 2] = {0}


#ifdef HID

#define DESC_VID 0x1234
#define DESC_PID 0x0002
#define DESC_CONFIG_SIZE (USB_DT_CONFIG_SIZE  \
	 + USB_DT_INTERFACE_SIZE + USB_DT_HID_SIZE + USB_DT_ENDPOINT_SIZE * 2)

#else

#define DESC_VID 0x1234
#define DESC_PID 0x0001
#define DESC_CONFIG_SIZE (USB_DT_CONFIG_SIZE  \
	 + USB_DT_INTERFACE_SIZE * 6 \
   + USB_DT_ENDPOINT_SIZE * 6 * 2)

#endif




#define DESC_CONFIG(value)              \
  USB_DT_CONFIG_SIZE,         /* bLength */             \
  USB_DT_CONFIG,              /* bDescriptorType */     \
  SPLIT_16(DESC_CONFIG_SIZE), /* wTotalLength */        \
  FW_NUM_INTERFACES,          /* bNumInterfaces */      \
  value,                      /* bConfigurationValue */ \
  0x00,                       /* iConfiguration */      \
  0x80,                       /* bmAttributs */         \
  0x0F                        /* MaxPower */


#define DESC_INTERFACE(num, alt, num_endpoints)   \
  USB_DT_INTERFACE_SIZE, /* bLength */            \
  USB_DT_INTERFACE,      /* bDescriptorType */    \
  num,                   /* bInterfaceNumber */   \
  alt,                   /* bAlternateSetting */  \
  num_endpoints,         /* bNumEndpoints */      \
  0xFF,                  /* bInterfaceClass */    \
  0xFF,                  /* bInterfaceSubClass */ \
  0xFF,                  /* bInterfaceProtocol */ \
  0x00                   /* iInterface */

#define DESC_INTERFACE_HID()   \
  USB_DT_INTERFACE_SIZE, /* bLength */            \
  USB_DT_INTERFACE,      /* bDescriptorType */    \
  FW_INTERFACE_0, /* bInterfaceNumber */   \
  0x00,                  /* bAlternateSetting */  \
  0x02,                  /* bNumEndpoints */      \
  0x03,                  /* bInterfaceClass */    \
  0x00,                  /* bInterfaceSubClass */ \
  0x00,                  /* bInterfaceProtocol */ \
  0x00                   /* iInterface */

#define DESC_ENDPOINT(address, attr, size, interval) \
  USB_DT_ENDPOINT_SIZE, /* bLength */                \
  USB_DT_ENDPOINT,      /* bDescriptorType */        \
  address,              /* bEndpointAddress */       \
  attr,                 /* bmAttributes */           \
  SPLIT_16(size),       /* wMaxPacketSize */         \
  interval              /* bInterval */ 


#pragma constseg USBDESC

#ifdef HID

static const uint8_t desc_device[] = { 
  USB_DT_DEVICE_SIZE,   /* bLength */
  USB_DT_DEVICE,        /* bDescriptorType */
  SPLIT_16(0x0200),     /* bcdUSB */
  0xFF,                 /* bDeviceClass (vendor specific) */
  0xFF,                 /* bDeviceSubClass */
  0xFF,                 /* bDeviceProtocol (vendor specific) */
  0x40,                 /* bMaxPacketSize0 */
  SPLIT_16(DESC_VID),   /* idVendor */
  SPLIT_16(DESC_PID),   /* idProduct */
  SPLIT_16(0x0102),     /* bcdDevice */
  0x00,                 /* iManufacturer */
  0x00,                 /* iProduct */
  0x00,                 /* iSerialNumber */
  0x01                  /* bNumConfigurations */
};

ALIGN_16(desc_device);
static const uint8_t desc_report[] = {
  0x06, 0xA0, 0xFF, /* usage page (0xFFA0 == vendor defined) */
  0x09, 0x01,       /* usage (vendor defined) */
  0xA1, 0x01,       /* start collection (application) */
  /* input report */
  0x09, 0x01,       /* usage (vendor defined) */
  0x15, 0x00,       /* logical minimum (0) */ 
  0x25, 0xFF,       /* logical maximum (255) */
  0x75, 0x08,       /* report size (8 bits) */
  0x95, 64,         /* report count */
  0x81, 0x00,       /* input (data, variable, absolute) */
  /* output report */
  0x09, 0x02,       /* usage (vendor defined) */
  0x15, 0x00,       /* logical minimum (0) */ 
  0x25, 0xFF,       /* logical maximum (255) */
  0x75, 0x08,       /* report size (8 bits) */
  0x95, 64,         /* report count */
  0x91, 0x00,       /* output (data, variable, absolute) */
  0xC0              /* end collection */
};

ALIGN_16(desc_report);
static const uint8_t desc_config_1[] = 
{
  /* configuration 1 */
  DESC_CONFIG(FW_CONFIG_0),

  /* interface 0/0 */
  DESC_INTERFACE_HID(),

  /* hid descriptor */
  USB_DT_HID_SIZE,     /* bLength */
  USB_DT_HID,          /* bDescriptorType */
  SPLIT_16(0x0110),    /* bcdHID */
  0x00,                /* bCountryCode */
  0x01,                /* bNumDescriptors */
  USB_DT_REPORT,       /* bClassDescriptorType */
  SPLIT_16(sizeof(desc_report)), /* wClassDescriptorLength */

  DESC_ENDPOINT(0x82, USB_ENDPOINT_TYPE_INTERRUPT, 64, 10),
  DESC_ENDPOINT(0x04, USB_ENDPOINT_TYPE_INTERRUPT, 64, 10),
};

#else

static const uint8_t desc_device[] = 
{ 
  USB_DT_DEVICE_SIZE,   /* bLength */
  USB_DT_DEVICE,        /* bDescriptorType */
  SPLIT_16(0x0200),     /* bcdUSB */
  0xFF,                 /* bDeviceClass (vendor specific) */
  0xFF,                 /* bDeviceSubClass */
  0xFF,                 /* bDeviceProtocol (vendor specific) */
  0x40,                 /* bMaxPacketSize0 */
  SPLIT_16(DESC_VID),   /* idVendor */
  SPLIT_16(DESC_PID),   /* idProduct */
  SPLIT_16(0x0102),     /* bcdDevice */
  0x00,                 /* iManufacturer */
  0x00,                 /* iProduct */
  0x00,                 /* iSerialNumber */
  0x02                  /* bNumConfigurations */
};

ALIGN_16(desc_device);
static const uint8_t desc_config_1[] = 
{
  /* configuration 1 */
  DESC_CONFIG(FW_CONFIG_0),

  /* interface 0/0 */
  DESC_INTERFACE(FW_INTERFACE_0, 0, 2),
  DESC_ENDPOINT(0x82, USB_ENDPOINT_TYPE_BULK, 64, 0),
  DESC_ENDPOINT(0x04, USB_ENDPOINT_TYPE_BULK, 64, 0),
  /* interface 0/1 */
  DESC_INTERFACE(FW_INTERFACE_0, 1, 2),
  DESC_ENDPOINT(0x82, USB_ENDPOINT_TYPE_INTERRUPT, 64, 1),
  DESC_ENDPOINT(0x04, USB_ENDPOINT_TYPE_INTERRUPT, 64, 1),
  /* interface 0/2 */
  DESC_INTERFACE(FW_INTERFACE_0, 2, 2),
  DESC_ENDPOINT(0x82, USB_ENDPOINT_TYPE_ISOCHRONOUS, 64, 1),
  DESC_ENDPOINT(0x04, USB_ENDPOINT_TYPE_ISOCHRONOUS, 64, 1),

  /* interface 1/0 */
  DESC_INTERFACE(FW_INTERFACE_1, 0, 2),
  DESC_ENDPOINT(0x86, USB_ENDPOINT_TYPE_BULK, 64, 0),
  DESC_ENDPOINT(0x08, USB_ENDPOINT_TYPE_BULK, 64, 0),
  /* interface 1/1 */
  DESC_INTERFACE(FW_INTERFACE_1, 1, 2),
  DESC_ENDPOINT(0x86, USB_ENDPOINT_TYPE_INTERRUPT, 64, 1),
  DESC_ENDPOINT(0x08, USB_ENDPOINT_TYPE_INTERRUPT, 64, 1),
  /* interface 1/2 */
  DESC_INTERFACE(FW_INTERFACE_1, 2, 2),
  DESC_ENDPOINT(0x86, USB_ENDPOINT_TYPE_ISOCHRONOUS, 64, 1),
  DESC_ENDPOINT(0x08, USB_ENDPOINT_TYPE_ISOCHRONOUS, 64, 1),
};


ALIGN_16(desc_config_1);
static const uint8_t desc_config_2[] = 
{
  /* configuration 2 */
  DESC_CONFIG(FW_CONFIG_1),
  /* interface 0/0 */
  DESC_INTERFACE(FW_INTERFACE_0, 0, 2),
  DESC_ENDPOINT(0x82, USB_ENDPOINT_TYPE_BULK, 64, 0),
  DESC_ENDPOINT(0x04, USB_ENDPOINT_TYPE_BULK, 64, 0),
  /* interface 0/1 */
  DESC_INTERFACE(FW_INTERFACE_0, 1, 2),
  DESC_ENDPOINT(0x82, USB_ENDPOINT_TYPE_INTERRUPT, 64, 1),
  DESC_ENDPOINT(0x04, USB_ENDPOINT_TYPE_INTERRUPT, 64, 1),
  /* interface 0/2 */
  DESC_INTERFACE(FW_INTERFACE_0, 2, 2),
  DESC_ENDPOINT(0x82, USB_ENDPOINT_TYPE_ISOCHRONOUS, 64, 1),
  DESC_ENDPOINT(0x04, USB_ENDPOINT_TYPE_ISOCHRONOUS, 64, 1),

  /* interface 3/0 */
  DESC_INTERFACE(FW_INTERFACE_1, 0, 2),
  DESC_ENDPOINT(0x86, USB_ENDPOINT_TYPE_BULK, 64, 0),
  DESC_ENDPOINT(0x08, USB_ENDPOINT_TYPE_BULK, 64, 0),
  /* interface 3/1 */
  DESC_INTERFACE(FW_INTERFACE_1, 1, 2),
  DESC_ENDPOINT(0x86, USB_ENDPOINT_TYPE_INTERRUPT, 64, 1),
  DESC_ENDPOINT(0x08, USB_ENDPOINT_TYPE_INTERRUPT, 64, 1),
  /* interface 3/2 */
  DESC_INTERFACE(FW_INTERFACE_1, 2, 2),
  DESC_ENDPOINT(0x86, USB_ENDPOINT_TYPE_ISOCHRONOUS, 64, 1),
  DESC_ENDPOINT(0x08, USB_ENDPOINT_TYPE_ISOCHRONOUS, 64, 1),
};

#endif

/* language ID string descriptor (English-US ) */
#ifdef HID
ALIGN_16(desc_config_1);
#else
ALIGN_16(desc_config_2);
#endif
static const uint8_t desc_string_langid[] = {
  4, 
  USB_DT_STRING, 
  0x09, 
  0x04
};

/* manufacturer ID string */
ALIGN_16(desc_string_langid);
static const uint8_t desc_string_manufacturer[] = {
  14, 
  USB_DT_STRING,
  'l',0,'i',0,'b',0,'u',0,'s',0,'b',0
};

/* product ID string */
ALIGN_16(desc_string_manufacturer);
static const uint8_t desc_string_product[] = {
   38, 
   USB_DT_STRING,
   'l', 0,
   'i', 0,
   'b', 0,
   'u', 0,
   's', 0,
   'b', 0,
   ' ', 0,
   't', 0,
   'e', 0,
   's', 0,
   't', 0,
   ' ', 0,
   'd', 0,
   'e', 0,
   'v', 0,
   'i', 0,
   'c', 0,
   'e', 0
 };

/* serial ID string */
ALIGN_16(desc_string_product);

#ifdef HID

static const uint8_t desc_string_serial[] = {
  20, 
  USB_DT_STRING,
  '1', 0,
  '2', 0,
  '3', 0,
  '4', 0,
  '-', 0,
  '0', 0,
  '0', 0,
  '0', 0,
  '2', 0
};

#else

static const uint8_t desc_string_serial[] = {
  20, 
  USB_DT_STRING,
  '1', 0,
  '2', 0,
  '3', 0,
  '4', 0,
  '-', 0,
  '0', 0,
  '0', 0,
  '0', 0,
  '1', 0
};

#endif

const void *fw_desc_get_device(void)
{
  return desc_device;
}

const void *fw_desc_get_config(uint8_t index)
{
#ifdef HID
    return index ? NULL : desc_config_1;
#else
  switch(index) {
  case 0: return desc_config_1;
  case 1: return desc_config_2;
  default: return NULL;
  }
#endif
}

const void *fw_desc_get_string(uint8_t index)
{
  switch(index) {
  case 0: return desc_string_langid; break;
  case 1: return desc_string_manufacturer; break;
  case 2: return desc_string_product; break;
  case 3: return desc_string_serial; break;
  default: return NULL;
  }
}

#ifdef HID

const void *fw_desc_get_hid(void)
{
  return desc_config_1 + USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE;
}

const void *fw_desc_get_report(void)
{
  return desc_report;
}

uint8_t fw_desc_get_size(void)
{
  return sizeof(desc_report);
}

#endif

