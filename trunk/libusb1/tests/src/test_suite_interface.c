#include "test_main.h"

TEST_SUITE_BEGIN(interface);
usb_dev_handle *dev;
unsigned char interface;
int i, c, a;

TEST_ASSERT(dev = test_open());

/* invalid device handle */
TEST_BEGIN(test0);
TEST_ASSERT(usb_claim_interface(NULL, 0) < 0);
TEST_ASSERT(usb_release_interface(NULL, 0) < 0);
TEST_ASSERT(usb_set_altinterface(NULL, 0) < 0);
TEST_END();

/* invalid configuration */
TEST_BEGIN(test1);
TEST_ASSERT(usb_set_configuration(dev, 0) >= 0);
TEST_ASSERT(usb_claim_interface(dev, 0) < 0);
TEST_ASSERT(usb_claim_interface(dev, 1) < 0);
TEST_ASSERT(usb_claim_interface(dev, 2) < 0);
TEST_ASSERT(usb_claim_interface(dev, 3) < 0);
TEST_ASSERT(usb_claim_interface(dev, 4) < 0);
TEST_ASSERT(usb_claim_interface(dev, 5) < 0);
TEST_END();


/* valid configuration, valid interfaces */
TEST_BEGIN(test3);
if(!test_is_hid(dev)) {
  TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 3) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 3) >= 0);

  TEST_ASSERT(usb_set_configuration(dev, 2) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 3) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 3) >= 0);
} else {
  TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
}
TEST_END();

/* valid configuration, invalid interfaces */
TEST_BEGIN(test4);
TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
TEST_ASSERT(usb_claim_interface(dev, 10) < 0);
TEST_ASSERT(usb_release_interface(dev, 10) < 0);
if(!test_is_hid(dev)) {
  TEST_ASSERT(usb_set_configuration(dev, 2) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 10) < 0);
  TEST_ASSERT(usb_release_interface(dev, 10) < 0);
}
TEST_END();

/* claiming and releasing interface multiple times */
TEST_BEGIN(test5);
TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
TEST_ASSERT(usb_release_interface(dev, 0) < 0);
TEST_END();

/* releasing wrong interface */
TEST_BEGIN(test6);
TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
TEST_ASSERT(usb_release_interface(dev, 1) < 0);
TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
TEST_END();

/* changing configuration while interface is claimed */
TEST_BEGIN(test7);
TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
TEST_ASSERT(usb_set_configuration(dev, 0) < 0);
TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
TEST_END();

/* claiming more than one interface */
TEST_BEGIN(test8);
if(!test_is_hid(dev)) {
  TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 3) < 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
}
TEST_END();

/* setting alternate setting, valid values */
TEST_BEGIN(test9);
if(test_is_hid(dev)) {
  TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 0) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
} else {
  TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
  
  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 1) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 2) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
  
  TEST_ASSERT(usb_claim_interface(dev, 3) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 1) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 2) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 3) >= 0);
  
  TEST_ASSERT(usb_set_configuration(dev, 2) >= 0);
  
  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 1) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 2) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
  
  TEST_ASSERT(usb_claim_interface(dev, 3) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 1) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 2) >= 0);
  TEST_ASSERT(usb_release_interface(dev, 3) >= 0);
}
TEST_END();


/* setting alternate setting, invalid values */
TEST_BEGIN(test10);
if(test_is_hid(dev)) {
  TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 10) < 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
} else {
  TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);

  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 10) < 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
  
  TEST_ASSERT(usb_claim_interface(dev, 3) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 10) < 0);
  TEST_ASSERT(usb_release_interface(dev, 3) >= 0);
  
  TEST_ASSERT(usb_set_configuration(dev, 2) >= 0);
  
  TEST_ASSERT(usb_claim_interface(dev, 0) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 10) < 0);
  TEST_ASSERT(usb_release_interface(dev, 0) >= 0);
  
  TEST_ASSERT(usb_claim_interface(dev, 3) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 10) < 0);
  TEST_ASSERT(usb_release_interface(dev, 3) >= 0);
}
TEST_END();

/* setting alternate setting, no interface claimed */
TEST_BEGIN(test11);
if(test_is_hid(dev)) {
  TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 0) < 0);
} else {
  TEST_ASSERT(usb_set_configuration(dev, 1) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 0) < 0);
  TEST_ASSERT(usb_set_altinterface(dev, 3) < 0);
  
  TEST_ASSERT(usb_set_configuration(dev, 2) >= 0);
  TEST_ASSERT(usb_set_altinterface(dev, 0) < 0);
  TEST_ASSERT(usb_set_altinterface(dev, 3) < 0);
}
TEST_END();

TEST_ASSERT(usb_close(dev) >= 0);

TEST_SUITE_END();
