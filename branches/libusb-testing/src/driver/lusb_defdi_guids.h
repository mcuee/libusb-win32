#ifndef __LUSB_DEFDI_GUIDS_
#define __LUSB_DEFDI_GUIDS_

#ifndef DEFINE_TO_STR
#define _DEFINE_TO_STR(x) #x
#define  DEFINE_TO_STR(x) _DEFINE_TO_STR(x)
#endif

#ifndef DEFINE_TO_STRW
#define _DEFINE_TO_STRW(x) L#x
#define  DEFINE_TO_STRW(x) _DEFINE_TO_STRW(x)
#endif

#define DEFINE_DEVICE_INTERFACE_GUID(BaseFieldName,BaseGuidName,L1,W1,W2,B1,B2,B3,B4,B5,B6,B7,B8)	\
	const GUID   BaseFieldName##Guid  = { L1, W1, W2, { B1, B2, B3, B4, B5, B6, B7, B8 } };			\
	const CHAR*  BaseFieldName##GuidA = DEFINE_TO_STR(BaseGuidName);								\
	const WCHAR* BaseFieldName##GuidW = DEFINE_TO_STRW(BaseGuidName)

#define _DefRawDeviceGuid {A5DCBF10-6530-11D2-901F-00C04FB951ED}
#define _DefLibusb0FilterGuid {F9F3FF14-AE21-48A0-8A25-8011A7A931D9}
#define _DefLibusb0DeviceGuid {20343A29-6DA1-4DB8-8A3C-16E774057BF5}
#define _DefLibusbKDeviceGuid {6C696275-7362-2D77-696E-33322D574446}

// USB Raw Device Interface GUID (DO NOT USE IN INF FILE)
DEFINE_DEVICE_INTERFACE_GUID(RawDevice, \
                             _DefRawDeviceGuid, \
                             0xA5DCBF10, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

// libusb0 Filter Device Interface GUID (DO NOT USE IN INF FILE)
DEFINE_DEVICE_INTERFACE_GUID(Libusb0Filter,
                             _DefLibusb0FilterGuid, \
                             0xF9F3FF14, 0xAE21, 0x48A0, 0x8A, 0x25, 0x80, 0x11, 0xA7, 0xA9, 0x31, 0xD9);

// libusb0 Default Device Interface GUID (DO NOT USE IN INF FILE)
DEFINE_DEVICE_INTERFACE_GUID(Libusb0Device,
                             _DefLibusb0DeviceGuid, \
                             0x20343A29, 0x6DA1, 0x4DB8, 0x8A, 0x3C, 0x16, 0xE7, 0x74, 0x05, 0x7B, 0xF5);

// libusbK Default Device Interface GUID (DO NOT USE IN INF FILE)
DEFINE_DEVICE_INTERFACE_GUID(LibusbKDevice,
                             _DefLibusbKDeviceGuid, \
                             0x6C696275, 0x7362, 0x2D77, 0x69, 0x6E, 0x33, 0x32, 0x2D, 0x57, 0x44, 0x46);


#endif
