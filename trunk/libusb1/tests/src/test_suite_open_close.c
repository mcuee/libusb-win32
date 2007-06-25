#include "test_main.h"

TEST_SUITE_BEGIN(open_close);
usb_dev_handle *dev;

TEST_BEGIN(test0);
TEST_ASSERT(usb_open(NULL) == NULL);
TEST_END();

TEST_BEGIN(test1);
TEST_ASSERT(usb_close(NULL) < 0);
TEST_END();

TEST_BEGIN(test2);
TEST_ASSERT(dev = test_open());
TEST_ASSERT(usb_close(dev) >= 0);
TEST_END();

TEST_SUITE_END();
