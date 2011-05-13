/* config.h.  Manual config for MSVC.  */

#ifndef _MSC_VER
#warn "msvc/config.h shouldn't be included for your development environment."
#error "Please make sure the msvc/ directory is removed from your build path."
#endif

/* embed WinUSB driver files from the following DDK location */
#ifndef DDK_DIR
//#define DDK_DIR "Z:/WinDDK/7600.16385.0"
#endif

/* embed libusb0 driver files from the following location */
#ifndef LIBUSB0_DIR
#define LIBUSB0_DIR "Z:/packages/libusb-win32"
#endif

/* embed libusbK driver files from the following location */
#ifndef LIBUSBK_DIR
// #define LIBUSBK_DIR "D:/libusbK/fre"
#endif

/* embed user defined driver files from the following location */
#ifndef USER_DIR
// #define USER_DIR "C:/signed-driver"
#endif

/* DDK WDF coinstaller version (string) */
#define WDF_VER "01009"

/* 32 bit support */
#ifndef OPT_M32
#define OPT_M32
#endif

/* 64 bit support */
// #define OPT_M64

/* embed IA64 driver files */
//#define OPT_IA64

/* Debug message logging */
#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
	/* Debug message logging */
	#define ENABLE_DEBUG_LOGGING

	/* Debug message logging (toggable) */
	#define INCLUDE_DEBUG_LOGGING

	/* Message logging */
	#define ENABLE_LOGGING 1
#endif

/* Output log message to a debug window/DebugView */
#define LOG_OUTPUT_DEBUGWINDOW