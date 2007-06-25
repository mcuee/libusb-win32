#ifndef __UNIT_TESTS_H__
#define __UNIT_TESTS_H__

#include <stdint.h>
#include "usb.h"
#include "unit.h"
#include "../firmware/config.h"


#define TEST_FX2_VID 0x04b4
#define TEST_FX2_PID 0x8613
#define TEST_VID 0x1234
#define TEST_PID 0x0001
#define TEST_HID_VID 0x1234
#define TEST_HID_PID 0x0002

#define TEST_FIRMWARE     "./firmware/main.hex"
#define TEST_HID_FIRMWARE "./firmware/hid_main.hex"

#define TEST_DEFAULT_TIMEOUT 1000

#define TEST_HID_INPUT_REPORT_SIZE (63 + 1)
#define TEST_HID_OUTPUT_REPORT_SIZE (63 + 1)
#define TEST_HID_CONFIG 1
#define TEST_HID_INTERFACE 0
#define TEST_HID_IN_EP 0x81
#define TEST_HID_OUT_EP 0x02

typedef int bool_t;

usb_dev_handle *test_open(void);
int test_clear_feature(usb_dev_handle *dev, int recipient, int feature, 
                       int index);  
int test_get_altinterface(usb_dev_handle *dev, int interface, 
                          char *alt_setting);
int test_get_configuration(usb_dev_handle *dev, unsigned char *config);
int test_get_status(usb_dev_handle *dev, unsigned short *status, int recipient,
                    int index);
int test_set_feature(usb_dev_handle *dev, int recipient, int feature, 
                     int index);
bool_t test_is_hid(usb_dev_handle *dev);

#endif
