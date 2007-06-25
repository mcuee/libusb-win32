#ifndef __FW_DESCRIPTORS_H__
#define __FW_DESCRIPTORS_H__

#include "types.h"

#ifdef HID
#define FW_CONFIG_0 1
#define FW_CONFIG_1 0xFF
#define FW_INTERFACE_0 0
#define FW_INTERFACE_1 0xFF
#define FW_NUM_INTERFACES 1
#else
#define FW_CONFIG_0 1
#define FW_CONFIG_1 2
#define FW_INTERFACE_0 0
#define FW_INTERFACE_1 3
#define FW_NUM_INTERFACES 2
#define FW_NUM_ALT_SETTINGS 3
#endif

const void *fw_desc_get_device(void);
const void *fw_desc_get_config(uint8_t index);
const void *fw_desc_get_string(uint8_t index);

#ifdef HID
const void *fw_desc_get_hid(void);
const void *fw_desc_get_report(void);
uint8_t fw_desc_get_size(void);
#endif

#endif
