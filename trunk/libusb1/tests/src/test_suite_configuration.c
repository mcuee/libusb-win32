#include "test_main.h"

TEST_SUITE_BEGIN(configuration);
usb_dev_handle *dev;
unsigned char config;
int c;

TEST_ASSERT(dev = test_open());

/* invalid parameter */
TEST_BEGIN(test0);
TEST_ASSERT(usb_set_configuration(NULL, 0) < 0);
TEST_END();

/* configuration 0 */
TEST_BEGIN(test1);
TEST_ASSERT(usb_set_configuration(dev, 0) >= 0);
TEST_ASSERT(test_get_configuration(dev, &config) == 1);
TEST_ASSERT(config == 0);
TEST_END();

/* valid configurations */
TEST_BEGIN(test2);
usb_set_debug(255);
TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
TEST_ASSERT(test_get_configuration(dev, &config) == 1);
TEST_ASSERT(config == 1);
if(!test_is_hid(dev)) {
  TEST_ASSERT(usb_set_configuration(dev, 2) >= 0);
  TEST_ASSERT(test_get_configuration(dev, &config) == 1);
  TEST_ASSERT(config == 2);
}
usb_set_debug(0);
TEST_END();

/* invalid configurations */
TEST_BEGIN(test3);
TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
TEST_ASSERT(usb_set_configuration(dev, 10) < 0);
TEST_ASSERT(test_get_configuration(dev, &config) == 1);
TEST_ASSERT(config == 1);
if(!test_is_hid(dev)) {
TEST_ASSERT(usb_set_configuration(dev, 2) >= 0);
TEST_ASSERT(usb_set_configuration(dev, 10) < 0);
TEST_ASSERT(test_get_configuration(dev, &config) == 1);
TEST_ASSERT(config == 2);
}
TEST_END();

TEST_ASSERT(usb_close(dev) >= 0);

TEST_SUITE_END();
