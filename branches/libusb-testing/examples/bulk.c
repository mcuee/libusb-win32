#include <usb.h>
#include <stdio.h>

//#define TEST_SET_CONFIGURATION
#define TEST_CLAIM_INTERFACE
#define TEST_BULK_READ
#define TEST_BULK_WRITE

/* the device's vendor and product id */
#define MY_VID 1234
#define MY_PID 5678

#define MY_CONFIG 1
#define MY_INTF 0

/* the device's endpoints */
#define EP_IN 0x81
#define EP_OUT 0x01

#define BUF_SIZE 64

usb_dev_handle *open_dev(void);

usb_dev_handle *open_dev(void)
{
    struct usb_bus *bus;
    struct usb_device *dev;

    for (bus = usb_get_busses(); bus; bus = bus->next)
    {
        for (dev = bus->devices; dev; dev = dev->next)
        {
            if (dev->descriptor.idVendor == MY_VID
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
	int ret;

    usb_init(); /* initialize the library */
    usb_find_busses(); /* find all busses */
    usb_find_devices(); /* find all connected devices */


    if (!(dev = open_dev()))
    {
        printf("error opening device: \n%s\n",usb_strerror());
        return 0;
    }
	else
	{
		printf("success: device %04X:%04X opened\n",MY_VID,MY_PID);
	}

#ifdef TEST_SET_CONFIGURATION
    if (usb_set_configuration(dev, MY_CONFIG) < 0)
    {
		printf("error setting config #%d: %s\n",MY_CONFIG,usb_strerror());
        usb_close(dev);
        return 0;
    }
	else
	{
		printf("success: set configuration #%d\n", MY_CONFIG);
	}
#endif

#ifdef TEST_CLAIM_INTERFACE
    if (usb_claim_interface(dev, 0) < 0)
    {
		printf("error claiming interface #%d:\n%s\n", MY_INTF, usb_strerror());
        usb_close(dev);
        return 0;
    }
	else
	{
		printf("success: claim_interface #%d\n", MY_INTF);
	}
#endif

#ifdef TEST_BULK_WRITE
	ret = usb_bulk_write(dev, EP_OUT, tmp, sizeof(tmp), 5000);
    if (ret <= 0)
    {
		printf("error writing:\n%s\n",usb_strerror());
    }
	else
	{
        printf("success: bulk write %d bytes\n",ret);
	}
#endif

#ifdef TEST_BULK_READ
	ret = usb_bulk_read(dev, EP_IN, tmp, sizeof(tmp), 5000);
    if (ret <= 0)
    {
		printf("error reading:\n%s\n",usb_strerror());
    }
	else
	{
        printf("success: bulk read %d bytes\n",ret);
	}
#endif

    usb_release_interface(dev, 0);
    usb_close(dev);
    printf("Done.\n");

    return 0;
}
