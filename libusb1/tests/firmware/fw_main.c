

#include "fw_descriptors.h"
#include "usb.h"
#include "types.h"
//#include "fx2regs.h"

#define EPCS_STALL 0
#define EPCS_BUSY 1
#define EPCS_HSNAK 7

#define USBCS_HSM 7


#define BIT_GET(data, bit) ((data) & (1 << (bit)))
#define BIT_SET(data, bit) data |= (1 << (bit))
#define BIT_CLR(data, bit) data &= ~(1 << (bit))

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

#define SET_SUDPTR_AUTO(address) \
do {                                    \
  BIT_SET(SUDPTRCTL, SUDPTRCTL_AUTO); \
  SUDPTRH = MSB(address); \
  SUDPTRL = LSB(address); \
} while(0)

/*     ep##BCH = 0;            \ */
/*     DELAY_US();             \ */

#define EP_IN_ARM(ep, size) \
do {                        \
    ep##BCL = size;         \
    DELAY_US();             \
} while(0)


#define EP_OUT_ARM(ep) ep##BCL = 0; DELAY_US()

#define EP_CFG_VALID     (1 << 7)
#define EP_CFG_DIR_IN    (1 << 6)
#define EP_CFG_DIR_OUT   0
#define EP_CFG_TYPE_ISO  (1 << 4)
#define EP_CFG_TYPE_BULK (2 << 4)
#define EP_CFG_TYPE_INT  (3 << 4)
#define EP_CFG_QUAD      0
#define EP_CFG_DOUBLE    2

#define DELAY_US()    \
  _asm                \
  nop; nop; nop; nop; \
  nop; nop; nop; nop; \
  nop; nop; nop; nop; \
  _endasm

#define EP_CONFIG(ep, dir, type) \
do { DELAY_US(); ep##CFG = dir | type | EP_CFG_DOUBLE |  EP_CFG_VALID; DELAY_US();} while(0)


#define EPCS_EMPTY 2
#define EPCS_FULL  3

#define EP_IS_FULL(ep) BIT_GET(ep##CS, EPCS_FULL)
#define EP_IS_EMPTY(ep) BIT_GET(ep##CS, EPCS_EMPTY)

#define TOGCTL_RESET 5

#define SUDPTRCTL_AUTO 0


volatile xdata at 0xe68a unsigned char EP0BCH;
volatile xdata at 0xe68b unsigned char EP0BCL;
volatile xdata at 0xe68d unsigned char EP1OUTBC;
volatile xdata at 0xe68f unsigned char EP1INBC;
volatile xdata at 0xe690 unsigned char EP2BCH;
volatile xdata at 0xe691 unsigned char EP2BCL;
volatile xdata at 0xe694 unsigned char EP4BCH;
volatile xdata at 0xe695 unsigned char EP4BCL;
volatile xdata at 0xe698 unsigned char EP6BCH;
volatile xdata at 0xe699 unsigned char EP6BCL;
volatile xdata at 0xe69c unsigned char EP8BCH;
volatile xdata at 0xe69d unsigned char EP8BCL;
volatile xdata at 0xe6a0 unsigned char EP0CS;
volatile xdata at 0xe6a1 unsigned char EP1OUTCS;
volatile xdata at 0xe6a2 unsigned char EP1INCS;
volatile xdata at 0xe6a3 unsigned char EP2CS;
volatile xdata at 0xe6a4 unsigned char EP4CS;
volatile xdata at 0xe6a5 unsigned char EP6CS;
volatile xdata at 0xe6a6 unsigned char EP8CS;

volatile xdata at 0xe610 unsigned char EP1OUTCFG;
volatile xdata at 0xe611 unsigned char EP1INCFG;
volatile xdata at 0xe612 unsigned char EP2CFG;
volatile xdata at 0xe613 unsigned char EP4CFG;
volatile xdata at 0xe614 unsigned char EP6CFG;
volatile xdata at 0xe615 unsigned char EP8CFG;
volatile xdata at 0xe618 unsigned char EP2FIFOCFG;
volatile xdata at 0xe619 unsigned char EP4FIFOCFG; /* Endpoint 4 FIFO 
                                                      Configuration */
volatile xdata at 0xe61a unsigned char EP6FIFOCFG; /* Endpoint 6 FIFO 
                                                      Configuration */
volatile xdata at 0xe61b unsigned char EP8FIFOCFG;

volatile xdata at 0xe648 unsigned char INPKTEND;
volatile xdata at 0xe649 unsigned char OUTPKTEND;

volatile xdata at 0xe683 unsigned char TOGCTL;
volatile xdata at 0xe680 unsigned char USBCS;
#define USBCS_DISCON 3
#define USBCS_RENUM 1

volatile xdata at 0xe600 unsigned char CPUCS;
#define CPUCS_CLKSPD1 4

volatile xdata at 0xe60a unsigned char REVID; /* Chip Revision */
volatile xdata at 0xe60b unsigned char REVCTL; /* Chip Revision Control */

volatile xdata at 0xe6b3 unsigned char SUDPTRH;
volatile xdata at 0xe6b4 unsigned char SUDPTRL;
volatile xdata at 0xe6b5 unsigned char SUDPTRCTL;

volatile xdata at 0xe740 unsigned char EP0BUF[64]; /* EP0 IN/OUT Buffer */
volatile xdata at 0xe780 unsigned char EP1OUTBUF[64]; /* EP1 OUT Buffer */
volatile xdata at 0xe7c0 unsigned char EP1INBUF[64]; /* EP1 IN Buffer */
volatile xdata at 0xf000 unsigned char EP2FIFOBUF[1024]; /* 512/1024-byte 
                                                            EP2/FIFO Buffer */
volatile xdata at 0xf400 unsigned char EP4FIFOBUF[1024]; /* 512/1024-byte 
                                                            EP4/FIFO Buffer */
volatile xdata at 0xf800 unsigned char EP6FIFOBUF[1024]; /* 512/1024-byte 
                                                            EP6/FIFO Buffer */
volatile xdata at 0xfc00 unsigned char EP8FIFOBUF[1024]; /* 512/1024-byte 
                                                            EP8/FIFO Buffer */

volatile xdata at 0xe604 unsigned char FIFORESET;

volatile xdata at 0xe6b8 unsigned char SETUPDAT[8];
#define SETUP_REQUEST_TYPE (SETUPDAT[0])
#define SETUP_REQUEST (SETUPDAT[1])
#define SETUP_VALUE_L (SETUPDAT[2])
#define SETUP_VALUE_H (SETUPDAT[3])
#define SETUP_VALUE ((((uint16_t)(SETUP_VALUE_H)) << 8) | SETUP_VALUE_L)
#define SETUP_INDEX_L (SETUPDAT[4])
#define SETUP_INDEX_H (SETUPDAT[5])
#define SETUP_INDEX ((((uint16_t)(SETUP_INDEX_H)) << 8) | SETUP_INDEX_L)
#define SETUP_LENGTH_L (SETUPDAT[6])
#define SETUP_LENGTH_H (SETUPDAT[7])
#define SETUP_LENGTH ((((uint16_t)(SETUP_LENGTH_H)) << 8) | SETUP_LENGTH_L)

static struct {
  uint8_t config;
  bool_t remote_wakeup;
  uint16_t alt_setting[FW_NUM_INTERFACES];
} device_state;

static void reset_toggle(uint8_t ep);
static void ack_ep0(void);
static void stall_ep0(void);
static void set_ep0_data_size(uint16_t size);
static void init(void);
static bool_t init_interface(uint16_t number, uint16_t alt_setting);
static void delay_ms(volatile uint8_t ms);

static void dispatch_setup(void);
static bool_t get_status_device(void);
static bool_t get_configuration(void);
static bool_t get_descriptor(void);
static bool_t get_status_interface(void);
static bool_t get_interface(void);
static bool_t get_status_endpoint(void);
static bool_t clear_feature_device(void);
static bool_t set_feature_device(void);
static bool_t set_configuration(void);
static bool_t get_descriptor(void);
static bool_t clear_feature_interface(void);
static bool_t set_feature_interface(void);
static bool_t set_interface(void);
static bool_t clear_feature_endpoint(void);
static bool_t set_feature_endpoint(void);

static bool_t dispatch_vendor_class(void);


static void handle_ep82(void);
static void handle_ep4(void);
static void handle_ep86(void);
static void handle_ep8(void);

static const struct {
  uint8_t request_type;
  uint8_t request;
  bool_t (*cb)(void);
} dispatch_table[] = {
  { USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE, 
    USB_REQ_GET_STATUS, get_status_device },
  { USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE, 
    USB_REQ_GET_CONFIGURATION, get_configuration },
  { USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE, 
    USB_REQ_GET_DESCRIPTOR, get_descriptor },

  { USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
    USB_REQ_GET_STATUS, get_status_interface },
  { USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
    USB_REQ_GET_INTERFACE, get_interface },
  { USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
    USB_REQ_GET_DESCRIPTOR, get_descriptor },

  { USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT,
    USB_REQ_GET_STATUS, get_status_endpoint },

  { USB_ENDPOINT_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
    USB_REQ_CLEAR_FEATURE, clear_feature_device },
  { USB_ENDPOINT_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
    USB_REQ_SET_FEATURE, set_feature_device },
  { USB_ENDPOINT_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
    USB_REQ_SET_CONFIGURATION, set_configuration },
  { USB_ENDPOINT_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
    USB_REQ_SET_DESCRIPTOR, get_descriptor },
  
  { USB_ENDPOINT_OUT | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
    USB_REQ_CLEAR_FEATURE, clear_feature_interface },
  { USB_ENDPOINT_OUT | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
    USB_REQ_SET_FEATURE, set_feature_interface },
  { USB_ENDPOINT_OUT | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
    USB_REQ_SET_INTERFACE, set_interface },

  { USB_ENDPOINT_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT,
    USB_REQ_CLEAR_FEATURE, clear_feature_endpoint },
  { USB_ENDPOINT_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT,
    USB_REQ_CLEAR_FEATURE, set_feature_endpoint }
};




static void reset_toggle(uint8_t ep)
{
  TOGCTL = (ep & 0x80) >> 3 | (ep & 0x0f);
  BIT_SET(TOGCTL, TOGCTL_RESET);
}

static void ack_ep0(void)
{
  BIT_SET(EP0CS, EPCS_HSNAK);
}

static void stall_ep0()
{
  BIT_SET(EP0CS, EPCS_STALL);
  //   BIT_SET(EP0CS, EPCS_HSNAK);
}

static void set_ep0_data_size(uint16_t size)
{
  EP0BCH = (uint8_t)(size >> 8);
  EP0BCL = (uint8_t)size;
}

static void init(void)
{ 
  uint8_t i;

  BIT_SET(USBCS, USBCS_RENUM);
  BIT_SET(USBCS, USBCS_DISCON);
  //USBCS |= bmDISCON | bmRENUM;
 	delay_ms(250); 	 
 	delay_ms(250); 	 
 	delay_ms(250); 	 
  BIT_CLR(USBCS, USBCS_DISCON);
  //  USBCS &= ~bmDISCON;
 	delay_ms(250); 	 

  device_state.config = 0;
  device_state.remote_wakeup = FALSE;

  for(i = 0; i < FW_NUM_INTERFACES; i++) {
    device_state.alt_setting[i] = 0;
  }

  BIT_SET(CPUCS, CPUCS_CLKSPD1);
  //CPUCS = bmCLKSPD1;	// CPU runs @ 48 MHz

  //  CKCON = 0;		// MOVX takes 2 cycles
  //  IFCONFIG=0xc0;
  DELAY_US();
  REVCTL = 0x03;
  DELAY_US();

  init_interface(FW_INTERFACE_0, 0);

  EP2FIFOBUF[0] = 1;
  EP_IN_ARM(EP2, 64);
  EP2FIFOBUF[0] = 1;
  EP_IN_ARM(EP2, 64);

  EP_OUT_ARM(EP4);
  EP_OUT_ARM(EP4);

#ifndef HID
  init_interface(FW_INTERFACE_1, 0);

  EP6FIFOBUF[0] = 0;
  EP_IN_ARM(EP6, 64);
  EP6FIFOBUF[0] = 0;
  EP_IN_ARM(EP6, 64);

  EP_OUT_ARM(EP8);
  EP_OUT_ARM(EP8);
#endif
}

static bool_t init_interface(uint16_t number, uint16_t alt_setting)
{
#ifdef HID

/*   if(number != FW_INTERFACE_0 || alt_setting != 0) */
/*     return FALSE; */
  EP_CONFIG(EP2, EP_CFG_DIR_IN, EP_CFG_TYPE_INT);
  EP_CONFIG(EP4, EP_CFG_DIR_OUT, EP_CFG_TYPE_INT);

  reset_toggle(0x82);
  reset_toggle(0x04);


  return TRUE;

#else

  if((number != FW_INTERFACE_0 
     && number != FW_INTERFACE_1) 
     || alt_setting > FW_NUM_ALT_SETTINGS)
    return FALSE;

  switch(alt_setting) {
    case 0:
      EP_CONFIG(EP2, EP_CFG_DIR_IN, EP_CFG_TYPE_BULK);
      EP_CONFIG(EP4, EP_CFG_DIR_OUT, EP_CFG_TYPE_BULK);
      break;
    case 1:
      EP_CONFIG(EP6, EP_CFG_DIR_IN, EP_CFG_TYPE_INT);
      EP_CONFIG(EP8, EP_CFG_DIR_OUT, EP_CFG_TYPE_INT);
      break;
    case 2:
      EP_CONFIG(EP6, EP_CFG_DIR_IN, EP_CFG_TYPE_ISO);
      EP_CONFIG(EP8, EP_CFG_DIR_OUT, EP_CFG_TYPE_ISO);
  }

  if(number == FW_INTERFACE_0) {
    
    reset_toggle(0x82);
    reset_toggle(0x04);
    
  }
  if(number == FW_INTERFACE_1) {
    reset_toggle(0x86);
    reset_toggle(0x08);
  }
  return TRUE;
#endif
}

static void delay_ms(volatile uint8_t ms)
{
  volatile uint8_t i;
  while(ms--) {
    for(i = 0; i < 200; i++) {
      DELAY_US();
      DELAY_US();
      DELAY_US();
      DELAY_US();
      DELAY_US();
    }
  }
}

static void dispatch_setup(void)
{
  uint8_t i;
  bool_t ret = FALSE;
  for(i = 0; i < ARRAY_SIZE(dispatch_table); i++) {
    if(dispatch_table[i].request_type == SETUP_REQUEST_TYPE
       && dispatch_table[i].request == SETUP_REQUEST) {
      return dispatch_table[i].cb() ? ack_ep0() : stall_ep0();
    }
  }
  ack_ep0();
  //  return dispatch_vendor_class() ? ack_ep0() : stall_ep0();
}

static bool_t get_status_device(void)
{
  EP0BUF[0] = 0x00;
  EP0BUF[1] = 0x00;
  if(device_state.remote_wakeup)
    BIT_SET(EP0BUF[0], USB_FEATURE_REMOTE_WAKEUP_BIT);
  set_ep0_data_size(2);
  return TRUE;
}

static bool_t get_status_interface(void)
{
  EP0BUF[0] = 0x00;
  EP0BUF[1] = 0x00;
  set_ep0_data_size(2);
  return TRUE;
}

static bool_t get_status_endpoint(void)
{
  EP0BUF[0] = 0x00;
  EP0BUF[1] = 0x00;
  switch(SETUP_INDEX_L & 0x0F) {
  case 0: if(BIT_GET(EP0CS, EPCS_STALL)) BIT_SET(EP0BUF[0], 0); break;
  case 2: if(BIT_GET(EP2CS, EPCS_STALL)) BIT_SET(EP0BUF[0], 0); break;
  case 4: if(BIT_GET(EP4CS, EPCS_STALL)) BIT_SET(EP0BUF[0], 0); break;
  case 6: if(BIT_GET(EP6CS, EPCS_STALL)) BIT_SET(EP0BUF[0], 0); break;
  case 8: if(BIT_GET(EP8CS, EPCS_STALL)) BIT_SET(EP0BUF[0], 0); break;
  }

  set_ep0_data_size(2);
  return TRUE;
}

static bool_t get_configuration(void)
{
  EP0BUF[0] = device_state.config;
  set_ep0_data_size(1);
  return TRUE;
}

static bool_t set_configuration(void)
{
  uint8_t i;

#ifdef HID

  if(SETUP_VALUE_L == 0 || SETUP_VALUE_L == FW_CONFIG_0) {
    device_state.config = SETUP_VALUE_L;
    for(i = 0; i < FW_NUM_INTERFACES; i++)
      device_state.alt_setting[i] = 0;
    //  init_interface(FW_INTERFACE_0, 0);
    return TRUE;
  }
#else

  if(SETUP_VALUE_L == 0 || SETUP_VALUE_L == FW_CONFIG_0
     || SETUP_VALUE_L == FW_CONFIG_1) {
    device_state.config = SETUP_VALUE_L;
    for(i = 0; i < FW_NUM_INTERFACES; i++)
      device_state.alt_setting[i] = 0;
    init_interface(FW_INTERFACE_0, 0);
    init_interface(FW_INTERFACE_1, 0);
    return TRUE;
  }
#endif

  return FALSE;
}

static bool_t get_descriptor(void)
{
  uint8_t size = 0;
  uint8_t i = 0;
  const uint8_t *desc = NULL;

  switch(SETUP_VALUE_H) {
  case USB_DT_DEVICE:
  case USB_DT_DEVICE_QUALIFIER:
    if(!SETUP_VALUE_L)
      desc = fw_desc_get_device();
    break;
  case USB_DT_CONFIG:
  case USB_DT_OTHER_SPEED_CONFIG:
    desc = fw_desc_get_config(SETUP_VALUE_L);
    break;
  case USB_DT_STRING:
    desc = fw_desc_get_string(SETUP_VALUE_L);
    break;
#ifdef HID
  case USB_DT_HID:
    desc = fw_desc_get_hid();
    break;
  case USB_DT_REPORT:
    size = fw_desc_get_size();
    desc = fw_desc_get_report();
    for(i = 0; i < size; i++)
      EP0BUF[i] = desc[i];
    EP0BCL = size;
    return TRUE;
#endif
  }
  
  if(desc) {
    SET_SUDPTR_AUTO(desc);
    return TRUE;
  }
  return FALSE;
}

static bool_t get_interface(void)
{
  if(SETUP_INDEX == FW_INTERFACE_0) {
    EP0BUF[0] = device_state.alt_setting[0];
    set_ep0_data_size(1);
    return TRUE;
  }
#ifndef HID
  if(SETUP_INDEX == FW_INTERFACE_1) {
    EP0BUF[0] = device_state.alt_setting[1];
    set_ep0_data_size(1);
    return TRUE;
  }
#endif
  return FALSE;
}

static bool_t set_interface(void)
{
  if(init_interface(SETUP_INDEX, SETUP_VALUE)) {
    device_state.alt_setting[0] = SETUP_VALUE;
    return TRUE;
  }
  return FALSE;
}

static bool_t clear_feature_device(void)
{
  device_state.remote_wakeup = FALSE;
  return TRUE;
}

static bool_t clear_feature_interface(void)
{ 
  return TRUE;
}

static bool_t clear_feature_endpoint(void)
{
  switch(SETUP_INDEX_L & 0x0F) {
  case 0: BIT_CLR(EP0CS, EPCS_STALL); break;
  case 2: BIT_CLR(EP2CS, EPCS_STALL); break;
  case 4: BIT_CLR(EP4CS, EPCS_STALL); break;
  case 6: BIT_CLR(EP6CS, EPCS_STALL); break;
  case 8: BIT_CLR(EP8CS, EPCS_STALL); break;
  }
  return TRUE;
}

static bool_t set_feature_device(void)
{
  if(SETUP_VALUE_L == 1)
    device_state.remote_wakeup = TRUE;
  return TRUE;
}

static bool_t set_feature_interface(void)
{
  return TRUE;
}

static bool_t set_feature_endpoint(void)
{
  switch(SETUP_INDEX_L & 0x0F) {
  case 0: BIT_SET(EP0CS, EPCS_STALL); break;
  case 2: BIT_SET(EP2CS, EPCS_STALL); break;
  case 4: BIT_SET(EP4CS, EPCS_STALL); break;
  case 6: BIT_SET(EP6CS, EPCS_STALL); break;
  case 8: BIT_SET(EP8CS, EPCS_STALL); break;
  }
  return TRUE;
}

static bool_t dispatch_vendor_class(void)
{
  return FALSE;
}

static void handle_ep82(void)
{
  EP2FIFOBUF[0] = 1;
  EP_IN_ARM(EP2, 64);
}

static void handle_ep4(void)
{
  EP_OUT_ARM(EP4);
}

static void handle_ep86(void)
{
  EP_IN_ARM(EP6, 64);
}

static void handle_ep8(void)
{
  EP_OUT_ARM(EP8);
}

void main()
{
  init();
  while(1) {
    if(BIT_GET(EP0CS, EPCS_HSNAK))
       dispatch_setup();
    if(!EP_IS_FULL(EP2))
      handle_ep82();
    if(!EP_IS_FULL(EP6))
      handle_ep86();
    if(!EP_IS_EMPTY(EP4))
      handle_ep4();
    if(!EP_IS_EMPTY(EP8))
      handle_ep8();
  }
}
