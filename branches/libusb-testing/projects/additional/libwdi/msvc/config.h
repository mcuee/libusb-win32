/* config.h.  Manual config for MSVC.  */

#ifdef _MSC_VER

/* embed WinUSB driver files from the following DDK location */
#ifndef DDK_DIR
// #define DDK_DIR "E:/WinDDK/7600.16385.0"
#endif

/* embed libusb0 driver files from the following location */
#ifndef LIBUSB0_DIR
// #define LIBUSB0_DIR "D:/libusb-win32"
#endif

/* embed user defined driver files from the following location */
#if !defined(USER_DIR) && !defined(LIBUSB0_DIR) && !defined(DDK_DIR)
	#define USER_DIR ""
#endif

/* DDK WDF coinstaller version (string) */
#define WDF_VER "01009"

/* 32 bit support */
//#define OPT_M32

/* 64 bit support */
//#define OPT_M64

/* embed IA64 driver files */
//#define OPT_IA64

#else // end of MSVC defaults


#endif

// MSVC and GCC defaults
#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
	/* Debug message logging */
	#define ENABLE_DEBUG_LOGGING

	/* Debug message logging (toggable) */
	#define INCLUDE_DEBUG_LOGGING

	/* Message logging */
	#define ENABLE_LOGGING 1

	/* Output log message to a debug window/DebugView */
	#define LOG_OUTPUT_DEBUGWINDOW
#endif
