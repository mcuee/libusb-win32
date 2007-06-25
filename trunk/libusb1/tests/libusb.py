import ctypes
import sys

if sys.platform.startswith('win'):
    packed_structures = True
    _libusb = ctypes.cdll.LoadLibrary('libusb0.dll')
    LIBUSB_PATH_MAX = 512
elif sys.platform.startswith('linux'):
    packed_structures = False
    _libusb = ctypes.cdll.LoadLibrary('libusb.so')
    LIBUSB_PATH_MAX = 4096

else:
    pass


class DeviceDescriptor(ctypes.Structure):
    _pack_ = packed_structures
    _fields_ = [('bLength', ctypes.c_ubyte),
                ('bDescriptorType', ctypes.c_ubyte),
                ('bcdUSB', ctypes.c_ushort),
                ('bDeviceClass', ctypes.c_ubyte),
                ('bDeviceSubClass', ctypes.c_ubyte),
                ('bDeviceProtocol', ctypes.c_ubyte),
                ('bMaxPacketSize0', ctypes.c_ubyte),
                ('idVendor', ctypes.c_ushort),
                ('idProduct', ctypes.c_ushort),
                ('bcdDevice', ctypes.c_ushort),
                ('iManufacturer', ctypes.c_ubyte),
                ('iProduct', ctypes.c_ubyte),
                ('iSerialNumber', ctypes.c_ubyte),
                ('bNumConfigurations', ctypes.c_ubyte)]
    
class ConfigDescriptor(ctypes.Structure):
    _pack_ = packed_structures
    _fields_ = [('bLength', ctypes.c_ubyte),
                ('bDescriptorType', ctypes.c_ubyte),
                ('wTotalLength', ctypes.c_ushort),
                ('bNumInterfaces', ctypes.c_ubyte),
                ('bConfigurationValue', ctypes.c_ubyte),
                ('iConfiguration', ctypes.c_ubyte),
                ('bmAttributes', ctypes.c_ubyte),
                ('MaxPower', ctypes.c_ubyte),
                ('interface',usb_interface_p),
                ('extra',ctypes.POINTER(uint8)),
                ('extralen',ctypes.c_int)]
    
class InterfaceDescriptor(ctypes.Structure):
    _pack_ = packed_structures
    _fields_ = [('bLength', ctypes.c_ubyte),
                ('bDescriptorType', ctypes.c_ubyte),
                ('bInterfaceNumber', ctypes.c_ubyte),
                ('bAlternateSetting', ctypes.c_ubyte),
                ('bNumEndpoints', ctypes.c_ubyte),
                ('bInterfaceClass', ctypes.c_ubyte),
                ('bInterfaceSubClass', ctypes.c_ubyte),
                ('bInterfaceProtocol', ctypes.c_ubyte),
                ('iInterface', ctypes.c_ubyte),
                ('endpoint',usb_endpoint_descriptor_p),
                ('extra',ctypes.POINTER(uint8)),
                ('extralen',ctypes.c_int)]

class EndpointDescriptor(ctypes.Structure):
    _pack_ = packed_structures
    _fields_ = [('bLength', ctypes.c_ubyte),
                ('bDescriptorType', ctypes.c_ubyte),
                ('bEndpointAddress', ctypes.c_ubyte),
                ('bmAttributes', ctypes.c_ubyte),
                ('wMaxPacketSize', ctypes.c_ushort),
                ('bInterval', ctypes.c_ubyte),
                ('bRefresh', ctypes.c_ubyte),
                ('bSynchAddress', ctypes.c_ubyte),
                ('extra',ctypes.c_char_p),
                ('extralen',ctypes.c_int)]
    
class Device(ctypes.Structure):
    _pack_ = packed_structures
    _fields_ = [('next',usb_device_p),
                ('prev',usb_device_p),
                ('filename',ctypes.c_char*(LIBUSB_PATH_MAX)),
                ('bus',usb_bus_p),
                ('descriptor',usb_device_descriptor),
                ('config',usb_config_descriptor_p),
                ('dev',ctypes.c_void_p),
                ('devnum', ctypes.c_ubyte),
                ('num_children', ctypes.c_ubyte),
                ('children',ctypes.POINTER(usb_device_p))]
class Bus(ctypes.Structure):
    _pack_ = packed_structures
    _fields_ = [('next',usb_bus_p),
                ('prev',usb_bus_p),
                ('dirname',ctypes.c_char*(LIBUSB_PATH_MAX)),
                ('devices',usb_device_p),
                ('location',ctypes.c_ulong),
                ('root_dev',usb_device_p)]

class Interface(ctypes.Structure):
    _pack_ = packed_structures
    _fields_ = [('altsetting',usb_interface_descriptor_p),
                ('num_altsetting',ctypes.c_int)]

class DevHandle(ctypes.Structure):
    _pack_ = packed_structures


_libusb.usb_get_busses.restype = ctypes.POINTER(Bus)

_libusb.usb_open.restype = ctypes.POINTER(DeviceHandle)
_libusb.usb_open.argtypes = [ctypes.POINTER(Device)]

_libusb.usb_close.argtypes = [ctypes.POINTER(DeviceHandle)]

_libusb.usb_claim_interface.argtypes = [ctypes.POINTER(DeviceHandle), \
                                        ctypes.c_int]
_libusb.usb_claim_interface.restype = ctypes.c_int

_libusb.usb_release_interface.restype = ctypes.c_int
_libusb.usb_release_interface.argtypes = [ctypes.POINTER(DeviceHandle), \
                                          ctypes.c_int]


_libusb.usb_reset.restype = ctypes.c_int
_libusb.usb_reset.argtypes = [ctypes.POINTER(DeviceHandle)]

_libusb.usb_control_msg.restype = ctypes.c_int

_libusb.usb_bulk_read.restype = ctypes.c_int

_libusb.usb_bulk_write.restype = ctypes.c_int

_libusb.usb_set_configuration.restype = ctypes.c_int
_libusb.usb_set_configuration.argtypes = [ctypes.POINTER(DeviceHandle), \
                                          ctypes.c_int]

def busses():
