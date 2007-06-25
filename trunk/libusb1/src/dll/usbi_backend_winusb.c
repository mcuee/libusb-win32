#include "usbi_backend_winusb.h"
#include "dll_load.h"
#include "registry.h"

#include <string.h>
#include <rpcdce.h>


/* winusb.dll interface */

#define SHORT_PACKET_TERMINATE  0x01
#define AUTO_CLEAR_STALL        0x02
#define PIPE_TRANSFER_TIMEOUT   0x03
#define IGNORE_SHORT_PACKETS    0x04
#define ALLOW_PARTIAL_READS     0x05
#define AUTO_FLUSH              0x06
#define RAW_IO                  0x07
#define MAXIMUM_TRANSFER_SIZE   0x08
#define AUTO_SUSPEND            0x81
#define SUSPEND_DELAY           0x83
#define DEVICE_SPEED            0x01
#define LowSpeed                0x01
#define FullSpeed               0x02
#define HighSpeed               0x03 

typedef enum _USBD_PIPE_TYPE {
	UsbdPipeTypeControl,
	UsbdPipeTypeIsochronous,
	UsbdPipeTypeBulk,
	UsbdPipeTypeInterrupt
} USBD_PIPE_TYPE;

typedef struct {
  USBD_PIPE_TYPE PipeType;
  UCHAR          PipeId;
  USHORT         MaximumPacketSize;
  UCHAR          Interval;
} WINUSB_PIPE_INFORMATION, *PWINUSB_PIPE_INFORMATION;

#pragma pack(1)
typedef struct {
  UCHAR  request_type;
  UCHAR  request;
  USHORT value;
  USHORT index;
  USHORT length;
} WINUSB_SETUP_PACKET, *PWINUSB_SETUP_PACKET;
#pragma pack()

typedef void *WINUSB_INTERFACE_HANDLE, *PWINUSB_INTERFACE_HANDLE;
typedef void *PUSB_INTERFACE_DESCRIPTOR;

DLL_DECLARE(WINAPI, BOOL, WinUsb_Initialize, 
            (HANDLE, PWINUSB_INTERFACE_HANDLE));
DLL_DECLARE(WINAPI, BOOL, WinUsb_Free, (WINUSB_INTERFACE_HANDLE));
DLL_DECLARE(WINAPI, BOOL, WinUsb_GetAssociatedInterface, 
            (WINUSB_INTERFACE_HANDLE, UCHAR, PWINUSB_INTERFACE_HANDLE));
DLL_DECLARE(WINAPI, BOOL, WinUsb_GetDescriptor, 
            (WINUSB_INTERFACE_HANDLE, UCHAR, UCHAR, USHORT, PUCHAR,
             ULONG, PULONG));
DLL_DECLARE(WINAPI, BOOL, WinUsb_QueryInterfaceSettings,
            (WINUSB_INTERFACE_HANDLE, UCHAR, PUSB_INTERFACE_DESCRIPTOR));
DLL_DECLARE(WINAPI, BOOL, WinUsb_QueryDeviceInformation, 
            (WINUSB_INTERFACE_HANDLE, ULONG, PULONG, PVOID));
DLL_DECLARE(WINAPI, BOOL, WinUsb_SetCurrentAlternateSetting, 
            (WINUSB_INTERFACE_HANDLE, UCHAR));
DLL_DECLARE(WINAPI, BOOL, WinUsb_GetCurrentAlternateSetting, 
            (WINUSB_INTERFACE_HANDLE, PUCHAR));
DLL_DECLARE(WINAPI, BOOL, WinUsb_QueryPipe, 
            (WINUSB_INTERFACE_HANDLE, UCHAR, UCHAR,
             PWINUSB_PIPE_INFORMATION));
DLL_DECLARE(WINAPI, BOOL, WinUsb_SetPipePolicy, 
            (WINUSB_INTERFACE_HANDLE, UCHAR, ULONG, ULONG, PVOID));
DLL_DECLARE(WINAPI, BOOL, WinUsb_GetPipePolicy, 
            (WINUSB_INTERFACE_HANDLE, UCHAR, ULONG, PULONG, PVOID));
DLL_DECLARE(WINAPI, BOOL, WinUsb_ReadPipe, 
            (WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PULONG,
             LPOVERLAPPED));
DLL_DECLARE(WINAPI, BOOL, WinUsb_WritePipe,
            (WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PULONG, 
             LPOVERLAPPED));
DLL_DECLARE(WINAPI, BOOL, WinUsb_ControlTransfer,
            (WINUSB_INTERFACE_HANDLE, WINUSB_SETUP_PACKET, PUCHAR, ULONG, 
             PULONG, LPOVERLAPPED));
DLL_DECLARE(WINAPI, BOOL, WinUsb_ResetPipe, 
            (WINUSB_INTERFACE_HANDLE, UCHAR));
DLL_DECLARE(WINAPI, BOOL, WinUsb_AbortPipe, 
            (WINUSB_INTERFACE_HANDLE, UCHAR));
DLL_DECLARE(WINAPI, BOOL, WinUsb_FlushPipe, 
            (WINUSB_INTERFACE_HANDLE, UCHAR));


/* helper functions */
static void _winusb_update_pipe_info(winusb_device_t dev);
static void _winusb_free_pipe_info(winusb_device_t dev);
static int _winusb_interface_to_index(winusb_device_t dev, int interface);
static void *_winusb_interface_by_endpoint(winusb_device_t dev, int endpoint);


static void _winusb_update_pipe_info(winusb_device_t dev)
{
  int i, e;
  WINUSB_PIPE_INFORMATION pipe_info;
  usbi_interface_descriptor_t idesc;
  UCHAR policy;

  _winusb_free_pipe_info(dev);

  if(!WinUsb_Initialize(dev->wdev, &dev->interfaces[0].handle))
    return;
  
  for(i = 1; i < WINUSB_MAX_INTERFACES; i++) {
    if(!WinUsb_GetAssociatedInterface(dev->interfaces[0].handle,
                                      i - 1, &dev->interfaces[i].handle))
      break;
  }
  
  for(i = 0; i < WINUSB_MAX_INTERFACES; i++) {
    if((dev->interfaces[i].handle)) {
      if(WinUsb_QueryInterfaceSettings(dev->interfaces[i].handle, 0, &idesc))
        dev->interfaces[i].number = idesc.bInterfaceNumber;
    }
  }
  
  for(i = 0; i < WINUSB_MAX_INTERFACES; i++) {
    if(dev->interfaces[i].handle) {
      for(e = 0; e < WINUSB_MAX_ENDPOINTS; e++) {
        if(!WinUsb_QueryPipe(dev->interfaces[i].handle, 0, e, 
                             &pipe_info))
          break;
        dev->interfaces[i].endpoints[e].address = pipe_info.PipeId;
        policy = FALSE;
        WinUsb_SetPipePolicy(dev->interfaces[i].handle, pipe_info.PipeId,
                             SHORT_PACKET_TERMINATE, sizeof(UCHAR), &policy);
        WinUsb_SetPipePolicy(dev->interfaces[i].handle, pipe_info.PipeId,
                             IGNORE_SHORT_PACKETS, sizeof(UCHAR), &policy);
        WinUsb_SetPipePolicy(dev->interfaces[i].handle, pipe_info.PipeId,
                             ALLOW_PARTIAL_READS, sizeof(UCHAR), &policy);
        policy = TRUE;
        WinUsb_SetPipePolicy(dev->interfaces[i].handle, pipe_info.PipeId,
                             AUTO_CLEAR_STALL, sizeof(UCHAR), &policy);
      }
    }
  }
}

static void _winusb_free_pipe_info(winusb_device_t dev)
{
  int i;

  for(i = 0; i < WINUSB_MAX_INTERFACES; i++) {
    if(dev->interfaces[i].handle)
      WinUsb_Free(dev->interfaces[i].handle);
  }
  memset(dev->interfaces, 0, sizeof(dev->interfaces));
  for(i = 0; i < WINUSB_MAX_INTERFACES; i++)
    dev->interfaces[i].number = -1;
}

static int _winusb_interface_to_index(winusb_device_t dev, int interface)
{
  int i;

  for(i = 0; i < WINUSB_MAX_INTERFACES; i++) {
    if(dev->interfaces[i].handle && dev->interfaces[i].number == interface)
      return i;
  }

  return -1;
}

static void *_winusb_interface_by_endpoint(winusb_device_t dev, int endpoint)
{
  int i, e;

  if(!endpoint)
    return dev->interfaces[0].handle;

  for(i = 0; i < WINUSB_MAX_INTERFACES; i++) {
    if(dev->interfaces[i].handle) {
      for(e = 0; e < WINUSB_MAX_ENDPOINTS; e++) {
        if(dev->interfaces[i].endpoints[e].address == endpoint)
          return dev->interfaces[i].handle;
      }
    }
  }
  return NULL;
}

int winusb_init(void)
{
  /* load winusb.dll */
  DLL_LOAD(winusb.dll, WinUsb_Initialize, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_Free, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_GetAssociatedInterface, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_GetDescriptor, TRUE); 
  DLL_LOAD(winusb.dll, WinUsb_QueryInterfaceSettings, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_QueryDeviceInformation, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_SetCurrentAlternateSetting, TRUE); 
  DLL_LOAD(winusb.dll, WinUsb_GetCurrentAlternateSetting, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_QueryPipe, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_SetPipePolicy, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_GetPipePolicy, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_ReadPipe, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_WritePipe, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_ControlTransfer, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_ResetPipe, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_AbortPipe, TRUE);
  DLL_LOAD(winusb.dll, WinUsb_FlushPipe, TRUE);

  return USBI_STATUS_SUCCESS;
}

int winusb_deinit(void)
{
  return USBI_STATUS_SUCCESS;
}

int winusb_set_debug(usbi_debug_level_t level)
{
  return USBI_STATUS_SUCCESS;
}

int winusb_get_name(int index, char *name, int size)
{
  usb_registry_device_t reg_dev;
  GUID guid;
  int i = 0, j;
  struct {
    GUID guid;
    int count;
  } guids[128];

  memset(guids, 0, sizeof(guids));

  while(usb_registry_get_device(&reg_dev, i++, TRUE)) {
    if(strlen(reg_dev.winusb_guid) && !strcmp(reg_dev.driver, "winusb")) {
      char *p = reg_dev.winusb_guid;
      while(*p) {
        if(*p == '}') {
          *p = 0;
          break;
        }
        p++;
      }
      if(UuidFromString(reg_dev.winusb_guid + 1, &guid) == RPC_S_OK) {
        for(j = 0; j < sizeof(guids)/sizeof(guids[0]); j++) {
          if(guids[j].count) {
            if(!memcmp(&guids[j].guid, &guid, sizeof(GUID))) {
              guids[j].count++;
              break;
            }
          } else {
            guids[j].count = 1;
            memcpy(&guids[j].guid, &guid, sizeof(GUID));
            break;
          }
        }
      }
    }
  }

  for(j = 0; (j < sizeof(guids)/sizeof(guids[0])) && guids[j].count; j++) {
    if(index < guids[j].count)
      return winio_name_by_guid(&guids[j].guid, index, name, size);
    else
      index -= guids[j].count;
  }
  return USBI_STATUS_NODEV;
}

int winusb_open(winusb_device_t dev, const char *name)
{
  int ret;
  ret = winio_open_name(&dev->wdev, name);
  if(ret >= 0)
    _winusb_update_pipe_info(dev);
  return ret;
}

int winusb_close(winusb_device_t dev)
{
  int ret;
  _winusb_free_pipe_info(dev);
  ret = winio_close(dev->wdev);
  return ret;
}

int winusb_reset(winusb_device_t dev)
{
  return USBI_STATUS_SUCCESS;
}

int winusb_reset_endpoint(winusb_device_t dev, int endpoint)
{
  void *interface;

  if(!(interface = _winusb_interface_by_endpoint(dev, endpoint)))
    return USBI_STATUS_PARAM;
  return WinUsb_ResetPipe(interface, (UCHAR)endpoint) ?
    USBI_STATUS_SUCCESS : USBI_STATUS_UNKNOWN;
}

int winusb_set_configuration(winusb_device_t dev, int value)
{
  int ret;

  ret = usbi_control_msg_sync((usbi_device_t)dev, USBI_DIRECTION_OUT 
                              | USBI_TYPE_STANDARD | USBI_RECIP_DEVICE, 
                              USBI_REQ_SET_CONFIGURATION, 
                              value, 0, NULL, 0, 1000); 
  if(ret >= 0)
    _winusb_update_pipe_info(dev);

  return ret;
}

int winusb_set_interface(winusb_device_t dev, int interface, int altsetting)
{
  int i;

  if(interface >= WINUSB_MAX_INTERFACES)
    return USBI_STATUS_PARAM;
  if((i = _winusb_interface_to_index(dev, interface)) < 0)
    return USBI_STATUS_PARAM;
  if(!WinUsb_SetCurrentAlternateSetting(dev->interfaces[i].handle, 
                                        (UCHAR)altsetting))
    return USBI_STATUS_UNKNOWN;

  return USBI_STATUS_SUCCESS;
}

int winusb_claim_interface(winusb_device_t dev, int interface)
{
  if(_winusb_interface_to_index(dev, interface) >= 0)
    return usbi_claim_interface_simple((usbi_device_t)dev, interface);
  return USBI_STATUS_PARAM;
}

int winusb_release_interface(winusb_device_t dev, int interface)
{
  if(_winusb_interface_to_index(dev, interface) >= 0)
    return usbi_release_interface_simple((usbi_device_t)dev, interface);
  return USBI_STATUS_PARAM;
}

int winusb_control_msg(winusb_device_t dev, int request_type, int request, 
                       int value, int index, void *data, int size, 
                       winusb_io_t io)
{
  ULONG junk;
  WINUSB_SETUP_PACKET sp;
  
  sp.request_type = (UCHAR)request_type;
  sp.request = (UCHAR)request;
  sp.value = (USHORT)value;
  sp.index = (USHORT)index;
  sp.length = (USHORT)size;

  if(!(io->wio = winio_create_io()))
    return USBI_STATUS_NOMEM;

  if(!WinUsb_ControlTransfer(dev->interfaces[0].handle,
                             sp, data, size, &junk, io->wio)) {
    if(GetLastError() != ERROR_IO_PENDING) {
      winio_free_io(io->wio);
      USBI_DEBUG_ERROR("WinUsb_ControlTransfer() failed");
      return USBI_STATUS_UNKNOWN;
    }
  }
  return USBI_STATUS_SUCCESS;
}

int winusb_transfer(winusb_device_t dev, int endpoint, usbi_transfer_t type,
                    void *data, int size, int packet_size, winusb_io_t io)
{
  void *i;
  ULONG junk;
  BOOL ret;

  if(!(i = _winusb_interface_by_endpoint(dev, endpoint)))
    return USBI_STATUS_PARAM;
  if(!(io->wio = winio_create_io()))
    return USBI_STATUS_NOMEM;
  if(USBI_ENDPOINT_IN(endpoint))
    ret = WinUsb_ReadPipe(i, endpoint, data, size, &junk, io->wio);
  else
    ret = WinUsb_WritePipe(i, endpoint, data, size, &junk, io->wio);
  if(!ret && GetLastError() != ERROR_IO_PENDING) {
    winio_free_io(io->wio);
    return USBI_STATUS_UNKNOWN;
  }
  return USBI_STATUS_SUCCESS;
}

int winusb_wait(winusb_device_t dev, winusb_io_t io, int timeout)
{
  return winio_wait(dev->wdev, io->wio, timeout);
}

int winusb_poll(winusb_device_t dev, winusb_io_t io)
{
  return winio_poll(dev->wdev, io->wio);
}

int winusb_cancel(winusb_device_t dev, winusb_io_t io)
{
  void *i;

  i = _winusb_interface_by_endpoint(dev, io->base.endpoint);
  if(!i)
    return USBI_STATUS_PARAM;
  WinUsb_AbortPipe(i, io->base.endpoint);
  winusb_wait(dev, io, INFINITE);
  return USBI_STATUS_SUCCESS;
}




