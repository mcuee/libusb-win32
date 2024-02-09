@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
:: oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo
:: LIBUSB-WIN32 EWDK MAKE UTILITY
:: Travis Robinson [libusbdotnet@gmail.com]
:: Peter Dons Tychesn [pdt@dontech.dk]
::
:: NOTE: param/values passed into make.cmd will override make.cfg
:: NOTE: destination directories are automatically created
:: oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo

CALL :ToAbsoluteDirs DIR_LIBUSB_DDK "%~0" DIR_LIBUSB_WORKING "!CD!"
IF /I "!DIR_LIBUSB_DDK!" NEQ "!DIR_LIBUSB_WORKING!" (
	PUSHD !CD!
	CD /D "!DIR_LIBUSB_DDK!"
)

IF /I "%~1" EQU "" GOTO ShowHelp
IF /I "%~1" EQU "?" GOTO ShowHelp
IF /I "%~1" EQU "/?" GOTO ShowHelp
IF /I "%~1" EQU "--help" GOTO ShowHelp
IF /I "%~1" EQU "help" GOTO ShowHelp

:BEGIN
CALL :ClearError

CALL :ToAbsoluteDirs CMDVAR_LIBUSB_DIR "!DIR_LIBUSB_DDK!..\"

ECHO Libusb-Win32 directory = !CMDVAR_LIBUSB_DIR!

SET MAKE_CFG=!DIR_LIBUSB_DDK!make.cfg
IF NOT EXIST "!MAKE_CFG!" (
	ECHO !MAKE_CFG! configuration file not found.
	GOTO CMDERROR
)

CALL :LoadConfig

IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

SET _PACKAGE_TYPE_=%~1

:: oooooooooooooooooooooooooooooooooooo
:: Package build section [if any]
:: 
IF /I "!_PACKAGE_TYPE_!" EQU "clean" (
	CALL :Super_Clean %*
	GOTO CMDEXIT
)
IF /I "!_PACKAGE_TYPE_!" EQU "cleanpackage" (
	CALL :Package_Clean %*
	GOTO CMDEXIT
)
IF /I "!_PACKAGE_TYPE_!" EQU "all" (
	CALL :Build_Binaries %*
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	GOTO CMDEXIT
)
IF /I "!_PACKAGE_TYPE_!" EQU "bin" (
	CALL :Build_Binaries %*
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	GOTO CMDEXIT
)

IF /I "!_PACKAGE_TYPE_!" EQU "dist" (
	SET LIBUSB_DIST_BUILD=true
	CALL :SetDDK "!CMDVAR_WINDDK_DIR!"
	CALL :TokenizeLibusbVersionH %*
	CALL :Package_Distributables %*
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	GOTO CMDEXIT
)

IF /I "!_PACKAGE_TYPE_!" EQU "makever" (
	CALL :TokenizeLibusbVersionH %*
	IF "!BUILD_ERRORLEVEL!" NEQ 0 GOTO CMDERROR
	GOTO CMDEXIT
)

IF /I "%~1" EQU "packagesetup" (
	CALL :PrepForPackaging %*
	CALL :CheckOrBuildBinaries
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	
	CALL :Package_Setup
	IF "!BUILD_ERRORLEVEL!" NEQ 0 GOTO CMDERROR
	GOTO CMDEXIT
)

IF /I "%~1" EQU "signfile" (
	CALL :SignFile %2
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	GOTO CMDEXIT
)

:: 
:: End of Package build section
:: oooooooooooooooooooooooooooooooooooo

:: oooooooooooooooooooooooooooooooooooo
:: WinDDK build section (not a package)
:: 
SET _PACKAGE_TYPE_=
CALL :LoadArguments 0 %*
	
IF NOT DEFINED CMDVAR_ARCH (
	ECHO The architecture must be specified. Example: CMD /C make.cmd "arch=x86"
	GOTO CMDERROR
)

CALL :CheckWinDDK pre
IF "!BUILD_ERRORLEVEL!" NEQ "0" GOTO CMDERROR

SET _RELEASE_OR_DEBUG_=Release
IF /I "!CMDVAR_DEBUGMODE!" equ "true" SET _RELEASE_OR_DEBUG_=Debug

IF /I "!CMDVAR_ARCH!" EQU "notused" (
	CALL :SetDDK "!CMDVAR_WINDDK_DIR!"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
) ELSE IF /I "!CMDVAR_ARCH!" EQU "x64" (
	CALL :SetDDK "!CMDVAR_WINDDK_DIR!"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
) ELSE IF /I "!CMDVAR_ARCH!" EQU "x86" (
	CALL :SetDDK "!CMDVAR_WINDDK_DIR!"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
) ELSE IF /I "!CMDVAR_ARCH!" EQU "arm" (
	CALL :SetDDK "!CMDVAR_WINDDK_DIR!"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
) ELSE IF /I "!CMDVAR_ARCH!" EQU "arm64" (
	CALL :SetDDK "!CMDVAR_WINDDK_DIR!"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
) ELSE (
	ECHO Invalid argument. arch=!CMDVAR_ARCH!
	GOTO CMDERROR
)
CALL :CheckWinDDK post
IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

call make_clean.bat all !CMDVAR_ARCH!
CALL :ClearError

SET BUILD_DIR=!BUILD_BASE_DIR!!CMDVAR_ARCH!\!_RELEASE_OR_DEBUG_!\

SET _LIBUSB_APP=!CMDVAR_APP!
CALL :Build
IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

:: 
:: Copy binaries to the output directory
:: 
CALL  :SafeCreateDir "!CMDVAR_OUTDIR!"
IF !ERRORLEVEL! NEQ 0 (
	Unable to re-create output directory !CNDVAR_OUTPUT!
	GOTO CMDERROR
)

IF EXIST !BUILD_DIR!*.pdb COPY /Y !BUILD_DIR!*.pdb "!CMDVAR_OUTDIR!" >NUL
IF EXIST !BUILD_DIR!*.sys COPY /Y !BUILD_DIR!*.sys "!CMDVAR_OUTDIR!" >NUL
IF EXIST !BUILD_DIR!*.dll COPY /Y !BUILD_DIR!*.dll "!CMDVAR_OUTDIR!" >NUL
IF EXIST !BUILD_DIR!*.exe COPY /Y !BUILD_DIR!*.exe "!CMDVAR_OUTDIR!" >NUL
IF EXIST !BUILD_DIR!libusb.lib COPY /Y !BUILD_DIR!libusb.lib "!CMDVAR_OUTDIR!" >NUL

CALL :DestroyErrorMarker

GOTO CMDEXIT

:: 
:: End of WinDDK build section
:: oooooooooooooooooooooooooooooooooooo

:: oooooooooooooooooooooooooooooooooooo
:: building functions 
:: 
:Build
	CALL :TokenizeLibusbVersionH true
	SET _title=Building libusb-win32 !_LIBUSB_APP!
	title !_title!

	SET CMDVAR_BUILDARCH=!_BUILDARCH!
	SET _ADD_C_DEFINES=/DLIBUSB0_DIR=\"!LIBUSB0_DIR!\"
	IF DEFINED CMDVAR_LOG_OUTPUT (
		SET _LOG_OUTPUT_=LOG_OUTPUT_TYPE_!CMDVAR_LOG_OUTPUT:+=+LOG_OUTPUT_TYPE_!
		SET _ADD_C_DEFINES=!_ADD_C_DEFINES! /DLOG_OUTPUT_TYPE=!_LOG_OUTPUT_!
	)
	
	CALL make_!_LIBUSB_APP!.bat !CMDVAR_ARCH! !_RELEASE_OR_DEBUG_! !_ADD_C_DEFINES!
	IF !ERRORLEVEL! NEQ 0 SET BUILD_ERRORLEVEL=1
	IF !BUILD_ERRORLEVEL! NEQ 0 (
		GOTO CMDERROR
	)
	
	IF /I "!CMDVAR_TESTSIGNING!" EQU "on" (
		IF EXIST libusb0.sys CALL :SignFile libusb0.sys
		IF EXIST libusb0.dll CALL :SignFile libusb0.dll
	)

	CALL :ClearError
	IF EXIST !BUILD_DIR!libusb0.lib move !BUILD_DIR!libusb0.lib !BUILD_DIR!libusb.lib %~1
GOTO :EOF

:Build_Binaries
	CALL :Build_PackageBinaries x86 msvc x86
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	
	PUSHD !CD!
	CD /D !_OUTDIR_!
	
	:: 
	:: build gcc lib 
	:: 
	CALL :BuildLib_GCC libusb_gcc.a libusb0.dll "!DIR_LIBUSB!libusb0.def"
	CALL :SafeMove libusb_gcc.a "!PACKAGE_LIB_DIR!gcc\libusb.a"
	
	POPD	

	CALL :Build_PackageBinaries x64 msvc_x64 amd64
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

	CALL :Build_PackageBinaries arm64 msvc_arm64 arm64
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

	CALL :SafeCopy "..\src\libusb_dyn.c" "!PACKAGE_LIB_DIR!dynamic\"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	
	CALL :SafeCopy "!PACKAGE_ROOT_DIR!gcc\libusb.a" "!PACKAGE_LIB_DIR!gcc\libusb.a" false
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	
	
	CALL :SafeMove "!PACKAGE_BIN_DIR!!_RELEASE_OR_DEBUG_!\x86\libusb0.dll" "!PACKAGE_BIN_DIR!!_RELEASE_OR_DEBUG_!\x86\libusb0_x86.dll"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	CALL :SafeCopy "!DIR_LIBUSB_DDK!..\installer_license.txt" "!PACKAGE_ROOT_DIR!installer_license.txt"
	CALL :TagEnv "!DIR_LIBUSB_DDK!..\!PACKAGE_BIN_NAME!-README.txt.in" "!PACKAGE_BIN_DIR!!PACKAGE_BIN_NAME!-README.txt"
	
	ECHO.
	ECHO libusb-win32 v!VERSION! binaries built at '!PACKAGE_BIN_DIR!'
	)
	ECHO.

GOTO :EOF

:: oooooooooooooooooooooooooooooooooooo
:: Packaging functions 
:: 
:Build_PackageBinaries
	SET _OUTDIR_=!PACKAGE_BIN_DIR!!_RELEASE_OR_DEBUG_!\%3\
	CALL :SafeCreateDir "!_OUTDIR_!"
	CALL :CmdExe make.cmd !_ARG_LINE! "arch=%1" "app=all" "outdir=!_OUTDIR_!"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
	
	CALL :SafeCreateDir "!PACKAGE_LIB_DIR!%2\"
	CALL :SafeMove "!_OUTDIR_!\libusb.lib" "!PACKAGE_LIB_DIR!%2\"

	:: Pre-sign everything
	IF /I "!CMDVAR_SIGNFILE!" EQU "true" (
		CALL :SignFile !_OUTDIR_!libusb0.dll sha256
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !_OUTDIR_!libusb0.sys sha256
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !_OUTDIR_!install-filter.exe sha256
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !_OUTDIR_!libusb0.sys sha256
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !_OUTDIR_!testlibusb.exe sha256
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !_OUTDIR_!testlibusb-win.exe sha256
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
	)
GOTO :EOF

:PackageText
	CALL :TagEnv "..\README.in" "%~2\README.txt"
	COPY /Y "..\*.txt" "%~2"

GOTO :EOF

:PrepForPackaging
	CALL :LoadArguments 1 %*
	CALL :TryCopyGccBinaries
	CALL make_super_clean.bat
GOTO :EOF

:SetPackage
	SET _PACKAGE_DEBUG_=
	IF /I "!CMDVAR_DEBUGMODE!" equ "true" SET _PACKAGE_DEBUG_=debug-
	SET CMDVAR_PCKGNAME=%~1-!_PACKAGE_DEBUG_!!CMDVAR_VERSION!
GOTO :EOF

:Package_Distributables
(
	CALL :PrepForPackaging %*
	CALL :CheckPackaging
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

	CALL :SafeCleanDir "!PACKAGE_ROOT_DIR!"
	IF !ERRORLEVEL! NEQ 0 GOTO :EOF
	
	:: Make debug binaries
	SET CMDVAR_DEBUGMODE=true
	SET _RELEASE_OR_DEBUG_=Debug
	CALL :Build_Binaries %*
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

	:: Make release binaries
	SET CMDVAR_DEBUGMODE=false
	SET _RELEASE_OR_DEBUG_=Release
	CALL :Build_Binaries %*
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

	:: Make package for microsoft upload
	IF /I "!CMDVAR_SIGNFILE!" EQU "true" (
		CALL :TagEnv "libusb0.inf.in" "!PACKAGE_BIN_DIR!Debug\libusb0.inf"
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
		CALL :SafeCopy "!PACKAGE_BIN_DIR!Debug\libusb0.inf" "!PACKAGE_BIN_DIR!Release\libusb0.inf"
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

		:: Create CAT file for submission
		inf2cat /os:7_X86,7_X64,8_X86,8_X64,10_X86,10_X64 /driver:!PACKAGE_BIN_DIR!Debug
		IF !ERRORLEVEL! NEQ 0 SET BUILD_ERRORLEVEL=1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
		inf2cat /os:7_X86,7_X64,8_X86,8_X64,10_X86,10_X64 /driver:!PACKAGE_BIN_DIR!Release
		IF !ERRORLEVEL! NEQ 0 SET BUILD_ERRORLEVEL=1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

		:: Create CAB file for submission
		CALL :TagEnv "libusb0.ddf.in" "!PACKAGE_ROOT_DIR!libusb0.ddf"
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
		makecab /f !PACKAGE_ROOT_DIR!libusb0.ddf
		IF !ERRORLEVEL! NEQ 0 SET BUILD_ERRORLEVEL=1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

		:: Sign cab file
		CALL :SignFile !PACKAGE_ROOT_DIR!libusb0.cab sha256
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF

		ECHO Submit package to microsoft and place result in package folder
		PAUSE

		:: Post-sign everything with SHA1 to allow win7 to work
		CALL :SignFile !PACKAGE_BIN_DIR!Debug\x86\libusb0.sys sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Debug\x86\libusb0_x86.dll sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Debug\amd64\libusb0.sys sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Debug\amd64\libusb0.dll sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Debug\arm64\libusb0.sys sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Debug\arm64\libusb0.dll sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF

		CALL :SignFile !PACKAGE_BIN_DIR!Release\x86\libusb0.sys sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Release\x86\libusb0_x86.dll sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Release\amd64\libusb0.sys sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Release\amd64\libusb0.dll sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Release\arm64\libusb0.sys sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
		CALL :SignFile !PACKAGE_BIN_DIR!Release\arm64\libusb0.dll sha1
		IF !BUILD_ERRORLEVEL! NEQ 0 GOTO :EOF
	)

	SET CMDVAR_DEBUGMODE=true
	SET _RELEASE_OR_DEBUG_=Debug
	CALL :Package_Bin %*
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

	SET CMDVAR_DEBUGMODE=false
	SET _RELEASE_OR_DEBUG_=Release
	CALL :Package_Bin %*
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	
	CALL :Package_Setup
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

	GOTO :EOF
)
GOTO :EOF

:Package_Bin
	CALL :SetPackage "!PACKAGE_BIN_NAME!"
	SET _WORKING_DIR=!PACKAGE_WORKING!!CMDVAR_PCKGNAME!\
	
	CALL :SafeReCreateDir "!_WORKING_DIR!"

	CALL :SafeCopyDir "!PACKAGE_BIN_DIR!!_RELEASE_OR_DEBUG_!" "!_WORKING_DIR!bin\"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

	CALL :SafeCopyDir "!PACKAGE_LIB_DIR!" "!_WORKING_DIR!lib\"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR
	
	CALL :SafeCopyDir "%CD%\..\examples" "!_WORKING_DIR!examples\"
	IF !BUILD_ERRORLEVEL! NEQ 0 GOTO CMDERROR

	CALL :SafeCopy "%CD%\..\src\lusb0_usb.h" "!_WORKING_DIR!include\"

	CALL :SafeCopy "!PACKAGE_ROOT_DIR!gcc\*" "!_WORKING_DIR!lib\gcc\"

	CALL :PackageText bin "!_WORKING_DIR!"
	
	PUSHD "!CD!"
	CD /D "!PACKAGE_WORKING!"

	"!ZIP!" -tzip a -r "!PACKAGE_SAVE_DIR!!CMDVAR_PCKGNAME!.zip" ".\!CMDVAR_PCKGNAME!"
	CALL :SafeDeleteDir ".\!CMDVAR_PCKGNAME!"
	POPD
	
GOTO :EOF

:Package_Setup
	CALL :SetPackage "!PACKAGE_SETUP_NAME!"
	SET _WORKING_DIR=!PACKAGE_WORKING!!PACKAGE_SETUP_NAME!\
	CALL :SafeReCreateDir "!_WORKING_DIR!"

	CALL :PackageText setup "!_WORKING_DIR!"
	
	IF /I "!CMDVAR_TESTSIGNING!" EQU "on" (
		CALL :SafeCopy "!PACKAGE_ROOT_DIR!cert\!CERT_FILE!" "!_WORKING_DIR!"
	)
	
	CALL :TagEnv "filter-bin-setup.iss.in" "!_WORKING_DIR!filter-bin-setup.iss"
	
	PUSHD "!CD!"
	CD /D "!_WORKING_DIR!"
	"!ISCC!" "filter-bin-setup.iss"
	IF !ERRORLEVEL! NEQ 0 GOTO CMDERROR

	CALL :SafeMove "!CMDVAR_PCKGNAME!.exe" "!PACKAGE_SAVE_DIR!"
	DEL /Q "!_WORKING_DIR!*"
	POPD
GOTO :EOF

:Super_Clean
	CALL :LoadArguments 1 %*
	CALL make_super_clean.bat
GOTO :EOF
	
:Package_Clean
	CALL :LoadArguments 1 %*
	CALL :SafeCleanDir "!PACKAGE_BIN_DIR!"
	CALL :SafeCleanDir "!PACKAGE_LIB_DIR!"
	CALL :SafeCleanDir "!PACKAGE_WORKING!"
	CALL :SafeCleanDir "!PACKAGE_SAVE_DIR!"
	CALL make_super_clean.bat
GOTO :EOF

:: oooooooooooooooooooooooooooooooooooo
:: Internal functions 
:: 
:SafeCleanDir
	IF EXIST "%~1" (
		ECHO [SafeCleanDir] %~1
		RMDIR /S /Q "%~1"
		GOTO :EOF
	)
GOTO :EOF

:SafeCopyDir
	SET __SafeCopyDir_Src=%~f1
	SET __SafeCopyDir_Dst=%~dp2
	IF NOT EXIST "!__SafeCopyDir_Src!" (
		ECHO [SafeCopyDir] %~1 does not exists.
		GOTO :EOF
	)

	IF "!__SafeCopyDir_Dst:~-1!" EQU "\" SET __SafeCopyDir_Dst=!__SafeCopyDir_Dst:~0,-1!
	FOR /F "usebackq eol=; tokens=* delims=" %%A IN (`DIR /S /B /A-H-S-R-D "!__SafeCopyDir_Src!\*.*"`) DO (
		CALL :StrRemove "%%~A" "!__SafeCopyDir_Src!"
		SET __SafeCopyDir_File=!__SafeCopyDir_Dst!\!StrRemove!
		CALL :ToAbsoluteDirs __SafeCopyDir_Dir "!__SafeCopyDir_File!"		
		IF NOT EXIST "!__SafeCopyDir_Dir!" MKDIR "!__SafeCopyDir_Dir!"
		ECHO [SafeCopyDir] File = !__SafeCopyDir_Dir!%%~nxA
		COPY /Y "%%~A" "!__SafeCopyDir_Dir!" >NUL
		
	)
GOTO :EOF

:StrRemove
	SET StrRemove=%~1
	SET StrRemove=!StrRemove:%~2=!
GOTO :EOF

:SafeReCreateDir
	CALL :SafeCleanDir "%~1"
	CALL :SafeCreateDir "%~1"
GOTO :EOF

:SafeCreateDir
	IF NOT EXIST "%~1" (
		ECHO [SafeCreateDir] %~1
		MKDIR "%~1"
		GOTO :EOF
	)
GOTO :EOF

:SafeDeleteDir
	IF EXIST "%~1" (
		ECHO [SafeDeleteDir] %~1
		RMDIR /S /Q "%~1"
		GOTO :EOF
	)
GOTO :EOF


:SafeDelete
	IF EXIST "%~1" (
		ECHO [SafeDelete] %~1
		DEL /Q "%~1"
		GOTO :EOF
	)
GOTO :EOF

:SafeCopy
	IF /I "%3" EQU "false" (
		ECHO [SafeCopy] %~2 already exists. skipping..
		IF EXIST "%~2" GOTO :EOF
	)
	IF EXIST "%~1" (
		ECHO [SafeCopy] %~1 %~2
		CALL :SafeCreateDir "%~dp2"
		COPY /Y "%~1" "%~2" 2>NUL>NUL
		GOTO :EOF
	)
GOTO :EOF

:SafeMove
	IF EXIST "%~1" (
		ECHO [SafeMove] %~1 %~2
		CALL :SafeCreateDir "%~dp2"
		MOVE /Y "%~1" "%~2"
		GOTO :EOF
	)
GOTO :EOF

:: 
:: params = outlibfile indllfile indeffile
::
:BuildLib_GCC
	IF NOT EXIST "!CMDVAR_DLLTOOL!" (
		ECHO [WARNING] gcc dlltool not found. Skipping gcc lib build..
		GOTO :EOF
	)

	"!CMDVAR_DLLTOOL!" --output-lib "%~1" --dllname "%~2" --input-def "%~3"
	IF !ERRORLEVEL! NEQ 0 ECHO [WARNING] gcc dlltool tool failed.
GOTO :EOF

:TryCopyGccBinaries
	CALL :SafeCopy ..\libusb.a "!PACKAGE_ROOT_DIR!gcc\"
GOTO :EOF

:SetDDK
	PUSHD !CD!
	SET SELECTED_DDK=%~1

	IF NOT EXIST "!SELECTED_DDK!\BuildEnv\SetupBuildEnv.cmd" (
		ECHO Failed locating WinDDK SetupBuildEnv.cmd at '!SELECTED_DDK!\BuildEnv\SetupBuildEnv.cmd'
		SET BUILD_ERRORLEVEL=1
		GOTO :EOF
	)
	CALL "!SELECTED_DDK!\BuildEnv\SetupBuildEnv.cmd"

	:: EWDK turns ECHO back on, so turn it off
	@ECHO OFF

	SET BUILD_ERRORLEVEL=!ERRORLEVEL!
	POPD
	IF NOT !BUILD_ERRORLEVEL!==0 (
		ECHO Failed setting DDK environment
	)
GOTO :EOF

:CheckOrBuildBinaries
	CALL :CmdExe make.cmd bin
	IF NOT !BUILD_ERRORLEVEL!==0 (
		ECHO [CheckOrBuildBinaries] Failed.
		pause
	)
GOTO :EOF

:CheckPackaging
	CALL :ToAbsolutePaths ZIP "!ZIP!" ISCC "!ISCC!"
	IF NOT EXIST "!ZIP!" (
		ECHO [CheckPackaging] Failed locating zip utility: !ZIP!
		ECHO [CheckPackaging] See !MAKE_CFG!
		GOTO CMDERROR
	)
	IF NOT EXIST "!ISCC!" (
		ECHO [CheckPackaging] Failed locating inno setup compiler: !ISCC!
		ECHO [CheckPackaging] See !MAKE_CFG!
		GOTO CMDERROR
	)
GOTO :EOF

:CheckWinDDK
	SET BUILD_ERRORLEVEL=1

	IF /I "%1" EQU "pre" (
		IF NOT EXIST "!CMDVAR_WINDDK_DIR!" GOTO WINDDK_NOTFOUND
		SET BUILD_ERRORLEVEL=0
		GOTO :EOF
		:WINDDK_NOTFOUND
			ECHO Invalid WinDDK Directory !CMDVAR_WINDDK_DIR!
			GOTO :EOF

	) ELSE (
		ECHO WinDDK ok.
		SET BUILD_ERRORLEVEL=0
	)
GOTO :EOF

:LoadConfig
	IF NOT EXIST "!MAKE_CFG!" (
		ECHO Config file not found "!MAKE_CFG!".
		SET BUILD_ERRORLEVEL=1
		GOTO :EOF
	)

	:: setup date variables using a generic method that works on all regions
	for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime /value') do set "LOCALTIME=%%a"
	SET _MM_=!LOCALTIME:~4,2!
	SET _DD_=!LOCALTIME:~6,2!
	SET _YYYY_=!LOCALTIME:~0,4!

	:: setup date variable to match US formatted dates for e.g. drivers
	SET DATE_US=!_MM_!/!_DD_!/!_YYYY_!

	FOR /F "eol=; tokens=1,2* usebackq delims==" %%I IN (!MAKE_CFG!) DO (
		IF NOT "%%~I" EQU "" (
			SET _PNAME=%%~I
			SET _PNAME=!_PNAME: =!
			IF /I "!_PNAME!" EQU "ZIP" (
				CALL :ToShortName _PVALUE "%%~J"
			) ELSE IF /I "!_PNAME!" EQU "ISCC" (
				CALL :ToShortName _PVALUE "%%~J"
			) ELSE IF /I "!_PNAME!" EQU "DLLTOOL" (
				CALL :ToShortName _PVALUE "%%~J"
			) ELSE (
				SET _PVALUE=%%J
			)
			SET !_PNAME!=!_PVALUE!
			SET CMDVAR_!_PNAME!=!_PVALUE!
		)
	)
	SET LIBUSB0_DIR=!PACKAGE_ROOT_DIR:\=/!
	IF "!LIBUSB0_DIR:~-1!" EQU "/" SET LIBUSB0_DIR=!LIBUSB0_DIR:~0,-1!
GOTO :EOF

:LoadArguments
	CALL :ParamValsToEnv :LoadArgumentsCallback %*
	
	IF NOT EXIST "!DIR_LIBUSB_DDK!" (
		ECHO Invalid !MAKE_CFG!.
		GOTO CMDERROR
	)
	CALL :ToAbsolutePaths DIR_LIBUSB_DDK "!DIR_LIBUSB_DDK!"
	CALL :ToAbsolutePaths DIR_LIBUSB "!DIR_LIBUSB_DDK!..\"
	
	IF "!CMDVAR_WINDDK!" NEQ "" SET CMDVAR_WINDDK_DIR=!CMDVAR_WINDDK!
	IF "!CMDVAR_OUTDIR!" EQU "" SET CMDVAR_OUTDIR=.\!CMDVAR_ARCH!
	IF "!CMDVAR_APP!" EQU "" SET CMDVAR_APP=all
GOTO :EOF

:: params = no_overwrite
:TokenizeLibusbVersionH
	CALL :ToAbsolutePaths _H_IN  "!DIR_LIBUSB_DDK!..\src\libusb-win32_version_h.in"
	CALL :ToAbsolutePaths _H_OUT "!DIR_LIBUSB_DDK!..\src\libusb-win32_version.h"
	IF /I "%~1" EQU "true" IF EXIST "!_H_OUT!" GOTO :EOF
	CALL :SafeDelete "!_H_OUT!"
	CALL :TagEnv "!_H_IN!" "!_H_OUT!"
GOTO :EOF

:LoadArgumentsCallback
	SET CMDVAR_%~1=%~2
GOTO :EOF

::
:: Parses param/value pairs.
:: Params = CallbackFunction ArgSkipCount "param1=value1" "param2=value2" ..
:: 
:ParamValsToEnv
	SET _SKIP_ARG_LINE=
	SET _ARG_LINE=
	
	SET _CALLBACK_FN=%~1
	SET _ARG_SKIP_COUNT=%~2
	SHIFT /1

	:ParamValsToEnv_Next
	SHIFT /1
	IF "%~1" EQU "" GOTO :EOF
	IF !_ARG_SKIP_COUNT! GTR 0 (
		SET _SKIP_ARG_LINE=!_SKIP_ARG_LINE! "%~1"
		SET /A _ARG_SKIP_COUNT=_ARG_SKIP_COUNT-1
		GOTO ParamValsToEnv_Next
	)
	SET _ARG_LINE=!_ARG_LINE! %1
	SET _PARAM_VALUE_=%%~1
	FOR /F "usebackq tokens=1,2 delims==" %%H IN ('%%~1') DO (
		IF "%%~H" NEQ "" (
			CALL !_CALLBACK_FN! %%H %%I
		)
	)
	GOTO ParamValsToEnv_Next
GOTO :EOF

:ToAbsoluteDirs
	IF NOT "%~1" EQU "" (
		SET %~1=%~dp2
		SHIFT /1
		SHIFT /1
		GOTO ToAbsoluteDirs
	)
GOTO :EOF

:GetDDKToolPath
	CALL :FindInPath %1 "%~2"
	IF "!%1!" EQU "" CALL :ToAbsolutePaths %1 "!CMDVAR_WINDDK_DIR!bin\x86\%~2"
	IF NOT EXIST "!%1!" (
		ECHO %~2 was not found in the path or !CMDVAR_WINDDK_DIR!bin\x86
		SET BUILD_ERRORLEVEL=1
		GOTO :EOF
	)
GOTO :EOF


:SignFile
	CALL :GetDDKToolPath MAKE-CERT makecert.exe
	IF "!BUILD_ERRORLEVEL!" NEQ "0" GOTO :EOF
	
	CALL :GetDDKToolPath CERT-MGR certmgr.exe
	IF "!BUILD_ERRORLEVEL!" NEQ "0" GOTO :EOF
	
	CALL :GetDDKToolPath SIGN-TOOL signtool.exe
	IF "!BUILD_ERRORLEVEL!" NEQ "0" GOTO :EOF
	
	ECHO [SignFile] NOTE - remember to type 'bcdedit -set testsigning on' to use
	ECHO                   test certificates on x64 machines.
	ECHO.
	IF NOT EXIST "!CMDVAR_CERT_FILE!" (
		ECHO [SignFile] clearing LibusbCertStore
		"!CERT-MGR!" -del -all -s LibusbCertStore
		
		ECHO [SignFile] making !CMDVAR_CERT_FILE! test certificate
		"!MAKE-CERT!" -a sha256 -r -pe -ss LibusbCertStore -n "CN=Libusb-Win32 Testing" !CMDVAR_CERT_FILE!
		IF "!ERRORLEVEL!" NEQ "0" (
			SET BUILD_ERRORLEVEL=1
			ECHO [SignFile] failed creating certificate !CMDVAR_CERT_FILE!
			GOTO :EOF
		)
		
		
		ECHO [SignFile] adding !CMDVAR_CERT_FILE! to root
		"!CERT-MGR!" -add !CMDVAR_CERT_FILE! -s -r localMachine root
		IF "!ERRORLEVEL!" NEQ "0" (
			SET BUILD_ERRORLEVEL=1
			ECHO [SignFile] failed adding certificate to root
			GOTO :EOF
		)	
		
		ECHO [SignFile] adding !CMDVAR_CERT_FILE! to trustedpublisher
		"!CERT-MGR!" -add !CMDVAR_CERT_FILE! -s -r localMachine trustedpublisher
		IF "!ERRORLEVEL!" NEQ "0" (
			SET BUILD_ERRORLEVEL=1
			ECHO [SignFile] failed adding certificate to trustedpublisher
			GOTO :EOF
		)		
		
	)
	
	"!SIGN-TOOL!" sign /f !CMDVAR_CERT_FILE! /as /v /fd %2 /tr http://timestamp.globalsign.com/tsa/r6advanced1 /td sha256 %1
	IF "!ERRORLEVEL!" NEQ "0" (
		SET BUILD_ERRORLEVEL=1
		GOTO :EOF
	)
GOTO :EOF

:: searches the directories listed in the PATH
:: environment variable for %2 and expands to the
:: drive letter and path of the first one found.
:: 
:FindInPath
	SET %1=%~dp$PATH:2
	IF "!%~1!" NEQ "" (
		CALL :ToAbsolutePaths %1 "!%~1!%2"
		ECHO [FindInPath] found path for %1
	)
GOTO :EOF

:ToAbsolutePaths
	IF NOT "%~1" EQU "" (
		SET %~1=%~f2
		SHIFT /1
		SHIFT /1
		GOTO ToAbsolutePaths
	)
GOTO :EOF

:ToShortName
	IF NOT "%~1" EQU "" (
		SET %~1=%~s2
		SHIFT /1
		SHIFT /1
		GOTO ToShortName
	)
GOTO :EOF

:: 
:: Params = <infile> <outfile>
:: 
:TagEnv
	CALL :CreateTempFile TAG_ENV_TMP
	
	IF NOT DEFINED _LTAG_ SET _LTAG_=@
	IF NOT DEFINED _RTAG_ SET _RTAG_=@
	IF EXIST "%~2" DEL /Q "%~2"
	ECHO [Tokenizing] %~1..
	SET CMDVAR_>!TAG_ENV_TMP!
	FOR /F "tokens=1,* delims=]" %%A IN ('"type %1|find /n /v """') DO (
		SET "line=%%B"
		IF DEFINED line (
			FOR /F "tokens=1,* usebackq delims==" %%I IN (!TAG_ENV_TMP!) DO (
				SET _TOKEN_KEY_=%%I
				SET _TOKEN_KEY_=!_LTAG_!!_TOKEN_KEY_:~7!!_RTAG_!
				CALL SET "line=%%line:!_TOKEN_KEY_!=%%~J%%"
			)
			ECHO !line!>> "%~2"
		) ELSE ECHO.>> "%~2"
	)
	CALL :DestroyTempFile TAG_ENV_TMP

GOTO :EOF

:CreateTempFile
	CALL :SafeCreateDir "!BUILD_BASE_DIR!"
	SET %1=!DIR_LIBUSB_DDK!!BUILD_BASE_DIR!tf!RANDOM!.tmp
GOTO :EOF

:DestroyTempFile
	IF EXIST "!%1!" (
		DEL /Q "!%1!"
		IF EXIST "!%1!" ECHO [DestroyTempFile] !%1! access denied.
	) ELSE (
		ECHO [DestroyTempFile] !%1! not found.
	)
	SET %1=
GOTO :EOF

:CreateErrorMarker
	SET _EMARKER=1
	CALL :SafeCreateDir "!BUILD_BASE_DIR!"
	ECHO !_EMARKER!>"!DIR_LIBUSB_DDK!!BUILD_BASE_DIR!emarker.tmp"
GOTO :EOF

:DestroyErrorMarker
	SET _EMARKER=
	IF EXIST "!DIR_LIBUSB_DDK!!BUILD_BASE_DIR!emarker.tmp" (
		DEL /Q "!DIR_LIBUSB_DDK!!BUILD_BASE_DIR!emarker.tmp"
		SET _EMARKER=1
	)
GOTO :EOF

:CmdExe
	CALL :CreateErrorMarker
	CMD /C %*
	IF NOT !ERRORLEVEL!==0 SET BUILD_ERRORLEVEL=1
	CALL :DestroyErrorMarker
	IF DEFINED _EMARKER! (
		SET BUILD_ERRORLEVEL=1
		ECHO [CmdExe] Last build did not complete.
	)
GOTO :EOF


:ClearError
	SET BUILD_ERRORLEVEL=0
GOTO :EOF

:CMDERROR
	SET BUILD_ERRORLEVEL=1
GOTO CMDEXIT

:CMDEXIT
IF /I "!DIR_LIBUSB_DDK!" NEQ "!DIR_LIBUSB_WORKING!" POPD
EXIT /B !BUILD_ERRORLEVEL!

:ShowHelp

ECHO.
ECHO LIBUSB-WIN32 WinDDK build utility/application packager
ECHO.
ECHO Summary: This batch script automates the libusb-win32 EWDK build process
ECHO          and creates libusb-win32 redistributable packages.
ECHO          [see also make.cfg]
ECHO.
ECHO BUILD USAGE: CMD /C make.cmd "Option=Value"
ECHO Options: 
ECHO [req] ARCH   x86/x64/arm/arm64
ECHO APP          all/dll/driver/install_filter/install_filter_win/test/testwin
ECHO              [Default = all]
ECHO OUTDIR       Directory that will contain the compiled binaries
ECHO              [Default = .\ARCH]
ECHO WINDDK       EWDK root directory
ECHO              [Default = see make.cfg]
ECHO DEBUGMODE    Setting this option to 'true' makes chk builds instead of fre.
ECHO              This also enables kernel debug messages and sets all default
ECHO              log modes to the max verbosity.
ECHO              [Default = false]
ECHO TESTSIGNING  Setting this option to 'on' signs te dll and driver with a
ECHO              test certifcate.
ECHO              [Default = off]
ECHO SIGNFILE     Sign the resulting binaries with a signature
ECHO		  [Default = false]
ECHO LOG_OUTPUT   Changes the log output type.  By default, console apps send log
ECHO              messages to stderr, dlls and windows apps send log messages to 
ECHO              OutputDebugString and kernel drivers send messages to 
ECHO              DbgPrint. Use DebugView to view DBGPRINT, DEBUGWINDOW logs.
ECHO              http://download.sysinternals.com/Files/DebugView.zip
ECHO              Log Output Types (case sensitive, combinable with +):
ECHO					DEFAULT      Use the build defaults. [see above]
ECHO					REMOVE       Strip all log messages [except errors].
ECHO					STDERR       output to stderr
ECHO					DEBUGWINDOW	 OutputDebugString, DbgPrint
ECHO					FILE	     redirect log messages to a file.
ECHO.
ECHO [Note: See make.cfg for more options that can be used when building]
ECHO Examples:
ECHO CMD /C make.cmd "arch=x86" "app=all" "outdir=.\x86"
ECHO CMD /C make.cmd "arch=x64" "outdir=.\x64" "winddk=E:\"
ECHO CMD /C make.cmd "arch=x64" "testsigning=on"
ECHO CMD /C make.cmd "arch=x86"
ECHO.
ECHO PACKAGE USAGE: make.cmd PackageCommand "Option=Value"
ECHO Package Commands:
ECHO ALL       Build binaries for all architectures.
ECHO DIST      Creates libusb-win32 dist packages.
ECHO.
ECHO Additional Commands:
ECHO CLEAN        Cleans all temporary files.
ECHO CLEANPACKAGE Cleans root package directory.
ECHO MAKEVER      Re/creates libusb-win32_version.h from the template.
ECHO.
ECHO [Note: See make.cfg for options that can be used when packaging]
ECHO.
ECHO Example: make.cmd clean
ECHO Example: make.cmd all
ECHO Example: make.cmd dist
ECHO.
GOTO CMDERROR

