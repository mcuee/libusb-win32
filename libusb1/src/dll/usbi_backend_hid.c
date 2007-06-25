#include "usbi_backend_hid.h"
#include "dll_load.h"
#include <windows.h>


/* from the HID spec */
#define HID_REQ_GET_REPORT   0x01 
#define HID_REQ_GET_IDLE     0x02 
#define HID_REQ_GET_PROTOCOL 0x03
#define HID_REQ_SET_REPORT   0x09 
#define HID_REQ_SET_IDLE     0x0A 
#define HID_REQ_SET_PROTOCOL 0x0B 

#define HID_REPORT_TYPE_INPUT   0x01
#define HID_REPORT_TYPE_OUTPUT  0x02
#define HID_REPORT_TYPE_FEATURE 0x03

/* internal limits */
#define HID_MAX_CONFIG_DESC_SIZE \
 USBI_DESC_LEN_CONFIG + USBI_DESC_LEN_INTERFACE + USBI_DESC_LEN_HID \
 + 2 * USBI_DESC_LEN_ENDPOINT
#define HID_MAX_REPORT_SIZE 1024 

#define HID_IN_EP 0x81
#define HID_OUT_EP 0x02

/* hid.dll interface */

#define HIDP_STATUS_SUCCESS  0x110000

#include <pshpack4.h>
typedef struct {
	ULONG Size; 
	USHORT VendorID;
	USHORT ProductID;
	USHORT VersionNumber;
} HIDD_ATTRIBUTES, *PHIDD_ATTRIBUTES;
#include <poppack.h>

typedef USHORT USAGE;
typedef unsigned long NTSTATUS;
typedef void* PHIDP_PREPARSED_DATA;


typedef struct {
  USAGE Usage;
  USAGE UsagePage;
  USHORT InputReportByteLength;
  USHORT OutputReportByteLength;
  USHORT FeatureReportByteLength;
  USHORT Reserved[17];
  USHORT NumberLinkCollectionNodes;
  USHORT NumberInputButtonCaps;
  USHORT NumberInputValueCaps;
  USHORT NumberInputDataIndices;
  USHORT NumberOutputButtonCaps;
  USHORT NumberOutputValueCaps;
  USHORT NumberOutputDataIndices;
  USHORT NumberFeatureButtonCaps;
  USHORT NumberFeatureValueCaps;
  USHORT NumberFeatureDataIndices;
} HIDP_CAPS, *PHIDP_CAPS;

/* functions exported by hid.dll */
DLL_DECLARE(WINAPI, BOOL, HidD_GetAttributes, (HANDLE, PHIDD_ATTRIBUTES));
DLL_DECLARE(WINAPI, VOID, HidD_GetHidGuid, (LPGUID));
DLL_DECLARE(WINAPI, BOOL, HidD_GetPreparsedData, 
            (HANDLE, PHIDP_PREPARSED_DATA *));
DLL_DECLARE(WINAPI, BOOL, HidD_FreePreparsedData, (PHIDP_PREPARSED_DATA));
DLL_DECLARE(WINAPI, BOOL, HidD_GetManufacturerString, (HANDLE, PVOID, ULONG));
DLL_DECLARE(WINAPI, BOOL, HidD_GetProductString, (HANDLE, PVOID, ULONG));
DLL_DECLARE(WINAPI, BOOL, HidD_GetSerialNumberString, (HANDLE, PVOID, ULONG));
DLL_DECLARE(WINAPI, NTSTATUS, HidP_GetCaps, 
            (PHIDP_PREPARSED_DATA, PHIDP_CAPS));
DLL_DECLARE(WINAPI, BOOL, HidD_SetNumInputBuffers, (HANDLE, ULONG));
DLL_DECLARE(WINAPI, BOOL, HidD_SetFeature, (HANDLE, PVOID, ULONG));
DLL_DECLARE(WINAPI, BOOL, HidD_GetFeature, (HANDLE, PVOID, ULONG));
DLL_DECLARE(WINAPI, BOOL, HidD_GetPhysicalDescriptor, (HANDLE, PVOID, ULONG));

/* XP only */
DLL_DECLARE(WINAPI, BOOL, HidD_GetInputReport, (HANDLE, PVOID, ULONG));
DLL_DECLARE(WINAPI, BOOL, HidD_SetOutputReport, (HANDLE, PVOID, ULONG));


/* the HID GUID */
static GUID _hid_guid;


/* special version of wcslen() */
static int _hid_wcslen(wchar_t *str);

/* descriptor helper functions, synchronous */
static int _hid_get_device_descriptor(hid_device_t dev, void *data, int size);
static int _hid_get_config_descriptor(hid_device_t dev, void *data, int size);
static int _hid_get_string_descriptor(hid_device_t dev, int index,
                                      void *data, int size);
static int _hid_get_hid_descriptor(hid_device_t dev, void *data, int size);
static int _hid_get_report_descriptor(hid_device_t dev, void *data, int size);
static int _hid_get_descriptor(hid_device_t dev, int recipient, 
                               int type, int index, void *data, int size);

/* helper functions to handle HID class requests, synchronous */
static int _hid_class_request(hid_device_t dev, int request_type, 
                              int request, int value, int index, 
                              void *data, int size);
static int _hid_get_report(hid_device_t dev, int id, void *data, int size);
static int _hid_set_report(hid_device_t dev, int id, void *data, int size);
static int _hid_get_feature(hid_device_t dev, int id, void *data, int size);
static int _hid_set_feature(hid_device_t dev, int id, void *data, int size);


static int _hid_wcslen(wchar_t *str)
{
  int ret = 0;
  while(*str && *str != 0x409) {
    ret++;
    str++;
  }
  return ret;
}

static int _hid_get_device_descriptor(hid_device_t dev, void *data, int size)
{
  usbi_device_descriptor_t d;

  d.bLength = USBI_DESC_LEN_DEVICE;
  d.bDescriptorType = USBI_DESC_TYPE_DEVICE;
  d.bcdUSB = 0x0200; /* 2.00 */
  d.bDeviceClass = 0;
  d.bDeviceSubClass = 0;
  d.bDeviceProtocol = 0;
  d.bMaxPacketSize0 = 64; /* fix this! */
  d.idVendor = (uint16_t)dev->vid;
  d.idProduct = (uint16_t)dev->pid;
  d.bcdDevice = 0x0100;
  d.iManufacturer = _hid_wcslen(dev->man_string) ? 1 : 0;
  d.iProduct = _hid_wcslen(dev->prod_string) ? 2 : 0;
  d.iSerialNumber  = _hid_wcslen(dev->ser_string) ? 3 : 0;
  d.bNumConfigurations = 1;
  
  if(size > USBI_DESC_LEN_DEVICE)
    size = USBI_DESC_LEN_DEVICE;
  memcpy(data, &d, size);
  return size;
}

static int _hid_get_config_descriptor(hid_device_t dev, void *data, int size)
{
  char num_endpoints = 0;
  int config_total_len = 0;
  char tmp[HID_MAX_CONFIG_DESC_SIZE];
  usbi_config_descriptor_t *cd;
  usbi_interface_descriptor_t *id;
  usbi_hid_descriptor_t *hd;
  usbi_endpoint_descriptor_t *ed;
  
  if(dev->input_report_size)
    num_endpoints++;
  if(dev->output_report_size)
    num_endpoints++;
  
  config_total_len = USBI_DESC_LEN_CONFIG + USBI_DESC_LEN_INTERFACE 
    + USBI_DESC_LEN_HID + num_endpoints * USBI_DESC_LEN_ENDPOINT;

  
  cd = (usbi_config_descriptor_t *)tmp;
  id = (usbi_interface_descriptor_t *)(tmp + USBI_DESC_LEN_CONFIG);
  hd = (usbi_hid_descriptor_t *)(tmp + USBI_DESC_LEN_CONFIG
                                 + USBI_DESC_LEN_INTERFACE);
  ed = (usbi_endpoint_descriptor_t *)(tmp + USBI_DESC_LEN_CONFIG
                                           + USBI_DESC_LEN_INTERFACE 
                                           + USBI_DESC_LEN_HID);

  cd->bLength = USBI_DESC_LEN_CONFIG;
  cd->bDescriptorType = USBI_DESC_TYPE_CONFIG;
  cd->wTotalLength = config_total_len;
  cd->bNumInterfaces = 1;
  cd->bConfigurationValue = 1;
  cd->iConfiguration = 0;
  cd->bmAttributes = 1 << 7; /* bus powered */
  cd->bMaxPower = 50;
  
  id->bLength = USBI_DESC_LEN_INTERFACE;
  id->bDescriptorType = USBI_DESC_TYPE_INTERFACE;
  id->bInterfaceNumber = 0;
  id->bAlternateSetting = 0;
  id->bNumEndpoints = num_endpoints;
  id->bInterfaceClass = 3;
  id->bInterfaceSubClass = 0;
  id->bInterfaceProtocol = 0;
  id->iInterface = 0;
  
  _hid_get_hid_descriptor(dev, hd, USBI_DESC_LEN_HID);
  
  if(dev->input_report_size) {
    ed->bLength = USBI_DESC_LEN_ENDPOINT;
    ed->bDescriptorType = USBI_DESC_TYPE_ENDPOINT;
    ed->bEndpointAddress = HID_IN_EP;
    ed->bmAttributes = 3;
    ed->wMaxPacketSize = dev->input_report_size - 1;
    ed->bInterval = 10;

    ed++;
  }
  
  if(dev->output_report_size) {
    ed->bLength = USBI_DESC_LEN_ENDPOINT;
    ed->bDescriptorType = USBI_DESC_TYPE_ENDPOINT;
    ed->bEndpointAddress = HID_OUT_EP;
    ed->bmAttributes = 3;
    ed->wMaxPacketSize = dev->output_report_size - 1;
    ed->bInterval = 10;
  }
  
  if(size > config_total_len)
    size = config_total_len;
  memcpy(data, tmp, size);
  return size;
}

static int _hid_get_string_descriptor(hid_device_t dev, int index,
                                      void *data, int size)
{
  void *tmp = NULL;
  int tmp_size = 0;

  /* language ID, EN-US */
  char string_langid[] = {
    4, 
    USBI_DESC_TYPE_STRING, 
    0x09, 
    0x04
  };
  
  switch(index) {
  case 0:
    tmp = string_langid;
    tmp_size = 4;
    break;
  case 1:
    tmp = dev->man_string;
    tmp_size = _hid_wcslen(dev->man_string) * sizeof(wchar_t);
    break;
  case 2:
    tmp = dev->prod_string;
    tmp_size = _hid_wcslen(dev->prod_string) * sizeof(wchar_t);
    break;
  case 3:
    tmp = dev->ser_string;
    tmp_size = _hid_wcslen(dev->ser_string) * sizeof(wchar_t);
    break;
  default:
    return USBI_STATUS_PARAM;
  }

  if(!tmp_size)
    return USBI_STATUS_PARAM;

  if(tmp_size > size)
    tmp_size = size;
  memcpy(data, tmp, tmp_size);
  return tmp_size;
}

static int _hid_get_hid_descriptor(hid_device_t dev, void *data, int size)
{
  usbi_hid_descriptor_t d;
  char tmp[256];
  int report_len;

  report_len = _hid_get_report_descriptor(dev, tmp, sizeof(tmp));

  d.bLength = USBI_DESC_LEN_HID;
  d.bDescriptorType = USBI_DESC_TYPE_HID;
  d.bcdHID = 0x0110; /* 1.10 */
  d.bCountryCode = 0;
  d.bNumDescriptors = 1;
  d.bClassDescriptorType = USBI_DESC_TYPE_REPORT;
  d.wClassDescriptorLength = report_len;

  if(size > USBI_DESC_LEN_HID)
    size = USBI_DESC_LEN_HID;
  memcpy(data, &d, size);
  return size;
}

static int _hid_get_report_descriptor(hid_device_t dev, void *data, int size)
{
  unsigned char d[256];
  int i = 0;

  /* usage page (0xFFA0 == vendor defined) */
  d[i++] = 0x06; d[i++] = 0xA0; d[i++] = 0xFF;
  /* usage (vendor defined) */
  d[i++] = 0x09; d[i++] = 0x01;
  /* start collection (application) */
  d[i++] = 0xA1; d[i++] = 0x01;
  /* input report */
  if(dev->input_report_size) {
    /* usage (vendor defined) */
    d[i++] = 0x09; d[i++] = 0x01;
    /* logical minimum (0) */ 
    d[i++] = 0x15; d[i++] = 0x00; 
    /* logical maximum (255) */
    d[i++] = 0x25; d[i++] = 0xFF;
    /* report size (8 bits) */
    d[i++] = 0x75; d[i++] = 0x08;
    /* report count */
    d[i++] = 0x95; d[i++] = (unsigned char)dev->input_report_size - 1;
    /* input (data, variable, absolute) */
    d[i++] = 0x81; d[i++] = 0x00;
  }
  /* output report */
  if(dev->output_report_size) {
    /* usage (vendor defined) */
    d[i++] = 0x09; d[i++] = 0x02;
    /* logical minimum (0) */ 
    d[i++] = 0x15; d[i++] = 0x00; 
    /* logical maximum (255) */
    d[i++] = 0x25; d[i++] = 0xFF;
    /* report size (8 bits) */
    d[i++] = 0x75; d[i++] = 0x08;
    /* report count */
    d[i++] = 0x95; d[i++] = (unsigned char)dev->output_report_size - 1;
    /* output (data, variable, absolute) */
    d[i++] = 0x91; d[i++] = 0x00;
  }
  /* end collection */
  d[i++] = 0xC0;

  if(size > i)
    size = i;
  memcpy(data, d, size);
  return size;
}

static int _hid_get_descriptor(hid_device_t dev, int recipient,
                               int type, int index, void *data, int size)
{
  switch(type) {
  case USBI_DESC_TYPE_DEVICE:
    return _hid_get_device_descriptor(dev, data, size);
  case USBI_DESC_TYPE_CONFIG:
    if(!index)
      return _hid_get_config_descriptor(dev, data, size);
    return USBI_STATUS_PARAM;
  case USBI_DESC_TYPE_STRING:
    return _hid_get_string_descriptor(dev, index, data, size);
  case USBI_DESC_TYPE_HID:
    if(!index)
      return _hid_get_hid_descriptor(dev, data, size);
    return USBI_STATUS_PARAM;
  case USBI_DESC_TYPE_REPORT:
    if(!index)
      return _hid_get_report_descriptor(dev, data, size);
    return USBI_STATUS_PARAM;
  case USBI_DESC_TYPE_PHYSICAL:
    if(HidD_GetPhysicalDescriptor(dev->wdev, data, size))
      return size;
    return USBI_STATUS_UNKNOWN;
  }
  return USBI_STATUS_PARAM;
}

static int _hid_class_request(hid_device_t dev, int request_type,
                              int request, int value, int index, 
                              void *data, int size)
{
  int report_type = (value >> 8) & 0xFF;
  int report_id = value & 0xFF;

  if(USBI_REQ_RECIPIENT(request_type) != USBI_RECIP_INTERFACE)
    return USBI_STATUS_PARAM;

  if(USBI_REQ_OUT(request_type)
     && request == HID_REQ_SET_REPORT 
     && report_type == HID_REPORT_TYPE_OUTPUT)
    return _hid_set_report(dev, report_id, data, size);

  if(USBI_REQ_IN(request_type)
     && request == HID_REQ_GET_REPORT 
     && report_type == HID_REPORT_TYPE_INPUT)
    return _hid_get_report(dev, report_id, data, size);

  if(USBI_REQ_OUT(request_type)
     && request == HID_REQ_SET_REPORT 
     && report_type == HID_REPORT_TYPE_FEATURE)
    return _hid_set_feature(dev, report_id, data, size);

  if(USBI_REQ_OUT(request_type)
     && request == HID_REQ_SET_REPORT 
     && report_type == HID_REPORT_TYPE_FEATURE)
    return _hid_get_feature(dev, report_id, data, size);

  return USBI_STATUS_PARAM;
}

static int _hid_get_report(hid_device_t dev, int id, void *data, int size)
{
  uint8_t buf[HID_MAX_REPORT_SIZE + 1];

  if(size > HID_MAX_REPORT_SIZE)
    return USBI_STATUS_PARAM;

  buf[0] = (uint8_t)id;

  if(HidD_GetInputReport && HidD_GetInputReport(dev->wdev, buf, size + 1))
    return size + 1;

  return winio_write_sync(dev->wdev, buf, size + 1, USBI_DEFAULT_TIMEOUT);
}

static int _hid_set_report(hid_device_t dev, int id, void *data, int size)
{
  uint8_t buf[HID_MAX_REPORT_SIZE + 1];

  if(size > HID_MAX_REPORT_SIZE)
    return USBI_STATUS_PARAM;

  buf[0] = (uint8_t)id;
  memcpy(buf + 1, data, size);

  if(HidD_SetOutputReport && HidD_SetOutputReport(dev->wdev, buf, size + 1))
    return size + 1;

  return winio_write_sync(dev->wdev, buf, size + 1, USBI_DEFAULT_TIMEOUT);
}

static int _hid_get_feature(hid_device_t dev, int id, void *data, int size)
{
  uint8_t buf[HID_MAX_REPORT_SIZE + 1];

  if(size > HID_MAX_REPORT_SIZE)
    return USBI_STATUS_PARAM;

  buf[0] = (uint8_t)id;

  if(HidD_GetFeature(dev->wdev, buf, size + 1))
    return size + 1;
  return USBI_STATUS_UNKNOWN;
}

static int _hid_set_feature(hid_device_t dev, int id, void *data, int size)
{
  uint8_t buf[HID_MAX_REPORT_SIZE + 1];

  if(size > HID_MAX_REPORT_SIZE)
    return USBI_STATUS_PARAM;

  buf[0] = (uint8_t)id;
  memcpy(buf + 1, data, size);

  if(HidD_SetFeature(dev->wdev, buf, size + 1))
    return size + 1;
  return USBI_STATUS_UNKNOWN;
}

int hid_init(void)
{
  /* load hid.dll */
  DLL_LOAD(hid.dll, HidD_GetAttributes, TRUE);
  DLL_LOAD(hid.dll, HidD_GetHidGuid, TRUE);
  DLL_LOAD(hid.dll, HidD_GetPreparsedData, TRUE);
  DLL_LOAD(hid.dll, HidD_FreePreparsedData, TRUE);
  DLL_LOAD(hid.dll, HidD_GetManufacturerString, TRUE);
  DLL_LOAD(hid.dll, HidD_GetProductString, TRUE);
  DLL_LOAD(hid.dll, HidD_GetSerialNumberString, TRUE);
  DLL_LOAD(hid.dll, HidP_GetCaps, TRUE);
  DLL_LOAD(hid.dll, HidD_SetNumInputBuffers, TRUE);
  DLL_LOAD(hid.dll, HidD_SetFeature, TRUE);
  DLL_LOAD(hid.dll, HidD_GetFeature, TRUE);
  DLL_LOAD(hid.dll, HidD_GetPhysicalDescriptor, TRUE);

  /* XP only, don't return on failure */
  DLL_LOAD(hid.dll, HidD_GetInputReport, FALSE);
  DLL_LOAD(hid.dll, HidD_SetOutputReport, FALSE);

  HidD_GetHidGuid(&_hid_guid);

  return USBI_STATUS_SUCCESS;
}

int hid_deinit(void)
{
  return USBI_STATUS_SUCCESS;
}

int hid_set_debug(usbi_debug_level_t level)
{
  return USBI_STATUS_SUCCESS;
}

int hid_get_name(int index, char *name, int size)
{
  return winio_name_by_guid(&_hid_guid, index, name, size);
}

int hid_open(hid_device_t dev, const char *name)
{
  HIDD_ATTRIBUTES hid_attributes;
	PHIDP_PREPARSED_DATA preparsed_data = NULL;
  HIDP_CAPS capabilities;
  int ret;
  int i;

  ret = winio_open_name(&dev->wdev, name);

  if(ret != USBI_STATUS_SUCCESS)
    return ret;
  
  hid_attributes.Size = sizeof(hid_attributes);
  
  do {
    if(!HidD_GetAttributes(dev->wdev, &hid_attributes))
      break;
    
    dev->vid = hid_attributes.VendorID;
    dev->pid = hid_attributes.ProductID;
    
    /* set the maximum available input buffer size */
    i = 32;
    while(HidD_SetNumInputBuffers(dev->wdev, i))
      i *= 2;

    /* get the maximum input and output report size */
    if(!HidD_GetPreparsedData(dev->wdev, &preparsed_data) || !preparsed_data)
      break;
    if(HidP_GetCaps(preparsed_data, &capabilities) != HIDP_STATUS_SUCCESS)
      break;
    dev->output_report_size = capabilities.OutputReportByteLength;
    dev->input_report_size = capabilities.InputReportByteLength;
    dev->feature_report_size = capabilities.FeatureReportByteLength;

    /* fetch string descriptors */
    HidD_GetManufacturerString(dev->wdev, dev->man_string, 
                               sizeof(dev->man_string));
    HidD_GetProductString(dev->wdev, dev->prod_string, 
                          sizeof(dev->prod_string));
    HidD_GetSerialNumberString(dev->wdev, dev->ser_string, 
                               sizeof(dev->ser_string));
  } while(0);
  
  if(preparsed_data)
    HidD_FreePreparsedData(preparsed_data);

  return ret;
}

int hid_close(hid_device_t dev)
{
  return winio_close(dev->wdev);
}

int hid_reset(hid_device_t dev)
{
  /* fix this, add code to flush IO buffers */
  return USBI_STATUS_SUCCESS;
}

int hid_reset_endpoint(hid_device_t dev, int endpoint)
{
  return USBI_STATUS_NOT_SUPPORTED;
}

int hid_set_configuration(hid_device_t dev, int value)
{
  if(!value || value == 1) {
    dev->config = value;
    return USBI_STATUS_SUCCESS;
  }
  return USBI_STATUS_PARAM;
}

int hid_set_interface(hid_device_t dev, int interface, int altsetting)
{
  if(interface == 0 && altsetting == 0)
    return USBI_STATUS_SUCCESS;
  return USBI_STATUS_PARAM;
}

int hid_claim_interface(hid_device_t dev, int interface)
{
  if(!interface)
    return usbi_claim_interface_simple((usbi_device_t)dev, interface);
  return USBI_STATUS_PARAM;
}

int hid_release_interface(hid_device_t dev, int interface)
{
  if(!interface)
    return usbi_release_interface_simple((usbi_device_t)dev, interface);
  return USBI_STATUS_PARAM;
}

int hid_control_msg(hid_device_t dev, int request_type, int request, 
                    int value, int index, void *data, int size, 
                    hid_io_t io)
{
  int ret = USBI_STATUS_PARAM;

  switch(USBI_REQ_TYPE(request_type))
    {
    case USBI_TYPE_STANDARD:      
      switch(request)
        {
        case USBI_REQ_GET_STATUS: 
          break;
        case USBI_REQ_CLEAR_FEATURE:
          break;
        case USBI_REQ_SET_FEATURE:
          break;
        case USBI_REQ_GET_DESCRIPTOR:
          ret = _hid_get_descriptor(dev, USBI_REQ_RECIPIENT(request_type),
                                    (value >> 8) & 0xFF, value & 0xFF,
                                    data, size);
          break;
        case USBI_REQ_SET_DESCRIPTOR:
          break;
        case USBI_REQ_GET_CONFIGURATION:
          *((uint8_t *)data) = (uint8_t)dev->config;
          ret = 1;
          break;
        case USBI_REQ_SET_CONFIGURATION:
          ret = hid_set_configuration(dev, value);
          break;
        case USBI_REQ_GET_INTERFACE:
          *((uint8_t *)data) = 0;
          ret = 1;
          break;
        case USBI_REQ_SET_INTERFACE:
          ret = hid_set_interface(dev, index, value);
          break;
        }
      break;

    case USBI_TYPE_CLASS:
      ret =_hid_class_request(dev, request_type, request, value, index, 
                              data, size);
      break;
    case USBI_TYPE_VENDOR:  
      break;
    }

  io->ret = ret;
  return ret >= 0 ? USBI_STATUS_SUCCESS : ret;
}

int hid_transfer(hid_device_t dev, int endpoint, usbi_transfer_t type,
                 void *data, int size, int packet_size, hid_io_t io)
{
  if(type != USBI_TRANSFER_INTERRUPT)
    return USBI_STATUS_PARAM;

  if(USBI_ENDPOINT_OUT(endpoint) && endpoint == HID_OUT_EP)
    return winio_write_async(dev->wdev, data, size, &io->wio);
  else if(USBI_ENDPOINT_IN(endpoint) && endpoint == HID_IN_EP)
    return winio_read_async(dev->wdev, data, size, &io->wio);
  return USBI_STATUS_PARAM;
}

int hid_wait(hid_device_t dev, hid_io_t io, int timeout)
{
  if(io->wio) /* asynchronous request? */
    return winio_wait(dev->wdev, io->wio, timeout);
  return io->ret;
}

int hid_poll(hid_device_t dev, hid_io_t io)
{
  if(io->wio) /* asynchronous request? */
    return winio_poll(dev->wdev, io->wio);
  return io->ret;
}

int hid_cancel(hid_device_t dev, hid_io_t io)
{
  if(io->wio) /* asynchronous request? */
    winio_cancel(dev->wdev, io->wio);
  return USBI_STATUS_SUCCESS;
}




