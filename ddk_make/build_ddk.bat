@ECHO OFF
:: Use this batch file instead of the winddk "build" command.
:: 
:: - Calls the winddk build command. Sets BUILD_ERRORLEVEL=1 if 
::   a build error is detected.
:: - Sets LOG_APPNAME define (unless already set)  
:: - Sets libusb-win32 version defines (unless already set)  
:: 

IF "%LOG_APPNAME%"=="" SET LOG_APPNAME=$(TARGETNAME)
SET COMMON_C_DEFINES=
SET COMMON_C_DEFINES=%COMMON_C_DEFINES% /DLOG_APPNAME="\"$(LOG_APPNAME)\""
IF DEFINED CMDVAR_LOG_DIRECTORY SET COMMON_C_DEFINES=%COMMON_C_DEFINES% /DLOG_DIRECTORY="\"$(CMDVAR_LOG_DIRECTORY)\""
SET COMMON_C_DEFINES=%COMMON_C_DEFINES% %*

IF EXIST "build%BUILD_ALT_DIR%.err" DEL /Q "build%BUILD_ALT_DIR%.err" >NUL
IF EXIST "build%BUILD_ALT_DIR%.wrn" DEL /Q "build%BUILD_ALT_DIR%.wrn" >NUL
SET BUILD_ERRORLEVEL=0

if exist libusb0.lib move /Y libusb0.lib libusb.lib >NUL

build -cwgZ 2>NUL>NUL
IF EXIST "build%BUILD_ALT_DIR%.err" TYPE "build%BUILD_ALT_DIR%.err"
IF EXIST "build%BUILD_ALT_DIR%.wrn" TYPE "build%BUILD_ALT_DIR%.wrn"
IF EXIST "build%BUILD_ALT_DIR%.err" SET BUILD_ERRORLEVEL=1
IF EXIST "build%BUILD_ALT_DIR%.err" SET ERRORLEVEL=1

