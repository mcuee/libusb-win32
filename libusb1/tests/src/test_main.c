#include "test_main.h"
#include "ezusb.h"
#include <windows.h>

/* helper functions */
static struct usb_device *test_find_device(int vid, int pid);
static bool_t test_load_firmware(const char *file);

static struct usb_device *_test_find_device(int vid, int pid)
{
  struct usb_bus *bus;
  struct usb_device *dev;
  
  usb_find_devices();

  for(bus = usb_get_busses(); bus; bus = bus->next) 
    {
      for(dev = bus->devices; dev; dev = dev->next) 
        {
          if(dev->descriptor.idVendor == vid
             && dev->descriptor.idProduct == pid)
            return dev;
        }
    }
  return NULL;
}

usb_dev_handle *test_open(void)
{
  struct usb_device *dev = NULL;
  
  usb_find_devices();

  if(dev = _test_find_device(TEST_VID, TEST_PID))
    return usb_open(dev);
  if(dev = _test_find_device(TEST_HID_VID, TEST_HID_PID))
    return usb_open(dev);
  return NULL;
}

static bool_t test_load_firmware(const char *file)
{
  struct usb_device *dev = NULL;
  usb_dev_handle *hdev;

  usb_find_devices();

  do {
    if(dev = _test_find_device(TEST_FX2_VID, TEST_FX2_PID))
      break;
    if(dev = _test_find_device(TEST_VID, TEST_PID))
      break;
    dev = _test_find_device(TEST_HID_VID, TEST_HID_PID);
  } while(0);

  if(!dev)
    return FALSE;

  if(!(hdev = usb_open(dev)))
    return FALSE;
  
  if(!ezusb_load_file(hdev, file)) {
    usb_close(hdev);
    return FALSE;
  }

  usb_close(hdev);
  Sleep(2000); /* wait for device to reenumerate */
  return TRUE;
}

int test_clear_feature(usb_dev_handle *dev, int recipient, int feature, 
                       int index)
{
  return usb_control_msg(dev, recipient, USB_REQ_CLEAR_FEATURE,
                         feature, index, NULL, 0, TEST_DEFAULT_TIMEOUT);
}
  
int test_get_altinterface(usb_dev_handle *dev, int interface, 
                          char *alt_setting)
{
  return usb_control_msg(dev, 0x80 | USB_RECIP_INTERFACE, 
                         USB_REQ_GET_INTERFACE, 0, interface, alt_setting, 1,
                         TEST_DEFAULT_TIMEOUT);
}

int test_get_configuration(usb_dev_handle *dev, unsigned char *config)
{
  return usb_control_msg(dev, 0x80 | USB_RECIP_DEVICE,  
                         USB_REQ_GET_CONFIGURATION,  
                         0, 0, config, 1, TEST_DEFAULT_TIMEOUT); 
}

int test_get_status(usb_dev_handle *dev, unsigned short *status, int recipient,
                   int index)
{
  return usb_control_msg(dev, 0x80 | recipient, USB_REQ_GET_STATUS,
                         0, index, (char *)status, 2, TEST_DEFAULT_TIMEOUT);
}

int test_set_feature(usb_dev_handle *dev, int recipient, int feature, 
                    int index)
{
  return usb_control_msg(dev, recipient, USB_REQ_SET_FEATURE, feature, 
                         index, NULL, 0, TEST_DEFAULT_TIMEOUT);
}

bool_t test_is_hid(usb_dev_handle *dev) {
  struct usb_device *d;

  d = usb_device(dev);

  if(d->descriptor.idVendor == TEST_HID_VID 
     && d->descriptor.idProduct == TEST_HID_PID)
    return TRUE;
  return FALSE;
}

/* test suites */
TEST_SUITE_DEFINE(open_close);
TEST_SUITE_DEFINE(configuration);
TEST_SUITE_DEFINE(interface);
TEST_SUITE_DEFINE(interrupt_hid);
//TEST_SUITE_DEFINE(descriptors);

/* main unit tests */
TEST_MAIN_BEGIN();
usb_dev_handle *dev;

/* initialize library and load test firmware */
TEST_PRINT("initializing library... ");
usb_init();
usb_set_debug(0);
TEST_PRINT("done\n");

TEST_PRINT("initializing busses... ");
if(usb_find_busses() < 0) {
  TEST_PRINT("failed\n");
  return 0;
}
TEST_PRINT("done\n");

TEST_PRINT("loading standard firmware... ");
if(!test_load_firmware(TEST_FIRMWARE)) {
  TEST_PRINT("failed\n");
  usb_close(dev);
  return 0;
}
TEST_PRINT("done\n");

TEST_PRINT("running test suites\n");

/* run test suites */
TEST_SUITE_RUN(open_close);
TEST_SUITE_RUN(configuration);
TEST_SUITE_RUN(interface);
/* TEST_SUITE_RUN(descriptors); */

#if 0
/* HID */
TEST_PRINT("loading HID firmware... ");
if(!test_load_firmware(TEST_HID_FIRMWARE)) {
  TEST_PRINT("failed\n");
  usb_close(dev);
  return 0;
}
TEST_PRINT("done\n");

TEST_PRINT("running test suites\n");

/* run test suites */
TEST_SUITE_RUN(open_close);
TEST_SUITE_RUN(configuration);
TEST_SUITE_RUN(interface);
TEST_SUITE_RUN(interrupt_hid);
/* TEST_SUITE_RUN(descriptors); */
#endif

TEST_MAIN_END();
