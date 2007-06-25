#include "test_main.h"

#define TEST_GET_DEVICE_DESC(d, i) \
  usb_get_descriptor(d, USB_DT_DEVICE, i, buf, USB_DT_DEVICE_SIZE)

#define TEST_GET_CONFIG_DESC(d, i, b, s) \
  usb_get_descriptor(d, USB_DT_CONFIG, i, b, s)

#define TEST_GET_STRING_DESC_A(d, i, b, s) \
  usb_get_descriptor(d, USB_DT_STRING, i, b, s)

#define TEST_GET_STRING_DESC_B(d, i, b, s) \
  usb_get_string(d, i, 0x0409, b, s)

#define TEST_GET_STRING_DESC_C(d, i, b, s) \
  usb_get_string_simple(d, i, b, s)

TEST_SUITE_BEGIN(descriptors);
usb_dev_handle *dev;
struct usb_config_descriptor cdesc;
char buf[1024];
int i,c;

#if 0

TEST_ASSERT(dev = test_open_device());

TEST_BEGIN(test0);
TEST_ASSERT(TEST_GET_DEVICE_DESC(NULL, 0) < 0);
TEST_END();

TEST_BEGIN(test1);
TEST_ASSERT(TEST_GET_DEVICE_DESC(dev, 0) == USB_DT_DEVICE_SIZE);
TEST_ASSERT(!memcmp(TEST_DEVICE_DESCRIPTOR, buf, USB_DT_DEVICE_SIZE));         
TEST_END();

TEST_BEGIN(test2);
TEST_ASSERT(TEST_GET_DEVICE_DESC(dev, 1) < 0);
TEST_END();

TEST_BEGIN(test3);
TEST_ASSERT(TEST_GET_CONFIG_DESC(NULL, 0, &cdesc, 0) < 0);
TEST_END();

TEST_BEGIN(test4);
TEST_ASSERT(TEST_GET_CONFIG_DESC(dev, 0, NULL, 0) < 0);
TEST_END();

TEST_BEGIN(test5);
FOREACH_VALID_CONFIG_INDEX(c) {
  TEST_ASSERT(TEST_GET_CONFIG_DESC(dev, 0, &cdesc, 4) == 4);
}
TEST_END();

TEST_BEGIN(test6);
FOREACH_VALID_CONFIG_INDEX(c) {
  memset(&cdesc, 0, sizeof(cdesc));
  TEST_ASSERT(TEST_GET_CONFIG_DESC(dev, c, &cdesc, USB_DT_CONFIG_SIZE) 
              == USB_DT_CONFIG_SIZE);
  TEST_ASSERT(!memcmp(TEST_CONFIG_DESCRIPTORS[c], &cdesc, USB_DT_CONFIG_SIZE));
}
TEST_END();

TEST_BEGIN(test7);
FOREACH_VALID_CONFIG_INDEX(c) {
  memset(&cdesc, 0, sizeof(cdesc));
  memset(buf, 0, sizeof(buf));
  TEST_ASSERT(TEST_GET_CONFIG_DESC(dev, c, &cdesc, USB_DT_CONFIG_SIZE) 
              == USB_DT_CONFIG_SIZE);
  TEST_ASSERT(TEST_GET_CONFIG_DESC(dev, c, buf, cdesc.wTotalLength) 
              == cdesc.wTotalLength);

  TEST_ASSERT(!memcmp(TEST_CONFIG_DESCRIPTORS[c], buf, cdesc.wTotalLength));
}
TEST_END();

TEST_BEGIN(test8);
FOREACH_INVALID_CONFIG_INDEX(c) {
  TEST_ASSERT(TEST_GET_CONFIG_DESC(dev, c, &cdesc, USB_DT_CONFIG_SIZE) < 0);
}
TEST_END();

TEST_BEGIN(test9);
FOREACH_VALID_STRING(i) {
  int ret;
  memset(buf, 0, sizeof(buf));
  ret = TEST_GET_STRING_DESC_A(dev, i, buf, sizeof(buf));
  TEST_ASSERT(ret > 0);
  TEST_ASSERT(!memcmp(TEST_STRING_DESCRIPTORS[i], buf, ret));
}
TEST_END();

TEST_BEGIN(test10);
  TEST_ASSERT(TEST_GET_STRING_DESC_A(NULL, i, buf, sizeof(buf)) < 0);
  TEST_ASSERT(TEST_GET_STRING_DESC_B(NULL, i, buf, sizeof(buf)) < 0);
  TEST_ASSERT(TEST_GET_STRING_DESC_C(NULL, i, buf, sizeof(buf)) < 0);
TEST_END();

TEST_BEGIN(test11);
FOREACH_VALID_STRING(i) {
  int ret;
  memset(buf, 0, sizeof(buf));
  ret = TEST_GET_STRING_DESC_B(dev, i, buf, sizeof(buf));
  TEST_ASSERT(ret > 0);
  TEST_ASSERT(!memcmp(TEST_STRING_DESCRIPTORS[i], buf, ret));
}
TEST_END();

TEST_BEGIN(test12);
FOREACH_VALID_STRING(i) {
  i++; /* skip string index 0 */
  int ret;
  memset(buf, 0, sizeof(buf));
  ret = TEST_GET_STRING_DESC_C(dev, i, buf, sizeof(buf));
  TEST_ASSERT(ret > 0);
  TEST_ASSERT(!strcmp(TEST_STRING_DESCRIPTORS_SIMPLE[i], buf));
}
TEST_END();

TEST_BEGIN(test13);
FOREACH_VALID_STRING(i) {
  TEST_ASSERT(TEST_GET_STRING_DESC_A(dev, i, buf, 0) < 0);
  TEST_ASSERT(TEST_GET_STRING_DESC_A(dev, i, buf, 3) == 3);
  TEST_ASSERT(TEST_GET_STRING_DESC_B(dev, i, buf, 0) < 0);
  TEST_ASSERT(TEST_GET_STRING_DESC_B(dev, i, buf, 3) == 3);
}
TEST_END();

TEST_BEGIN(test14);
FOREACH_VALID_STRING(i) {
  i++; /* skip string index 0 */
  TEST_ASSERT(TEST_GET_STRING_DESC_C(dev, i, buf, 0) < 0);
  TEST_ASSERT(TEST_GET_STRING_DESC_C(dev, i, buf, 3) == 3);
}
TEST_END();

TEST_BEGIN(test15);
FOREACH_INVALID_STRING(i) {
  TEST_ASSERT(TEST_GET_STRING_DESC_A(dev, i, buf, sizeof(buf)) < 0);
  TEST_ASSERT(TEST_GET_STRING_DESC_B(dev, i, buf, sizeof(buf)) < 0);
}
TEST_END();

TEST_BEGIN(test16);
FOREACH_INVALID_STRING(i) {
  i++; /* skip string index 0 (language id)*/
  TEST_ASSERT(TEST_GET_STRING_DESC_C(dev, i, buf, sizeof(buf)) < 0);
}
TEST_END();


#endif 
TEST_ASSERT(usb_close(dev) >= 0);

TEST_SUITE_END();
