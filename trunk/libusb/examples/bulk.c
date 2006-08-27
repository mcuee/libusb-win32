#include <usb.h>
#include <stdio.h>

/* the device's vendor and product id */
#define MY_VID 1234
#define MY_PID 5678

/* the device's endpoints */
#define EP_IN 0x81
#define EP_OUT 0x01

#define BUF_SIZE 64

usb_dev_handle *open_dev(void);

usb_dev_handle *open_dev(void)
{
  struct usb_bus *bus;
  struct usb_device *dev;

  for(bus = usb_get_busses(); bus; bus = bus->next) 
    {
      for(dev = bus->devices; dev; dev = dev->next) 
        {
          if(dev->descriptor.idVendor == MY_VID
             && dev->descriptor.idProduct == MY_PID)
            {
              return usb_open(dev);
            }
        }
    }
  return NULL;
}

int main(void)
{
  usb_dev_handle *dev = NULL; /* the device handle */
  char tmp[BUF_SIZE];

  usb_init(); /* initialize the library */
  usb_find_busses(); /* find all busses */
  usb_find_devices(); /* find all connected devices */


  if(!(dev = open_dev()))
    {
      printf("error: device not found!\n");
      return 0;
    }

  if(usb_set_configuration(dev, 1) < 0)
    {
      printf("error: setting config 1 failed\n");
      usb_close(dev);
      return 0;
    }

  if(usb_claim_interface(dev, 0) < 0)
    {
      printf("error: claiming interface 0 failed\n");
      usb_close(dev);
      return 0;
    }
  
  if(usb_bulk_write(dev, EP_OUT, tmp, sizeof(tmp), 5000) 
     != sizeof(tmp))
    {
      printf("error: bulk write failed\n");
    }

  if(usb_bulk_read(dev, EP_IN, tmp, sizeof(tmp), 5000) 
     != sizeof(tmp))
    {
      printf("error: bulk read failed\n");
    }

  usb_release_interface(dev, 0);
  usb_close(dev);

  return 0;
}
