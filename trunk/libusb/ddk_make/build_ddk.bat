@ECHO OFF
:: Use this batch file instead of the winddk "build" command.
:: 
:: - Calls the winddk build command. Sets ERRORLEVEL=1 if 
::   a build error is detected.
:: - Sets libusb-win32 version defines (unless already set)  
:: 

IF "%VERSION_MAJOR%"=="" SET VERSION_MAJOR=1
IF "%VERSION_MINOR%"=="" SET VERSION_MINOR=1
IF "%VERSION_MICRO%"=="" SET VERSION_MICRO=14
IF "%VERSION_NANO%"=="" SET VERSION_NANO=0

IF EXIST "build%BUILD_ALT_DIR%.err" DEL /Q "build%BUILD_ALT_DIR%.err" >NUL
IF EXIST "build%BUILD_ALT_DIR%.wrn" DEL /Q "build%BUILD_ALT_DIR%.wrn" >NUL
SET ERRORLEVEL=0

build %1 %2 %3 %4 %5 %6 %7 %8 %9 2>NUL>NUL
IF EXIST "build%BUILD_ALT_DIR%.err" TYPE "build%BUILD_ALT_DIR%.err"
IF EXIST "build%BUILD_ALT_DIR%.wrn" TYPE "build%BUILD_ALT_DIR%.wrn"
IF EXIST "build%BUILD_ALT_DIR%.err" SET ERRORLEVEL=1

