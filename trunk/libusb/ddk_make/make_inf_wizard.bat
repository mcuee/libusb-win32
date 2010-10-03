@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

SET TESTS_DIR=..\tests
SET SRC_DIR=..\src
call make_clean.bat

:: Check arguments
::

IF "!LIBWDI_DIR!" EQU "" SET LIBWDI_DIR=..\projects\additional\libwdi\

:: Check for libwdi ddk_build.cmd
::
IF NOT EXIST "!LIBWDI_DIR!\ddk_build.cmd" (
	ECHO libwdi ddk_build.cmd not found at '!LIBWDI_DIR!'
	GOTO SHOW_LIBWDI_HELP
)

:: Build libwdi
::
SET BUILD_ERRORLEVEL=0
SET _CD_=!CD!
PUSHD !_CD_!

CD /D "!LIBWDI_DIR!"
IF EXIST "build%BUILD_ALT_DIR%.err" DEL /Q "build%BUILD_ALT_DIR%.err" >NUL
IF EXIST "build%BUILD_ALT_DIR%.wrn" DEL /Q "build%BUILD_ALT_DIR%.wrn" >NUL

IF EXIST "!LIBUSB0_DIR!" (
	SET C_DEFINES=/DLIBUSB0_DIR=\"!LIBUSB0_DIR!\" /DOPT_M32 /DOPT_M64 /DOPT_IA64
) ELSE (
	ECHO.
	ECHO [Warning] The LIBUSB0_DIR environment variable has not been set. This 
	ECHO           inf-wizard will contain only the inf generator and not
	ECHO           embedded libusb-win32 binaries. 
	ECHO.
	ECHO '!LIBUSB0_DIR!'
	SET C_DEFINES=/DUSER_DIR=\"\" /DOPT_M32 /DOPT_M64
)
ECHO Building (%BUILD_ALT_DIR%) libwdi..
CALL ddk_build.cmd no_samples 2>NUL

IF EXIST "build%BUILD_ALT_DIR%.err" SET BUILD_ERRORLEVEL=1
IF EXIST "build%BUILD_ALT_DIR%.err" SET ERRORLEVEL=1
IF !BUILD_ERRORLEVEL! NEQ 0 (

	ECHO Failed building libwdi.
	GOTO BUILD_ERROR
)

POPD

::
:: Copy in the inf-wizard sources
COPY /Y "!LIBWDI_DIR!\libwdi\libwdi.lib" >NUL
COPY /Y "!LIBWDI_DIR!\libwdi\libwdi.h" >NUL
COPY /Y "!LIBWDI_DIR!\libwdi\msapi_utf8.h" >NUL
COPY /Y sources_inf_wizard sources >NUL
COPY /Y %SRC_DIR%\inf_wizard*.* >NUL
COPY /Y %SRC_DIR%\libusb-win32_version.* >NUL
copy %SRC_DIR%\*.manifest . >NUL

ECHO Building (%BUILD_ALT_DIR%) %0..
CALL build_ddk.bat !_ARGS_!
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%BUILD_ALT_DIR%)
EXIT /B 1
GOTO BUILD_DONE

:SHOW_LIBWDI_HELP
ECHO.
ECHO inf-wizard-libusb-win32 WinDDK build utility
ECHO.
ECHO Summary: This batch script automates the inf-wizard WinDDK build process
ECHO          and creates inf-wizard with embedded binaries.
ECHO.
ECHO NOTE : This batch script must be run from a x86 windkk build environment.
ECHO.
ECHO USAGE EXAMPLE:
ECHO
ECHO example #1.
ECHO SET LIBUSB0_DIR=Z:\packages\libusb-win32\
ECHO make_inf_wizard.bat
ECHO.
GOTO BUILD_DONE

:BUILD_SUCCESS
GOTO BUILD_DONE

:BUILD_DONE
