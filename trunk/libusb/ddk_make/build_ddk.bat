@ECHO OFF
:: Use this batch file instead of the winddk "build" command.
:: 
:: - Calls the winddk build command. Sets ERRORLEVEL=1 if 
::   a build error is detected.
:: - Sets libusb-win32 version defines (unless already set)  
:: 

IF "%VERSION_MAJOR%"=="" SET VERSION_MAJOR=0
IF "%VERSION_MINOR%"=="" SET VERSION_MINOR=1
IF "%VERSION_MICRO%"=="" SET VERSION_MICRO=12
IF "%VERSION_NANO%"=="" SET VERSION_NANO=2

IF EXIST "build%BUILD_ALT_DIR%.err" DEL /Q "build%BUILD_ALT_DIR%.err"
SET ERRORLEVEL=0
build %1 %2 %3 %4 %5 %6 %7 %8 %9
IF EXIST "build%BUILD_ALT_DIR%.err" SET ERRORLEVEL=1

