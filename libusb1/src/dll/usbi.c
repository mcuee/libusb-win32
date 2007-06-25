#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "usbi.h"
#include "usbi_backend_libusb0.h"
#include "usbi_backend_winusb.h"
#include "usbi_backend_hid.h"

#define USBI_MAX_DEVICES 255

#define DRIVER_ENTRY(prefix) {         \
  sizeof(struct prefix##_device_t),    \
  sizeof(struct prefix##_io_t),        \
  FALSE,                               \
  (void *)prefix##_init,               \
  (void *)prefix##_deinit,             \
  (void *)prefix##_set_debug,          \
  (void *)prefix##_get_name,           \
  (void *)prefix##_open,               \
  (void *)prefix##_close,              \
  (void *)prefix##_reset,              \
  (void *)prefix##_reset_endpoint,     \
  (void *)prefix##_set_configuration,  \
  (void *)prefix##_set_interface,      \
  (void *)prefix##_claim_interface,    \
  (void *)prefix##_release_interface,  \
  (void *)prefix##_control_msg,        \
  (void *)prefix##_transfer,           \
  (void *)prefix##_wait,               \
  (void *)prefix##_poll,               \
  (void *)prefix##_cancel }

static struct {
  int device_size;
  int io_size;
  int valid;
  int (*init)(void);
  int (*deinit)(void);
  int (*set_debug)(usbi_debug_level_t level);
  int (*get_name)(int index, char *name, int size);
  int (*open)(usbi_device_t dev, const char *name);
  int (*close)(usbi_device_t dev);
  int (*reset)(usbi_device_t dev);
  int (*reset_endpoint)(usbi_device_t dev, int endpoint);
  int (*set_configuration)(usbi_device_t dev, int value);
  int (*set_interface)(usbi_device_t dev, int interface, int altsetting);
  int (*claim_interface)(usbi_device_t dev, int interface);
  int (*release_interface)(usbi_device_t dev, int interface);
  int (*control_msg)(usbi_device_t dev, int request_type, int request, 
                     int value, int index, void *data, int size, usbi_io_t io);
  int (*transfer)(usbi_device_t dev, int endpoint, usbi_transfer_t type,
                  void *data, int size, int packet_size, usbi_io_t io);
  int (*wait)(usbi_device_t dev, usbi_io_t io, int timeout);
  int (*poll)(usbi_device_t dev, usbi_io_t io);
  int (*cancel)(usbi_device_t dev, usbi_io_t io);
} drivers[] = {
  DRIVER_ENTRY(libusb0),
  DRIVER_ENTRY(winusb),
  DRIVER_ENTRY(hid)
};

static struct {
  int driver;
  char *name;
} devices[USBI_MAX_DEVICES];

#define MAX_DRIVER (sizeof(drivers)/sizeof(drivers[0]))

#define USBI_DRIVERS_FOREACH(d) \
  for((d) = 0; (d) < MAX_DRIVER; (d)++)

#define USBI_DRIVERS_FOREACH_VALID(d) \
  USBI_DRIVERS_FOREACH(d) \
  if(drivers[d].valid)

#define USBI_DEVICES_FOREACH(d) \
  for((d) = USBI_FIRST_ID; (d) < USBI_MAX_DEVICES; (d)++)

#define USBI_DEBUG_ASSERT_DEV(dev) do {                                        \
  USBI_DEBUG_ASSERT(dev, "invalid device handle", USBI_STATUS_PARAM);         \
  USBI_DEBUG_ASSERT((dev->driver >= 0), "invalid driver", USBI_STATUS_PARAM); \
  USBI_DEBUG_ASSERT((dev->driver < MAX_DRIVER), "invalid driver",             \
               USBI_STATUS_PARAM);                                       \
 } while(0)

#define USBI_DEBUG_ASSERT_CONFIG(dev)                                  \
  USBI_DEBUG_ASSERT(dev->config.value > 0, "invalid configuration 0", \
               USBI_STATUS_STATE)

#define USBI_DEBUG_ASSERT_INTERFACE(dev)                                \
  USBI_DEBUG_ASSERT(dev->interface.value >= 0, "no interface claimed", \
               USBI_STATUS_STATE)

#define USBI_DEBUG_ASSERT_IO(io) do {                             \
  USBI_DEBUG_ASSERT(io, "invalid IO handle", USBI_STATUS_PARAM); \
  USBI_DEBUG_ASSERT_DEV(io->dev);                                 \
 } while(0)


static usbi_debug_level_t _usbi_debug_level = USBI_DEBUG_LEVEL_NONE;

static int _usbi_update_config_desc_cache(usbi_device_t dev, int config);
static void *_usbi_find_interface_desc(usbi_device_t dev, int interface, 
                                       int alt_setting);
static void *_usbi_find_endpoint_desc(void *interface_desc, int size, 
                                      int endpoint);
static usbi_device_t _usbi_alloc_dev(int id);
static usbi_io_t _usbi_alloc_io(usbi_device_t dev, int endpoint, 
                               usbi_transfer_t type, int direction, int size);

static void _usbi_add_device(const char *name, int driver);

static usbi_device_t _usbi_alloc_dev(int id)
{
  usbi_device_t dev;
  int size;

  size = drivers[devices[id].driver].device_size;
  if(!(dev = malloc(size)))
    return NULL;
  memset(dev, 0, size);
  dev->driver = devices[id].driver;
  dev->id = id;
  strcpy(dev->name, devices[id].name);
  dev->interface.value = -1;
  return dev;
}

static usbi_io_t _usbi_alloc_io(usbi_device_t dev, int endpoint, 
                               usbi_transfer_t type, int direction, int size)
{
  usbi_io_t io;
  int io_size;
  
  io_size = drivers[dev->driver].io_size;
  if(!(io = malloc(io_size)))
    return NULL;
  memset(io, 0, io_size);
  io->dev = dev;
  io->endpoint = endpoint;
  io->type = type;
  io->direction = direction;
  io->size = size;
  return io;
}

static void _usbi_add_device(const char *name, int driver)
{
  int i;

  /* device already present? */
  USBI_DEVICES_FOREACH(i) {
    if(devices[i].name && !strcmp(name, devices[i].name))
      return;
    }

  /* add device */
  USBI_DEVICES_FOREACH(i) {
    if(!devices[i].name) {
      if((devices[i].name = strdup(name))) {
        devices[i].driver = driver;
        return;
      }
    }
  }
}

int usbi_init(void)
{
  int i, ret = USBI_STATUS_UNKNOWN;
  
  /* initialize device structures */
  memset(devices, 0, sizeof(devices));
  USBI_DEVICES_FOREACH(i)
    devices[i].driver = -1;

  /* initialize backend drivers */
  USBI_DRIVERS_FOREACH(i) {
    if(drivers[i].init() == USBI_STATUS_SUCCESS)
      drivers[i].valid = TRUE;
      ret = USBI_STATUS_SUCCESS;
  }
  return ret;
}

int usbi_deinit(void)
{
  int i;

  USBI_DEVICES_FOREACH(i)
    if(devices[i].name)
      free(devices[i].name);

  USBI_DRIVERS_FOREACH_VALID(i)
    drivers[i].deinit();

  return USBI_STATUS_SUCCESS;
}

int usbi_set_debug(usbi_debug_level_t level)
{
  int i;

  _usbi_debug_level = level;

  USBI_DRIVERS_FOREACH_VALID(i)
    drivers[i].set_debug(level);

  return USBI_STATUS_SUCCESS;
}

void _usbi_debug_printf(FILE *stream, usbi_debug_level_t level, 
                        const char *format, ...)
{
  if(level <= _usbi_debug_level) {
    char tmp[512];
    va_list args;
    va_start(args, format);
    vsnprintf(tmp, sizeof(tmp) - 1, format, args);
    va_end(args);
    printf(tmp);
    OutputDebugStringA(tmp);
  }
}

void usbi_refresh_ids(void)
{
  int i, n;
  char name[1024];
  usbi_device_t dev;

  /* remove old devices */
  USBI_DEVICES_FOREACH(i) {
    if(devices[i].name) {
      if(usbi_open(i, &dev) >= 0) {
        /* device is still present */
        usbi_close(dev);
      }
      else {
        /* device has been removed */
        free(devices[i].name);
        devices[i].name = NULL;
      }
    }
  }

  USBI_DRIVERS_FOREACH_VALID(i) {
    n = 0;
    while(drivers[i].get_name(n++, name, sizeof(name)) >= 0)
      _usbi_add_device(name, i);
  }
}

int usbi_get_first_id(void)
{
  int i;

  USBI_DEVICES_FOREACH(i) {
    if(devices[i].name) 
      return i;
  }
  return 0;
}

int usbi_get_next_id(int id)
{
  int i;

  if(id >= USBI_MAX_DEVICES)
    return 0;
  for(i = id + 1; i < USBI_MAX_DEVICES; i++)
    if(devices[i].name)
      return i;

  return 0;
}

int usbi_get_prev_id(int id)
{
  int i;

  if(id <= USBI_FIRST_ID)
    return 0;
  for(i = id - 1; i >= USBI_FIRST_ID; i--)
    if(devices[i].name)
      return i;

  return 0;
}

int usbi_open(int id, usbi_device_t *dev)
{
  int ret;

  *dev = NULL;

  USBI_DEBUG_ASSERT(dev, "invalid parameter 'dev'", USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT((id >= USBI_FIRST_ID), "invalid device ID", USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT((id < USBI_MAX_DEVICES), "invalid device ID",
               USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT(devices[id].name, "invalid device ID", USBI_STATUS_NODEV);

  *dev = _usbi_alloc_dev(id);
  USBI_DEBUG_ASSERT(*dev, "memory allocation failed", USBI_STATUS_NOMEM);

  ret = drivers[devices[id].driver].open(*dev, devices[id].name);

  if(ret < 0) {
    USBI_DEBUG_ERROR("unable to open device %s", devices[id].name);
    free(*dev);
    *dev = NULL;
  }
    
  return ret;
}

int usbi_get_id(usbi_device_t dev, int *id)
{
  USBI_DEBUG_ASSERT_DEV(dev);
  *id = dev->id;
  return USBI_STATUS_SUCCESS;
}

int usbi_close(usbi_device_t dev)
{
  int ret;
  USBI_DEBUG_ASSERT_DEV(dev);
  if(dev->interface.value >= 0)
    usbi_release_interface(dev, dev->interface.value);
  ret = drivers[dev->driver].close(dev);
  free(dev);
  return ret;
}

int usbi_reset(usbi_device_t dev)
{
  USBI_DEBUG_ASSERT_DEV(dev);
  return drivers[dev->driver].reset(dev);
}

int usbi_reset_endpoint(usbi_device_t dev, int endpoint)
{
  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_CONFIG(dev);
  USBI_DEBUG_ASSERT_INTERFACE(dev);
  return drivers[dev->driver].reset_endpoint(dev, endpoint);
}

int usbi_set_configuration(usbi_device_t dev, int config)
{
  int ret;
  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT((dev->interface.value < 0),
                    "an interface is still claimed", USBI_STATUS_BUSY);

  ret = drivers[dev->driver].set_configuration(dev, config);
  if(ret >= 0) {
    dev->config.value = config;
    dev->interface.value = -1;
  }
  return ret;
}

int usbi_set_interface(usbi_device_t dev, int interface, int altsetting)
{
  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_INTERFACE(dev);
  USBI_DEBUG_ASSERT_CONFIG(dev);
  return drivers[dev->driver].set_interface(dev, interface, altsetting);
}

int usbi_claim_interface(usbi_device_t dev, int interface)
{
  int ret;
  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_CONFIG(dev);
  
  if(dev->interface.value == interface)
    return USBI_STATUS_SUCCESS;

  USBI_DEBUG_ASSERT((dev->interface.value < 0),
                    "another interface is still claimed", USBI_STATUS_BUSY);
  ret = drivers[dev->driver].claim_interface(dev, interface);
  if(ret >= 0)
    dev->interface.value = interface;
  return ret;
}

int usbi_release_interface(usbi_device_t dev, int interface)
{
  int ret;
  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_CONFIG(dev);
  
  USBI_DEBUG_ASSERT((dev->interface.value == interface),
                    "invalid parameter", USBI_STATUS_PARAM);
  ret = drivers[dev->driver].release_interface(dev, interface);
  if(ret >= 0)
    dev->interface.value = -1;
  return ret;
}

int usbi_control_msg(usbi_device_t dev, int request_type, int request, 
                     int value, int index, void *data, int size, 
                     usbi_io_t *io)
{
  int ret;

  USBI_DEBUG_ASSERT_DEV(dev);

  *io = _usbi_alloc_io(dev, 0, USBI_TRANSFER_CONTROL, request_type & 0x80,
                      size);
  USBI_DEBUG_ASSERT(*io, "memory allocation failed", USBI_STATUS_NOMEM);

  ret = drivers[dev->driver].control_msg(dev, request_type, request, 
                                         value, index, data, size, *io); 
  if(ret < 0) {
    free(*io);
    *io = NULL;
  }
  return ret;
}

int usbi_transfer(usbi_device_t dev, int endpoint, usbi_transfer_t type,
                  void *data, int size, int packet_size, usbi_io_t *io)
{
  int ret;

  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_CONFIG(dev);
  USBI_DEBUG_ASSERT_INTERFACE(dev);

  *io = _usbi_alloc_io(dev, endpoint, type, endpoint & 0x80, size);
  USBI_DEBUG_ASSERT(*io, "memory allocation failed", USBI_STATUS_NOMEM);

  ret = drivers[dev->driver].transfer(dev, endpoint, type, data, size, 
                                      packet_size, *io);
  if(ret < 0) {
    free(*io);
    *io = NULL;
  }
  return ret;
}

int usbi_wait(usbi_io_t io, int timeout)
{
  int ret;

  USBI_DEBUG_ASSERT_IO(io);

  ret = drivers[io->dev->driver].wait(io->dev, io, timeout);
  free(io);
  return ret;
}

int usbi_poll(usbi_io_t io)
{
  int ret;

  USBI_DEBUG_ASSERT_IO(io);

  if((ret = drivers[io->dev->driver].poll(io->dev, io)) != USBI_STATUS_PENDING)
    free(io);
  return ret;
}

int usbi_cancel(usbi_io_t io)
{
  USBI_DEBUG_ASSERT_IO(io);

  drivers[io->dev->driver].cancel(io->dev, io);
  free(io);
  return USBI_STATUS_SUCCESS;
}

void usbi_unicode_to_ansi(wchar_t *in, char *out, int outlen)
{
  int i = 0;
  
  memset(out, 0, outlen);

  while(in[i] && (i < (outlen - 1))) {
    out[i] = (char)in[i];
    i++;
  }
}

int usbi_control_msg_sync(usbi_device_t dev, int request_type, int request, 
                          int value, int index, void *data, int size,
                          int timeout)
{
  int ret;
  usbi_io_t io;
  ret = usbi_control_msg(dev, request_type, request, 
                         value, index, data, size, &io);
  if(!USBI_SUCCESS(ret))
    return ret;
  return usbi_wait(io, timeout);
}

int usbi_transfer_sync(usbi_device_t dev, int endpoint, usbi_transfer_t type,
                       void *data, int size, int packet_size, int timeout)
{
  int ret;
  usbi_io_t io;
  ret = usbi_transfer(dev, endpoint, type, data, size, packet_size, &io);
  if(!USBI_SUCCESS(ret))
    return ret;
  return usbi_wait(io, timeout);
}

int usbi_get_configuration(usbi_device_t dev, int *config)
{
  int ret;
  char tmp = 0;

  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(config, config, USBI_STATUS_PARAM);

  ret = usbi_control_msg_sync(dev, USBI_DIRECTION_IN | USBI_RECIP_DEVICE
                              | USBI_TYPE_STANDARD,
                              USBI_REQ_GET_CONFIGURATION, 0, 0, &tmp, 1,
                              USBI_DEFAULT_TIMEOUT);
  *config = tmp;
  return ret;
}

int usbi_get_interface(usbi_device_t dev, int interface, int *alt_setting)
{
  int ret;
  char tmp = 0;

  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(alt_setting, alt_setting, USBI_STATUS_PARAM);

  ret = usbi_control_msg_sync(dev, USBI_DIRECTION_IN | USBI_RECIP_INTERFACE
                              | USBI_TYPE_STANDARD,
                              USBI_REQ_GET_INTERFACE, 0, interface, 
                              &tmp, 1,
                              USBI_DEFAULT_TIMEOUT);
  *alt_setting = tmp;
  return ret;
}

int usbi_get_device_descriptor(usbi_device_t dev, void *descriptor, int size)
{
  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(descriptor, descriptor, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(size, size, USBI_STATUS_PARAM);

  memset(descriptor, 0, size);

  return usbi_control_msg_sync(dev, USBI_DIRECTION_IN | USBI_RECIP_DEVICE
                               | USBI_TYPE_STANDARD,
                               USBI_REQ_GET_DESCRIPTOR, 
                               USBI_DESC_TYPE_DEVICE << 8, 0, 
                               descriptor, size,
                               USBI_DEFAULT_TIMEOUT);
}


int usbi_get_config_descriptor(usbi_device_t dev, int config, 
                               void *descriptor, int size)
{
  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(descriptor, descriptor, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(size, size, USBI_STATUS_PARAM);

  memset(descriptor, 0, size);

  return usbi_control_msg_sync(dev, USBI_DIRECTION_IN | USBI_RECIP_DEVICE
                               | USBI_TYPE_STANDARD,
                               USBI_REQ_GET_DESCRIPTOR, 
                               (USBI_DESC_TYPE_CONFIG << 8) | (config & 0xFF), 
                               0, descriptor, size,
                               USBI_DEFAULT_TIMEOUT);
}

int usbi_get_interface_descriptor(usbi_device_t dev, int config, 
                                  int interface, int alt_setting,
                                  void *descriptor, int size)
{
  int ret;
  void *interface_desc = NULL;

  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(descriptor, descriptor, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(size, size, USBI_STATUS_PARAM);

  ret = _usbi_update_config_desc_cache(dev, config);
  if(ret < 0)
    return ret;

  memset(descriptor, 0, size);

  interface_desc = _usbi_find_interface_desc(dev, interface, alt_setting);

  if(!interface_desc)
    return USBI_STATUS_PARAM;

  size = size > USBI_DESC_LEN_INTERFACE ? USBI_DESC_LEN_INTERFACE : size;
  memcpy(descriptor, interface_desc, size);
  return size;
}

int usbi_get_endpoint_descriptor(usbi_device_t dev, int config, 
                                 int interface, int alt_setting,
                                 int endpoint, void *descriptor, int size)
{
  void *interface_desc = NULL;
  void *endpoint_desc = NULL;
  int tmp;

  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(descriptor, descriptor, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(size, size, USBI_STATUS_PARAM);

  tmp = _usbi_update_config_desc_cache(dev, config);
  if(tmp < 0)
    return tmp;

  memset(descriptor, 0, size);

  interface_desc = _usbi_find_interface_desc(dev, interface, alt_setting);

  if(!interface_desc)
    return USBI_STATUS_PARAM;
  
  tmp = dev->config.desc_size 
    - ((int)interface_desc - (int)dev->config.desc);
  
  endpoint_desc = _usbi_find_endpoint_desc(interface_desc, tmp, endpoint);

  if(!endpoint_desc)
    return USBI_STATUS_PARAM;

  size = size > USBI_DESC_LEN_ENDPOINT ? USBI_DESC_LEN_ENDPOINT : size;
  memcpy(descriptor, endpoint_desc, size);
  return size;
}


static int _usbi_update_config_desc_cache(usbi_device_t dev, int config)
{ 
  usbi_config_descriptor_t *d = NULL;
  int ret;
  volatile int size;

  if(dev->config.desc_index == config && dev->config.desc)
    return USBI_STATUS_SUCCESS;

  size = sizeof(usbi_config_descriptor_t);
  if(!(d = malloc(size)))
    return USBI_STATUS_NOMEM;
  ret = usbi_get_config_descriptor(dev, config, d, size);
  if(ret != size) {
    free(d);
    return ret < 0 ? ret : USBI_STATUS_UNKNOWN;
  }

  size = d->wTotalLength;

  if(!(d = realloc(d, size)))
    return USBI_STATUS_NOMEM;

  ret = usbi_get_config_descriptor(dev, config, d, size); 

  if(ret < 0) {
    free(d);
    return ret;
  }
  if(ret < sizeof(usbi_config_descriptor_t)) {
    free(d);
    return USBI_STATUS_UNKNOWN;
  }
  if(dev->config.desc)
    free(dev->config.desc);
  dev->config.desc = d;
  dev->config.desc_index = config;
  dev->config.desc_size = ret;
  return ret;
}

static void *_usbi_find_interface_desc(usbi_device_t dev, int interface, 
                                       int alt_setting)
{
  uint8_t *p = dev->config.desc;
  int size = dev->config.desc_size;

  p += USBI_DESC_LEN_CONFIG;
  size -= USBI_DESC_LEN_CONFIG;
  while(size >= USBI_DESC_LEN_INTERFACE) {
    usbi_interface_descriptor_t *i = (usbi_interface_descriptor_t *)p;
    if(i->bDescriptorType == USBI_DESC_TYPE_INTERFACE) {
      if(!interface && i->bAlternateSetting == alt_setting)
        return p;
      if(i->bAlternateSetting == alt_setting && interface)
        interface--;
    }
    p += i->bLength;
    size -= i->bLength;
  }
  return NULL;
}

static void *_usbi_find_endpoint_desc(void *interface_desc, int size, 
                                      int endpoint)
{
  uint8_t *p = interface_desc;

  p += USBI_DESC_LEN_INTERFACE;
  size = size - USBI_DESC_LEN_INTERFACE;
  while(size >= USBI_DESC_LEN_ENDPOINT) {
    usbi_endpoint_descriptor_t *e = (usbi_endpoint_descriptor_t *)p;
    if(e->bDescriptorType == USBI_DESC_TYPE_ENDPOINT) {
      if(!endpoint)
        return p;
      else
        endpoint--;
    }
    p += e->bLength;
    size -= e->bLength;
  }
  return NULL;
}

int usbi_get_string_descriptor(usbi_device_t dev, int index, int lang_id,
                               void *descriptor, int size)
{
  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(descriptor, descriptor, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(size, size, USBI_STATUS_PARAM);

  memset(descriptor, 0, size);
  return usbi_control_msg_sync(dev, USBI_DIRECTION_IN | USBI_RECIP_DEVICE
                               | USBI_TYPE_STANDARD,
                               USBI_REQ_GET_DESCRIPTOR, 
                               (USBI_DESC_TYPE_STRING << 8) | (index & 0xFF),
                               lang_id, descriptor, size,
                               USBI_DEFAULT_TIMEOUT);
}

int usbi_get_string(usbi_device_t dev, int index, char *string, int size)
{
  wchar_t tmp[128];
  usbi_string_descriptor_t *s = (usbi_string_descriptor_t *)tmp;
  int ret;

  USBI_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(string, string, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(size, size, USBI_STATUS_PARAM);

  if(!size)
    return 0;

  memset(string, 0, size);
  memset(tmp, 0, sizeof(tmp));

  /* get language ID (index 0)*/
  ret = usbi_get_string_descriptor(dev, 0, 0,  s, sizeof(tmp));
  if(ret < 4)
    return ret < 0 ? ret : USBI_STATUS_UNKNOWN;

  /* get descriptor */
  ret = usbi_get_string_descriptor(dev, index, s->wData[0], s, sizeof(tmp));
  if(ret < 4)
    return ret < 0 ? ret : USBI_STATUS_UNKNOWN;

  usbi_unicode_to_ansi(tmp, string, size);
  return strlen(string);
}

int usbi_clear_halt(usbi_device_t dev, int endpoint)
{
  USBI_DEBUG_ASSERT_DEV(dev);
  return usbi_control_msg_sync(dev, USBI_DIRECTION_OUT | USBI_RECIP_ENDPOINT
                               | USBI_TYPE_STANDARD,
                               USBI_REQ_SET_FEATURE, 
                               0, endpoint & 0xFF, NULL, 0,
                               USBI_DEFAULT_TIMEOUT);
}

int usbi_claim_interface_simple(usbi_device_t dev, int interface)
{
  int i;
  char tmp[USBI_DEVICE_NAME_SIZE + 10];

  USBI_DEBUG_ASSERT_DEV(dev);

  if(dev->interface.value == interface)
    return USBI_STATUS_SUCCESS;
  if(dev->interface.value >= 0)
    return USBI_STATUS_BUSY;

  sprintf(tmp, "%s-%d", dev->name, interface);
  for(i = 0; i < strlen(tmp); i++) {
    if(tmp[i] == '\\')
      tmp[i] = '/';
  }

  dev->interface.mutex = CreateMutex(0, TRUE, tmp);
  if(dev->interface.mutex && GetLastError() != ERROR_ALREADY_EXISTS) {
    dev->interface.value = interface;
    return USBI_STATUS_SUCCESS;
  }
  return USBI_STATUS_BUSY;
}

int usbi_release_interface_simple(usbi_device_t dev, int interface)
{
  USBI_DEBUG_ASSERT_DEV(dev);

  if(dev->interface.value < 0)
    return USBI_STATUS_SUCCESS;
  if(dev->interface.value != interface)
    return USBI_STATUS_PARAM;
  if(dev->interface.mutex) {
    ReleaseMutex(dev->interface.mutex);
    CloseHandle(dev->interface.mutex);
    dev->interface.mutex = NULL;
  }

  dev->interface.value = -1;

  return USBI_STATUS_SUCCESS;
}

int usbi_get_claimed_interface(usbi_device_t dev, int *interface)
{
  USBI_DEBUG_ASSERT_DEV(dev);

  if(dev->interface.value < 0)
    return USBI_STATUS_STATE;
  *interface = dev->interface.value;
  return USBI_STATUS_SUCCESS;
}
