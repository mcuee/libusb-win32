/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
 * Copyright (c) 2000-2005 Johannes Erdfelt <johannes@erdfelt.com>
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

#include "usb.h"
#include "usbi.h"
#include "errno.h"
#include <windows.h>


#define LIBUSB_MAX_DEVICES 256
#define LIBUSB_DEFAULT_TIMEOUT 1000

#ifndef ETIMEDOUT
#define ETIMEDOUT 116	
#endif

#define LIST_FOREACH(list, node) \
  for(node = list; node; node = node->next)

#define LIST_ADD(list, node) \
	do { \
	  if(list) { \
	    node->next = list; \
	    node->next->prev = node; \
	  } else \
	    node->next = NULL; \
	  node->prev = NULL; \
	  list = node; \
	} while(0)


typedef struct {
  usb_dev_handle *dev;
  usbi_transfer_t type;
  int endpoint;
  void *data;
  int size;
  int packet_size;
  usbi_io_t io;
} usb_context_t;


static struct usb_version _usb_version = {
  { VERSION_MAJOR,
    VERSION_MINOR,
    VERSION_MICRO,
    VERSION_NANO },
  { -1, -1, -1, -1 }
};

static struct usb_bus _virtual_bus;
static struct usb_device _virtual_hub;

static int _usbi_to_errno(int error);
static void *_usb_malloc(int size);
static void _usb_deinit(void);

static void _usb_free_device(struct usb_device *dev);
static void _usb_free_device_list(struct usb_device *list);
static void _usb_fetch_descriptors(struct usb_device *dev);
static void *_usb_fetch_configs(usbi_device_t usbi_dev, int num_config);
static struct usb_interface *
_usb_fetch_interfaces(usbi_device_t usbi_dev, int config, int num_interfaces);
static struct usb_interface_descriptor *
_usb_fetch_altsettings(usbi_device_t usbi_dev, int config, 
                       int interface, int num_alt);
static void *_usb_fetch_endpoints(usbi_device_t usbi_dev, int config, 
                                  int interface, int alt_setting, 
                                  int num_endpoints);

static int _usb_setup_async(usb_dev_handle *dev, void **context,
                            usbi_transfer_t type, int endpoint,
                            int packet_size);


static int _usbi_to_errno(int error)
{
  if(error >= 0)
    return error;
  switch(error) {
  case USBI_STATUS_SUCCESS: return 0;
  case USBI_STATUS_PENDING: return -EBUSY;
  case USBI_STATUS_PARAM: return -EINVAL;
  case USBI_STATUS_BUSY: return -EBUSY;
  case USBI_STATUS_NOMEM: return -ENOMEM;
  case USBI_STATUS_TIMEOUT: return -ETIMEDOUT;
  case USBI_STATUS_NODEV: return -ENODEV;
  default: return -EIO;
  }
}

static void *_usb_malloc(int size)
{
  void *ret = NULL;
  ret = malloc(size);
  if(ret)
    memset(ret, 0, size);
  return ret;
}

static void _usb_deinit(void)
{
  _usb_free_device_list(_virtual_bus.devices);
  if(_virtual_hub.children)
    free(_virtual_hub.children);
  usbi_deinit();
}

static void _usb_free_device(struct usb_device *dev)
{
  int c, i, a;
  
  if(dev) {
    if(dev->config) {
      for(c = 0; c < dev->descriptor.bNumConfigurations; c++) {
        if(dev->config[c].interface) {
          for(i = 0; i < dev->config[c].bNumInterfaces; i++) {
            if(dev->config[c].interface[i].altsetting) {
              for(a = 0; a < dev->config[c].interface[i].num_altsetting; a++) {
                if(dev->config[c].interface[i].altsetting[a].endpoint)
                  free(dev->config[c].interface[i].altsetting[a].endpoint);
              }
              free(dev->config[c].interface[i].altsetting);
            }
          }
          free(dev->config[c].interface);
        }
      }
      free(dev->config);
    }
    free(dev);
  }
}

static void _usb_free_device_list(struct usb_device *list)
{
  if(list) {
    if(list->next)
      _usb_free_device_list(list->next);
    _usb_free_device(list);
  }
}

static void _usb_fetch_descriptors(struct usb_device *dev)
{
  usbi_device_t usbi_dev;

  dev->config = NULL;

  if(usbi_open(dev->devnum, &usbi_dev) < 0)
    return;
  if(usbi_get_device_descriptor(usbi_dev, &dev->descriptor, USB_DT_DEVICE_SIZE)
     != USB_DT_DEVICE_SIZE)
    return;

  dev->config = _usb_fetch_configs(usbi_dev, 
                                   dev->descriptor.bNumConfigurations);

  usbi_close(usbi_dev);
}

static void *_usb_fetch_configs(usbi_device_t usbi_dev, int num_config)
{
  int size, c;
  struct usb_config_descriptor *d;

  if(!num_config)
    return NULL;
  
  size = num_config * sizeof(struct usb_config_descriptor);
  d = _usb_malloc(size);
  USBI_DEBUG_ASSERT(d, "memory allocation failed", NULL);
  
  for(c = 0; c < num_config; c++) {
    if(usbi_get_config_descriptor(usbi_dev, c, d + c,
                                  USB_DT_CONFIG_SIZE) != USB_DT_CONFIG_SIZE)
      continue;
    d[c].interface = _usb_fetch_interfaces(usbi_dev, c, d->bNumInterfaces);
  }
  return d;
}

static struct usb_interface *
_usb_fetch_interfaces(usbi_device_t usbi_dev, int config, int num_interfaces)
{
  struct usb_interface *interface;
  struct usb_interface_descriptor idesc;
  int i, size;

  if(!num_interfaces)
    return NULL;

  size = num_interfaces * sizeof(struct usb_interface);
  
  interface = _usb_malloc(size);
  USBI_DEBUG_ASSERT(interface, "memory allocation failed", NULL);
  
  for(i = 0; i < num_interfaces; i++) {
    int num_alt = 0;
    while(1) {
      if(usbi_get_interface_descriptor(usbi_dev, config, i, num_alt, &idesc,
                                       USB_DT_INTERFACE_SIZE)
         != USB_DT_INTERFACE_SIZE)
        break;
      num_alt++;
    }
    if(num_alt) {
      interface[i].altsetting = _usb_fetch_altsettings(usbi_dev, config, i,
                                                       num_alt);
      if(interface[i].altsetting)
        interface[i].num_altsetting = num_alt;
    }
  }
  return interface;
}

static struct usb_interface_descriptor *
_usb_fetch_altsettings(usbi_device_t usbi_dev, int config, 
                       int interface, int num_alt)
{
  struct usb_interface_descriptor *idesc;
  int size, a;

  if(!num_alt)
    return NULL;
  
  size = num_alt * sizeof(struct usb_interface_descriptor);
  
  idesc = _usb_malloc(size);
  USBI_DEBUG_ASSERT(idesc, "memory allocation failed", NULL);
  
  for(a = 0; a < num_alt; a++) {
    if(usbi_get_interface_descriptor(usbi_dev, config, interface, a, 
                                     idesc + a, USB_DT_INTERFACE_SIZE)
       == USB_DT_INTERFACE_SIZE) {
      idesc[a].endpoint = _usb_fetch_endpoints(usbi_dev, config,
                                               interface, a,
                                               idesc[a].bNumEndpoints);
    }
  }
  return idesc;
}

static void *_usb_fetch_endpoints(usbi_device_t usbi_dev, int config, 
                                  int interface, int alt_setting, 
                                  int num_endpoints)
{
  struct usb_endpoint_descriptor *edesc;
  int size, e;

  if(!num_endpoints)
    return NULL;
  
  size = num_endpoints * sizeof(struct usb_endpoint_descriptor);
  
  edesc = _usb_malloc(size);
  USBI_DEBUG_ASSERT(edesc, "memory allocation failed", NULL);
  
  for(e = 0; e < num_endpoints; e++)
    usbi_get_endpoint_descriptor(usbi_dev, config, interface, alt_setting,
                                 e, edesc + e, USB_DT_ENDPOINT_SIZE);
  return edesc;
}

/* DLL main entry point */
BOOL WINAPI DllMain(HANDLE module, DWORD reason, LPVOID reserved)
{
  switch(reason)
    {
    case DLL_PROCESS_ATTACH:
      break;
    case DLL_PROCESS_DETACH:
      _usb_deinit();
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    default:
      break;
    }
  return TRUE;
}


void usb_init(void)
{
  USBI_DEBUG_TRACE("dll version: %d.%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, 
              VERSION_MICRO, VERSION_NANO);

  /* TODO: get driver version */

  /* initialize backend */
  usbi_init();
  usbi_set_debug(USBI_DEBUG_LEVEL_ALL);
  usbi_refresh_ids();

  /* initialize the virtual bus */
  memset(&_virtual_bus, 0, sizeof(_virtual_bus));
  snprintf(_virtual_bus.dirname, sizeof(_virtual_bus.dirname) - 1, "/bus0");
  _virtual_bus.root_dev = &_virtual_hub;

  /* initialize the virtual root-hub */
  memset(&_virtual_hub, 0, sizeof(_virtual_hub));
  snprintf(_virtual_hub.filename, sizeof(_virtual_hub.filename) -1,
           "virtual_root_hub");
  _virtual_hub.bus = &_virtual_bus;    
  _virtual_hub.descriptor.bLength = USB_DT_DEVICE_SIZE;
  _virtual_hub.descriptor.bDescriptorType = USB_DT_DEVICE;
  _virtual_hub.descriptor.bcdUSB = 0x0200;
  _virtual_hub.descriptor.bDeviceClass = USB_CLASS_HUB;
  _virtual_hub.descriptor.bDeviceSubClass = 0;
  _virtual_hub.descriptor.bDeviceProtocol = 0;
  _virtual_hub.descriptor.bMaxPacketSize0 = 64;
  _virtual_hub.descriptor.idVendor = 0;
  _virtual_hub.descriptor.idProduct = 0;
  _virtual_hub.descriptor.bcdDevice = 0x100;
  _virtual_hub.descriptor.iManufacturer = 0;
  _virtual_hub.descriptor.iProduct = 0;
  _virtual_hub.descriptor.iSerialNumber = 0;
  _virtual_hub.descriptor.bNumConfigurations = 0;
  _virtual_hub.devnum = 1;

  return;
}

void usb_set_debug(int level)
{
  usbi_set_debug(level);
}

int usb_find_busses(void)
{
  /* nothing to do here, we only have one static bus */
  return 0;
}

int usb_find_devices(void)
{
  struct usb_device *dev;
  int id, i;

  /* delete old list */
  _usb_free_device_list(_virtual_bus.devices);
  _virtual_bus.devices = NULL;

  /* build new device list */
  usbi_refresh_ids();
  id = usbi_get_first_id();

  while(id >= USBI_FIRST_ID) {
    dev = _usb_malloc(sizeof(struct usb_device));
    USBI_DEBUG_ASSERT(dev, "memory allocation failed", -ENOMEM);
    dev->bus = &_virtual_bus;
    dev->devnum = id;
    snprintf(dev->filename, sizeof(dev->filename) - 1, "dev%04d", id);
    _usb_fetch_descriptors(dev);
    LIST_ADD(_virtual_bus.devices, dev);
    id = usbi_get_next_id(id);
  }
  
  /* build children list */
  if(_virtual_hub.children) {
    free(_virtual_hub.children);
    _virtual_hub.children = NULL;
  }
  _virtual_hub.num_children = 0;

  if(!_virtual_bus.devices) 
    return 0;

  /* count devices */
  i = 0;
  LIST_FOREACH(_virtual_bus.devices, dev)
    i++;

  _virtual_hub.children = _usb_malloc(sizeof(struct usb_device *) * i);
  USBI_DEBUG_ASSERT(_virtual_hub.children, "memory allocation failed", 
                    -ENOMEM);
  _virtual_hub.num_children = i;
  i = 0;
  LIST_FOREACH(_virtual_bus.devices, dev)
    _virtual_hub.children[i++] = dev;

  return 0;
}

struct usb_device *usb_device(usb_dev_handle *dev)
{
  struct usb_device_descriptor d;
  struct usb_device *node;

  USBI_DEBUG_ASSERT_PARAM(dev, dev, NULL);

  if(usbi_get_device_descriptor(dev, &d, USB_DT_DEVICE_SIZE) 
     != USB_DT_DEVICE_SIZE)
    return NULL; /* device node is gone */

  /* find device node */
  LIST_FOREACH(_virtual_bus.devices, node) {
    if(node->descriptor.idVendor == d.idVendor
       && node->descriptor.idProduct == d.idProduct)
      return node;
  }
  /* device node is gone */
  return NULL;
}

struct usb_bus *usb_get_busses(void)
{
  return &_virtual_bus;
}

const struct usb_version *usb_get_version(void)
{
  return &_usb_version;
}

usb_dev_handle *usb_open(struct usb_device *dev)
{
  int ret;
  usbi_device_t h;

  USBI_DEBUG_ASSERT_PARAM(dev, dev, NULL);
  USBI_DEBUG_ASSERT((dev->devnum >= USBI_FIRST_ID), "invalid device ID", NULL);

  ret = usbi_open(dev->devnum, &h);
  if(ret < 0) {
    USBI_DEBUG_ERROR("unable to open device %s", dev->filename);
    return NULL;
  }
  return h;
}

int usb_close(usb_dev_handle *dev)
{
  return _usbi_to_errno(usbi_close(dev));
}

int usb_get_string(usb_dev_handle *dev, int index, int langid, char *buf,
                   size_t buflen)
{
  return _usbi_to_errno(usbi_get_string_descriptor(dev, index, 
                                                   langid, buf, buflen));
}

int usb_get_string_simple(usb_dev_handle *dev, int index, char *buf,
                          size_t buflen)
{
  return _usbi_to_errno(usbi_get_string(dev, index, buf, buflen));
}

int usb_get_descriptor_by_endpoint(usb_dev_handle *dev, int ep,
                                   unsigned char type, unsigned char index,
                                   void *buf, int size)
{
  USBI_DEBUG_ASSERT_PARAM(!ep, ep, -EINVAL);

  return usb_get_descriptor(dev, type, index, buf, size);
}

int usb_get_descriptor(usb_dev_handle *dev, unsigned char type,
                       unsigned char index, void *buf, int size)
{
  int ret;
  int request_type;

  USBI_DEBUG_ASSERT_PARAM(buf, buf, -EINVAL);
  USBI_DEBUG_ASSERT_PARAM(size, size, -EINVAL);
  
  switch(type) {
  case USB_DT_REPORT:
  case USB_DT_PHYSICAL:
    request_type = USB_ENDPOINT_IN | USB_TYPE_STANDARD | USBI_RECIP_INTERFACE;
  break;    
  case USB_DT_DEVICE:
  case USB_DT_CONFIG:
  case USB_DT_STRING:
  case USB_DT_HUB:
  default:
    request_type = USB_ENDPOINT_IN | USB_TYPE_STANDARD | USBI_RECIP_DEVICE;
  }

  ret = usbi_control_msg_sync(dev, request_type, 
                              USBI_REQ_GET_DESCRIPTOR, 
                              (type << 8) | (index & 0xFF), 0, 
                              buf, size, LIBUSB_DEFAULT_TIMEOUT);

  return _usbi_to_errno(ret);
}

int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
                   int timeout)
{
  int ret;

  USBI_DEBUG_ASSERT_PARAM(USBI_ENDPOINT_OUT(ep), ep, -EINVAL);

  ret = usbi_transfer_sync(dev, ep, USBI_TRANSFER_BULK,
                           bytes, size, 0, timeout);
  return _usbi_to_errno(ret);
}

int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size,
                  int timeout)
{
  int ret;

  USBI_DEBUG_ASSERT_PARAM(USBI_ENDPOINT_IN(ep), ep, -EINVAL);

  ret = usbi_transfer_sync(dev, ep, USBI_TRANSFER_BULK,
                           bytes, size, 0, timeout);
  return _usbi_to_errno(ret);
}

int usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, int size,
                        int timeout)
{
  int ret;

  USBI_DEBUG_ASSERT_PARAM(USBI_ENDPOINT_OUT(ep), ep, -EINVAL);

  ret = usbi_transfer_sync(dev, ep, USBI_TRANSFER_INTERRUPT,
                           bytes, size, 0, timeout);
  return _usbi_to_errno(ret);
}


int usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, int size,
                       int timeout)
{
  int ret;

  USBI_DEBUG_ASSERT_PARAM(USBI_ENDPOINT_IN(ep), ep, -EINVAL);

  ret = usbi_transfer_sync(dev, ep, USBI_TRANSFER_INTERRUPT,
                           bytes, size, 0, timeout);
  return _usbi_to_errno(ret);
}


int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
                    int value, int index, char *bytes, int size, 
                    int timeout)
{
  int ret;
 
  USBI_DEBUG_ASSERT_PARAM(dev, dev, -EINVAL);

  ret = usbi_control_msg_sync(dev, requesttype, request, 
                              value, index, bytes, size, timeout);
  return _usbi_to_errno(ret);
}

int usb_set_configuration(usb_dev_handle *dev, int configuration)
{
  return _usbi_to_errno(usbi_set_configuration(dev, configuration));
}

int usb_claim_interface(usb_dev_handle *dev, int interface)
{
  return _usbi_to_errno(usbi_claim_interface(dev, interface));
}

int usb_release_interface(usb_dev_handle *dev, int interface)
{
  return _usbi_to_errno(usbi_release_interface(dev, interface));
}

int usb_set_altinterface(usb_dev_handle *dev, int alternate)
{
  int interface, ret;

  ret = usbi_get_claimed_interface(dev, &interface);
  if(ret < 0)
    return _usbi_to_errno(ret);
  return _usbi_to_errno(usbi_set_interface(dev, interface, alternate));
}

int usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
  return usb_clear_halt(dev, ep);
}

int usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
  return _usbi_to_errno(usbi_clear_halt(dev, ep));
}

int usb_reset(usb_dev_handle *dev)
{
  return _usbi_to_errno(usbi_reset(dev));
}

char *usb_strerror(void)
{
  return "usb_strerror() is not supported";
}

static int _usb_setup_async(usb_dev_handle *dev, void **context,
                            usbi_transfer_t type, int endpoint,
                            int packet_size)
{
  usb_context_t *c;

  USBI_DEBUG_ASSERT_PARAM(context, context, -EINVAL);

  *context = NULL;
  c = _usb_malloc(sizeof(usb_context_t));
  USBI_DEBUG_ASSERT(c, "memory allocation failed", -ENOMEM);

  c->dev = dev;
  c->type = type;
  c->endpoint = endpoint;
  c->packet_size = packet_size;
  *context = c;
  return 0;
}

int usb_isochronous_setup_async(usb_dev_handle *dev, void **context,
                                 unsigned char ep, int pktsize)
{
  return _usb_setup_async(dev, context, USBI_TRANSFER_ISOCHRONOUS, 
                          ep, pktsize);
}

int usb_bulk_setup_async(usb_dev_handle *dev, void **context,
                         unsigned char ep)
{
  return _usb_setup_async(dev, context, USBI_TRANSFER_BULK, ep, 0);
}

int usb_interrupt_setup_async(usb_dev_handle *dev, void **context,
                              unsigned char ep)
{
  return _usb_setup_async(dev, context, USBI_TRANSFER_INTERRUPT, ep, 0);
}

int usb_submit_async(void *context, char *bytes, int size)
{
  int ret;
  usb_context_t *c = (usb_context_t *)context;

  USBI_DEBUG_ASSERT_PARAM(context, context, -EINVAL);

  c->data = bytes;
  c->size = size;
  ret = usbi_transfer(c->dev, c->endpoint, c->type,
                      c->data, c->size, c->packet_size, &c->io);
  return _usbi_to_errno(ret);
}

int usb_reap_async(void *context, int timeout)
{
  usb_context_t *c = (usb_context_t *)context;
  USBI_DEBUG_ASSERT_PARAM(context, context, -EINVAL);
  return _usbi_to_errno(usbi_wait(c->io, timeout));
}

int usb_reap_async_nocancel(void *context, int timeout)
{
  usb_context_t *c = (usb_context_t *)context;
  USBI_DEBUG_ASSERT_PARAM(context, context, -EINVAL);
  return _usbi_to_errno(usbi_poll(c->io));
}


int usb_cancel_async(void *context)
{
  usb_context_t *c = (usb_context_t *)context;
  USBI_DEBUG_ASSERT_PARAM(context, context, -EINVAL);
  return _usbi_to_errno(usbi_cancel(c->io));
}

int usb_free_async(void **context)
{
  USBI_DEBUG_ASSERT_PARAM(context, context, -EINVAL);
  USBI_DEBUG_ASSERT_PARAM(*context, context, -EINVAL);
  free(*context);
  *context = NULL;
  return 0;
}


int usb_install_driver_np(const char *inf_file)
{
  return _usbi_to_errno(usbi_install_inf_file(inf_file));
}

int usb_touch_inf_file_np(const char *inf_file)
{
  return _usbi_to_errno(usbi_install_touch_inf_file(inf_file));
}

void CALLBACK usb_touch_inf_file_np_rundll(HWND wnd, HINSTANCE instance,
                                           LPSTR cmd_line, int cmd_show)
{
  usbi_install_touch_inf_file(cmd_line);
}

void CALLBACK usb_install_driver_np_rundll(HWND wnd, HINSTANCE instance,
                                           LPSTR cmd_line, int cmd_show)
{
  usbi_install_inf_file(cmd_line);
}
