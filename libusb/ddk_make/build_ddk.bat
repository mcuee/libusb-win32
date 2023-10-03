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

SET BUILD_ERRORLEVEL=0
SET OUTDIR=build\%1\%2\%3\
SET INTDIR=build\%2\%3\

MSBuild ../projects/vs2019/libusb-win32.sln -m -t:%1 -p:IntDir=%~dp0%OUTDIR%;OutDir=%~dp0%INTDIR%;Configuration=%3;Platform=%2;BuildProjectReferences=false
IF %ERRORLEVEL% NEQ 0 SET BUILD_ERRORLEVEL=1
