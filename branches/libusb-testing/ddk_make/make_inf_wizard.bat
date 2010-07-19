@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

SET TESTS_DIR=..\tests
SET SRC_DIR=..\src
call make_clean.bat

:: Check arguments
::
SET _ARGS_=%*
IF "!LIBWDI_DIR!" EQU "" (
	IF "%~1" EQU "" (
		ECHO The libwdi directory must be passed as the first argument or the LIBWDI_DIR
		ECHO environment variable must be set.
		ECHO Example:
		ECHO make_inf_wizard.bat "Z:\PROJECTS\libusb-win32-stage\projects\additional\libwdi"
		GOTO :SHOW_LIBWDI_HELP
	) ELSE (
		SET LIBWDI_DIR=%~1
		SET _ARGS_=%~2
	)
)

:: Check for libwdi ddk_build.cmd
::
IF NOT EXIST "!LIBWDI_DIR!\ddk_build.cmd" (
	ECHO libwdi ddk_build.cmd not found at '!LIBWDI_DIR!'
	GOTO :SHOW_LIBWDI_HELP
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
	SET C_DEFINES=/DUSER_DIR=\"\"
)
CALL ddk_build.cmd no_samples

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
COPY /Y sources_inf_wizard sources >NUL
COPY /Y %SRC_DIR%\inf_wizard*.* >NUL
COPY /Y %SRC_DIR%\libusb_version.h >NUL
COPY /Y %SRC_DIR%\common*.* >NUL

ECHO Building (%BUILD_ALT_DIR%) %0..
CALL build_ddk.bat !_ARGS_!
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%BUILD_ALT_DIR%)
EXIT /B 1
GOTO BUILD_DONE

:SHOW_LIBWDI_HELP
GOTO BUILD_DONE

:BUILD_SUCCESS
GOTO BUILD_DONE

:BUILD_DONE
