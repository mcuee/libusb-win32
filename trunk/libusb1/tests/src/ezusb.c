#include <inttypes.h>
#include <stdio.h>
#include "ezusb.h"

//#define	EZUSB_CS_ADDRESS      0x7F92
#define	EZUSB_CS_ADDRESS      0xE600
#define MAX_HEX_RECORD_LENGTH 16


typedef struct {
  uint32_t length;
  uint32_t address;
  uint32_t type;
  uint8_t data[MAX_HEX_RECORD_LENGTH];
} hex_record;


static int ezusb_read_hex_record(FILE *file, uint8_t *buffer);
static int ezusb_load(usb_dev_handle *dev, uint32_t address, uint32_t length, 
                      uint8_t *data);


static int ezusb_read_hex_record(FILE *file, uint8_t *buffer)
{
  char c;
  uint32_t i, length, address, type, read, tmp, checksum;
  
  if(feof(file))
    {
      return 0;
    }
    
  read = fscanf(file, ":%2X%4X%2X", &length, &address, &type);
  
  if(read != 3) 
    {
      return 0;
    }

  if(type != 0)
    return 1;

  checksum = length + (address >> 8) + address + type;
  
  if(length > MAX_HEX_RECORD_LENGTH) 
    {
      return 0;
    }
  
  for(i = 0; i < length; i++) 
    {
      read = fscanf(file, "%2X", &tmp);

      if(read != 1) 
        {
          return 0;
        }
      
      buffer[address + i] = (uint8_t)tmp;
      checksum += tmp;
    }

  read = fscanf(file, "%2X\n", &tmp);
  
  if((read != 1) || (((uint8_t)(checksum + tmp)) != 0x00)) 
    {
      return 0;
    }
  
  return 1;
}

static int ezusb_load(usb_dev_handle *dev, uint32_t address, uint32_t length, 
                      uint8_t *data)
{
  if(usb_control_msg(dev, 0x40, 0xA0, address, 0, data, length, 5000) 
     != length)
    {
      return 0;
    }
  
  return 1;
}


int ezusb_load_file(usb_dev_handle *dev, const char *hex_file)
{
  uint8_t ezusb_cs;
  FILE *firmware;
  int t;
  int packet_size;
  uint8_t buf[0x4000];
  uint8_t *p;

  if(!(firmware = fopen(hex_file, "r"))) 
    {
      return 0;
    }

  memset(buf, 0, sizeof(buf));
  while(ezusb_read_hex_record(firmware, buf));

  ezusb_cs = 1;
    
  if(!ezusb_load(dev, EZUSB_CS_ADDRESS, 1, &ezusb_cs))
    return 0;
    
  t = sizeof(buf);
  p = buf;
  while(t) {
    packet_size = t > 1024 ? 1024 : t;
    if(!ezusb_load(dev, p - buf, packet_size, p))
      return 0;
    p += packet_size;
    t -= packet_size;
  }

  ezusb_cs = 0;
  ezusb_load(dev, EZUSB_CS_ADDRESS, 1, &ezusb_cs);

  fclose(firmware);

  return 1;
}
