#ifndef __CONFIG_H__
#define __CONFIG_H__

#define MSB(a) ((unsigned char)(((unsigned int)(a)) >> 8))
#define LSB(a) ((unsigned char)(((unsigned int)(a)) & 0x000000ff))

#ifdef HID
#define VENDOR_ID   0x1234
#define PRODUCT_ID  0x0002

#define TEST_NUM_CONFIGS 1
#define TEST_CONFIG_VALUE_0 1

#define TEST_NUM_STRINGS 4

#define TEST_NUM_INTERFACES 1
#define TEST_NUM_ALTSETTINGS 1
#define TEST_INTERFACE_NUMBER_0 0
#define TEST_NUM_ENDPOINTS_PER_INTERFACE 2


#define TEST_CONFIG_SIZE (USB_DT_CONFIG_SIZE  \
	 + USB_DT_INTERFACE_SIZE * TEST_NUM_INTERFACES * TEST_NUM_ALTSETTINGS \
   + USB_DT_ENDPOINT_SIZE * TEST_NUM_INTERFACES * TEST_NUM_ALTSETTINGS \
   * TEST_NUM_ENDPOINTS_PER_INTERFACE + USB_DT_HID_SIZE)
 
#ifndef __DESCRIPTORS_C__


extern const char device_descriptor[];
extern const char * const config_descriptors_high[];
extern const char * const config_descriptors_full[];
extern const char * const string_descriptors[];
extern const char * const string_descriptors_simple[];
extern const char hid_descriptor[];
extern const char report_descriptor[];

#endif
#else

#define VENDOR_ID   0x1234
#define PRODUCT_ID  0x0001

#define TEST_NUM_CONFIGS 2
#define TEST_CONFIG_VALUE_0 1
#define TEST_CONFIG_VALUE_1 2

#define TEST_NUM_STRINGS 4

#define TEST_NUM_INTERFACES 2
#define TEST_NUM_ALTSETTINGS 3
#define TEST_INTERFACE_NUMBER_0 0
#define TEST_INTERFACE_NUMBER_1 3
#define TEST_NUM_ENDPOINTS_PER_INTERFACE 2


#define TEST_CONFIG_SIZE (USB_DT_CONFIG_SIZE  \
	 + USB_DT_INTERFACE_SIZE * TEST_NUM_INTERFACES * TEST_NUM_ALTSETTINGS \
   + USB_DT_ENDPOINT_SIZE * TEST_NUM_INTERFACES * TEST_NUM_ALTSETTINGS \
   * TEST_NUM_ENDPOINTS_PER_INTERFACE)

#ifndef __DESCRIPTORS_C__


extern const char device_descriptor[];
extern const char * const config_descriptors_high[];
extern const char * const config_descriptors_full[];
extern const char * const string_descriptors[];
extern const char * const string_descriptors_simple[];

#endif
#endif
#endif
