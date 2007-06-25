#include "test_main.h"

TEST_SUITE_BEGIN(interrupt_hid);
usb_dev_handle *dev;
uint8_t buf[1024];

TEST_ASSERT(dev = test_open());

/* invalid device handle */
TEST_BEGIN(test0);
TEST_ASSERT(usb_interrupt_write(NULL, TEST_HID_OUT_EP, buf, 
                                TEST_HID_OUTPUT_REPORT_SIZE,
                                TEST_DEFAULT_TIMEOUT) < 0);
TEST_ASSERT(usb_interrupt_read(NULL, TEST_HID_IN_EP, buf, 
                               TEST_HID_INPUT_REPORT_SIZE,
                               TEST_DEFAULT_TIMEOUT) < 0);
TEST_END();

/* device not configured */
TEST_BEGIN(test1);
TEST_ASSERT(usb_interrupt_write(dev, TEST_HID_OUT_EP, buf, 
                                TEST_HID_OUTPUT_REPORT_SIZE,
                                TEST_DEFAULT_TIMEOUT) < 0);
TEST_ASSERT(usb_interrupt_read(dev, TEST_HID_IN_EP, buf, 
                                TEST_HID_INPUT_REPORT_SIZE,
                               TEST_DEFAULT_TIMEOUT) < 0);
TEST_END();

/* interface not claimed */
TEST_BEGIN(test2);
TEST_ASSERT(usb_set_configuration(dev, TEST_HID_CONFIG) >= 0);
TEST_ASSERT(usb_interrupt_write(dev, TEST_HID_OUT_EP, buf, 
                                TEST_HID_OUTPUT_REPORT_SIZE,
                                TEST_DEFAULT_TIMEOUT) < 0);
TEST_ASSERT(usb_interrupt_read(dev, TEST_HID_IN_EP, buf, 
                                TEST_HID_INPUT_REPORT_SIZE,
                               TEST_DEFAULT_TIMEOUT) < 0);
TEST_END();

/* device configured, interface claimed */
TEST_BEGIN(test3);
TEST_ASSERT(usb_set_configuration(dev, TEST_HID_CONFIG) >= 0);
TEST_ASSERT(usb_claim_interface(dev, TEST_HID_INTERFACE) >= 0);

buf[0] = 0;
TEST_ASSERT(usb_interrupt_write(dev, TEST_HID_OUT_EP, buf, 
                                TEST_HID_OUTPUT_REPORT_SIZE,
                                TEST_DEFAULT_TIMEOUT) 
            == TEST_HID_OUTPUT_REPORT_SIZE);
buf[0] = 0;
TEST_ASSERT(usb_interrupt_read(dev, TEST_HID_IN_EP, buf, 
                                TEST_HID_INPUT_REPORT_SIZE,
                                TEST_DEFAULT_TIMEOUT) 
            == TEST_HID_INPUT_REPORT_SIZE);

TEST_ASSERT(usb_release_interface(dev, TEST_HID_INTERFACE) >= 0);
TEST_END();


TEST_ASSERT(usb_close(NULL) < 0);

TEST_SUITE_END();
