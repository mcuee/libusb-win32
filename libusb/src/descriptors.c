/*
 * Parses descriptors
 *
 * Copyright (c) 2001 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include "usbi.h"

int usb_get_descriptor_by_endpoint(usb_dev_handle *udev, int ep,
	unsigned char type, unsigned char index, void *buf, int size)
{
  memset(buf, 0, size);

  return usb_control_msg(udev, ep | USB_ENDPOINT_IN, USB_REQ_GET_DESCRIPTOR,
                        (type << 8) + index, 0, buf, size, 1000);
}

int usb_get_descriptor(usb_dev_handle *udev, unsigned char type,
	unsigned char index, void *buf, int size)
{
  memset(buf, 0, size);

  return usb_control_msg(udev, USB_ENDPOINT_IN, USB_REQ_GET_DESCRIPTOR,
                        (type << 8) + index, 0, buf, size, 1000);
}

/*
 * This code looks surprisingly similar to the code I wrote for the Linux
 * kernel. It's not a coincidence :)
 */

static int usb_parse_endpoint(struct usb_endpoint_descriptor *endpoint, unsigned char *buffer, int size)
{
  struct usb_descriptor_header *header;
  unsigned char *begin;
  int parsed = 0, len, numskipped;

  header = (struct usb_descriptor_header *)buffer;

  /* Everything should be fine being passed into here, but we sanity */
  /*  check JIC */
  if (header->bLength > size) {
    if (usb_debug >= 1)
      fprintf(stderr, "ran out of descriptors parsing\n");
    return -1;
  }
                
  if (header->bDescriptorType != USB_DT_ENDPOINT) {
    if (usb_debug >= 2)
      fprintf(stderr, "unexpected descriptor 0x%X, expecting endpoint descriptor, type 0x%X\n",
         endpoint->bDescriptorType, USB_DT_ENDPOINT);
    return parsed;
  }
  if (header->bLength == USB_DT_ENDPOINT_AUDIO_SIZE)
    memcpy(endpoint, buffer, USB_DT_ENDPOINT_AUDIO_SIZE);
  else
    memcpy(endpoint, buffer, USB_DT_ENDPOINT_SIZE);
        
  USB_LE16_TO_CPU(endpoint->wMaxPacketSize);

  buffer += header->bLength;
  size -= header->bLength;
  parsed += header->bLength;

  /* Skip over the rest of the Class Specific or Vendor Specific */
  /*  descriptors */
  begin = buffer;
  numskipped = 0;
  while (size >= sizeof(struct usb_descriptor_header)) {
    header = (struct usb_descriptor_header *)buffer;

    if (header->bLength < 2) {
      if (usb_debug >= 1)
        fprintf(stderr, "invalid descriptor length of %d\n", header->bLength);
      return -1;
    }

    /* If we find another "proper" descriptor then we're done  */
    if ((header->bDescriptorType == USB_DT_ENDPOINT) ||
        (header->bDescriptorType == USB_DT_INTERFACE) ||
        (header->bDescriptorType == USB_DT_CONFIG) ||
        (header->bDescriptorType == USB_DT_DEVICE))
      break;

    if (usb_debug >= 1)
      fprintf(stderr, "skipping descriptor 0x%X\n", header->bDescriptorType);
    numskipped++;

    buffer += header->bLength;
    size -= header->bLength;
    parsed += header->bLength;
  }

  if (numskipped && usb_debug >= 2)
    fprintf(stderr, "skipped %d class/vendor specific endpoint descriptors\n", numskipped);

  /* Copy any unknown descriptors into a storage area for drivers */
  /*  to later parse */
  len = (int)(buffer - begin);
  if (!len) {
    endpoint->extra = NULL;
    endpoint->extralen = 0;
    return parsed;
  }

  endpoint->extra = malloc(len);
  if (!endpoint->extra) {
    if (usb_debug >= 1)
      fprintf(stderr, "couldn't allocate memory for endpoint extra descriptors\n");
    endpoint->extralen = 0;
    return parsed;
  }

  memcpy(endpoint->extra, begin, len);
  endpoint->extralen = len;

  return parsed;
}

static int usb_parse_interface(struct usb_interface *interface,
	unsigned char *buffer, int size)
{
  int i, len, numskipped, retval, parsed = 0;
  struct usb_descriptor_header *header;
  struct usb_interface_descriptor *ifp;
  unsigned char *begin;

  interface->num_altsetting = 0;

  while (size > 0) {
    interface->altsetting = realloc(interface->altsetting, sizeof(struct usb_interface_descriptor) * (interface->num_altsetting + 1));
    if (!interface->altsetting) {
      if (usb_debug >= 1)
        fprintf(stderr, "couldn't malloc interface->altsetting\n");
      return -1;
    }

    ifp = interface->altsetting + interface->num_altsetting;
    interface->num_altsetting++;

    memcpy(ifp, buffer, USB_DT_INTERFACE_SIZE);

    /* Skip over the interface */
    buffer += ifp->bLength;
    parsed += ifp->bLength;
    size -= ifp->bLength;

    begin = buffer;
    numskipped = 0;

    /* Skip over any interface, class or vendor descriptors */
    while (size >= sizeof(struct usb_descriptor_header)) {
      header = (struct usb_descriptor_header *)buffer;

      if (header->bLength < 2) {
        if (usb_debug >= 1)
          fprintf(stderr, "invalid descriptor length of %d\n", header->bLength);
        return -1;
      }

      /* If we find another "proper" descriptor then we're done */
      if ((header->bDescriptorType == USB_DT_INTERFACE) ||
          (header->bDescriptorType == USB_DT_ENDPOINT) ||
          (header->bDescriptorType == USB_DT_CONFIG) ||
          (header->bDescriptorType == USB_DT_DEVICE))
        break;

      numskipped++;

      buffer += header->bLength;
      parsed += header->bLength;
      size -= header->bLength;
    }

    if (numskipped && usb_debug >= 2)
      fprintf(stderr, "skipped %d class/vendor specific interface descriptors\n", numskipped);

    /* Copy any unknown descriptors into a storage area for */
    /*  drivers to later parse */
    len = (int)(buffer - begin);
    if (!len) {
      ifp->extra = NULL;
      ifp->extralen = 0;
    } else {
      ifp->extra = malloc(len);
      if (!ifp->extra) {
        if (usb_debug >= 1)
          fprintf(stderr, "couldn't allocate memory for interface extra descriptors\n");
        ifp->extralen = 0;
        return -1;
      }
      memcpy(ifp->extra, begin, len);
      ifp->extralen = len;
    }

    /* Did we hit an unexpected descriptor? */
    header = (struct usb_descriptor_header *)buffer;
    if ((size >= sizeof(struct usb_descriptor_header)) &&
        ((header->bDescriptorType == USB_DT_CONFIG) ||
        (header->bDescriptorType == USB_DT_DEVICE)))
      return parsed;

    if (ifp->bNumEndpoints > USB_MAXENDPOINTS) {
      if (usb_debug >= 1)
        fprintf(stderr, "too many endpoints\n");
      return -1;
    }

    ifp->endpoint = (struct usb_endpoint_descriptor *)
                     malloc(ifp->bNumEndpoints *
                     sizeof(struct usb_endpoint_descriptor));
    if (!ifp->endpoint) {
      if (usb_debug >= 1)
        fprintf(stderr, "couldn't allocate memory for ifp->endpoint\n");
      return -1;      
    }

    memset(ifp->endpoint, 0, ifp->bNumEndpoints *
           sizeof(struct usb_endpoint_descriptor));

    for (i = 0; i < ifp->bNumEndpoints; i++) {
      header = (struct usb_descriptor_header *)buffer;

      if (header->bLength > size) {
        if (usb_debug >= 1)
          fprintf(stderr, "ran out of descriptors parsing\n");
        return -1;
      }
                
      retval = usb_parse_endpoint(ifp->endpoint + i, buffer, size);
      if (retval < 0)
        return retval;

      buffer += retval;
      parsed += retval;
      size -= retval;
    }

    /* We check to see if it's an alternate to this one */
    ifp = (struct usb_interface_descriptor *)buffer;
    if (size < USB_DT_INTERFACE_SIZE ||
        ifp->bDescriptorType != USB_DT_INTERFACE ||
        !ifp->bAlternateSetting)
      return parsed;
  }

  return parsed;
}

int usb_parse_configuration(struct usb_config_descriptor *config, char *buffer)
{
  int i, retval, size;
  struct usb_descriptor_header *header;

  memcpy(config, buffer, USB_DT_CONFIG_SIZE);
  USB_LE16_TO_CPU(config->wTotalLength);
  size = config->wTotalLength;

  if (config->bNumInterfaces > USB_MAXINTERFACES) {
    if (usb_debug >= 1)
      fprintf(stderr, "too many interfaces\n");
    return -1;
  }

  config->interface = (struct usb_interface *)
                       malloc(config->bNumInterfaces *
                       sizeof(struct usb_interface));
  if (!config->interface) {
    if (usb_debug >= 1)
      fprintf(stderr, "out of memory\n");
    return -1;      
  }

  memset(config->interface, 0, config->bNumInterfaces * sizeof(struct usb_interface));

  buffer += config->bLength;
  size -= config->bLength;
        
  config->extra = NULL;
  config->extralen = 0;

  for (i = 0; i < config->bNumInterfaces; i++) {
    int numskipped, len;
    char *begin;

    /* Skip over the rest of the Class Specific or Vendor */
    /*  Specific descriptors */
    begin = buffer;
    numskipped = 0;
    while (size >= sizeof(struct usb_descriptor_header)) {
      header = (struct usb_descriptor_header *)buffer;

      if ((header->bLength > size) || (header->bLength < 2)) {
        if (usb_debug >= 1)
          fprintf(stderr, "invalid descriptor length of %d\n", header->bLength);
        return -1;
      }

      /* If we find another "proper" descriptor then we're done */
      if ((header->bDescriptorType == USB_DT_ENDPOINT) ||
          (header->bDescriptorType == USB_DT_INTERFACE) ||
          (header->bDescriptorType == USB_DT_CONFIG) ||
          (header->bDescriptorType == USB_DT_DEVICE))
        break;

      if (usb_debug >= 2)
        fprintf(stderr, "skipping descriptor 0x%X\n", header->bDescriptorType);
      numskipped++;

      buffer += header->bLength;
      size -= header->bLength;
    }

    if (numskipped && usb_debug >= 2)
      fprintf(stderr, "skipped %d class/vendor specific endpoint descriptors\n", numskipped);

    /* Copy any unknown descriptors into a storage area for */
    /*  drivers to later parse */
    len = (int)(buffer - begin);
    if (len) {
      /* FIXME: We should realloc and append here */
      if (!config->extralen) {
        config->extra = malloc(len);
        if (!config->extra) {
          if (usb_debug >= 1)
            fprintf(stderr, "couldn't allocate memory for config extra descriptors\n");
          config->extralen = 0;
          return -1;
        }

        memcpy(config->extra, begin, len);
        config->extralen = len;
      }
    }

    retval = usb_parse_interface(config->interface + i, buffer, size);
    if (retval < 0)
      return retval;

    buffer += retval;
    size -= retval;
  }

  return size;
}

void usb_destroy_configuration(struct usb_device *dev)
{
  int c, i, j, k;
        
  if (!dev->config)
    return;

  for (c = 0; c < dev->descriptor.bNumConfigurations; c++) {
    struct usb_config_descriptor *cf = &dev->config[c];

    if (!cf->interface)
      break;

    for (i = 0; i < cf->bNumInterfaces; i++) {
      struct usb_interface *ifp = &cf->interface[i];
                                
      if (!ifp->altsetting)
        break;

      for (j = 0; j < ifp->num_altsetting; j++) {
        struct usb_interface_descriptor *as = &ifp->altsetting[j];
                                        
        if (as->extra)
          free(as->extra);

        if (!as->endpoint)
          break;
                                        
        for (k = 0; k < as->bNumEndpoints; k++) {
          if (as->endpoint[k].extra)
            free(as->endpoint[k].extra);
        }       
        free(as->endpoint);
      }

      free(ifp->altsetting);
    }

    free(cf->interface);
  }

  free(dev->config);
}

void usb_fetch_and_parse_descriptors(usb_dev_handle *udev)
{
  struct usb_device *dev = udev->device;
  int i;

  if (dev->descriptor.bNumConfigurations > USB_MAXCONFIG) {
    if (usb_debug >= 1)
      fprintf(stderr, "Too many configurations (%d > %d)\n", dev->descriptor.bNumConfigurations, USB_MAXCONFIG);
    return;
  }

  if (dev->descriptor.bNumConfigurations < 1) {
    if (usb_debug >= 1)
      fprintf(stderr, "Not enough configurations (%d < %d)\n", dev->descriptor.bNumConfigurations, 1);
    return;
  }

  dev->config = (struct usb_config_descriptor *)malloc(dev->descriptor.bNumConfigurations * sizeof(struct usb_config_descriptor));
  if (!dev->config) {
    if (usb_debug >= 1)
      fprintf(stderr, "Unable to allocate memory for config descriptor\n");
    return;
  }

  memset(dev->config, 0, dev->descriptor.bNumConfigurations *
	sizeof(struct usb_config_descriptor));

  for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {
    char buffer[8], *bigbuffer;
    struct usb_config_descriptor *desc = (struct usb_config_descriptor *)buffer;
    int res;

    /* Get the first 8 bytes so we can figure out what the total length is */
    res = usb_get_descriptor(udev, USB_DT_CONFIG, i, buffer, 8);
    if (res < 8) {
      if (usb_debug >= 1) {
        if (res < 0)
          fprintf(stderr, "Unable to get descriptor (%d)\n", res);
        else
          fprintf(stderr, "Config descriptor too short (expected %d, got %d)\n", 8, res);
      }

      goto err;
    }

    USB_LE16_TO_CPU(desc->wTotalLength);

    bigbuffer = malloc(desc->wTotalLength);
    if (!bigbuffer) {
      if (usb_debug >= 1)
        fprintf(stderr, "Unable to allocate memory for descriptors\n");
      goto err;
    }

    res = usb_get_descriptor(udev, USB_DT_CONFIG, i, bigbuffer, desc->wTotalLength);
    if (res < desc->wTotalLength) {
      if (usb_debug >= 1) {
        if (res < 0)
          fprintf(stderr, "Unable to get descriptor (%d)\n", res);
        else
          fprintf(stderr, "Config descriptor too short (expected %d, got %d)\n", desc->wTotalLength, res);
      }

      free(bigbuffer);
      goto err;
    }

    res = usb_parse_configuration(&dev->config[i], bigbuffer);
    if (usb_debug >= 2) {
      if (res > 0)
        fprintf(stderr, "Descriptor data still left\n");
      else if (res < 0)
        fprintf(stderr, "Unable to parse descriptors\n");
    }

    free(bigbuffer);
  }

  return;

err:
  free(dev->config);

  dev->config = NULL;
}

