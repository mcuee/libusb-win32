
This folder contains batch files to compile libusb-win32 with Microsoft's DDK.

Requirements:

1) Use WinDDK 6001.18002 for Windows 2000 to Windows 7 compatibility.
   (NOTE: WinDDK 6001.18002 is that last to support Windows 2000)

How to compile:

1) install the DDK
2) open a DDK build environment, either a "checked" or a "free" one
3) launch one of the following batch files to compile the sources:
    
    make_driver.bat: builds the driver
    make_dll.bat: builds the DLL
    make_install_filter.bat: builds install-filter.exe utility
    make_test.bat: builds the command line version of the test tool
    make_test_win.bat: builds the Windows version of the test tool
    make_all.bat: builds everything, driver, DLL, and the test tools