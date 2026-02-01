@echo off
setlocal enabledelayedexpansion
cls

echo Detecting Visual Studio environment...
set "VSVARS_PATH="

for /f "tokens=*" %%a in ('
    reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall" /s /f "Visual Studio" ^|
    findstr "HKEY"
') do (
    for /f "tokens=2*" %%b in ('
        reg query "%%a" /v InstallLocation 2^>nul ^|
        findstr /i "InstallLocation"
    ') do (
        if not "%%c"=="" (
            if exist "%%c\VC\Auxiliary\Build\vcvars64.bat" (
                set "VSVARS_PATH=%%c\VC\Auxiliary\Build\vcvars64.bat"
                goto :found_vs
            )
        )
    )
)

:found_vs
if not defined VSVARS_PATH (
    echo ERROR: Visual Studio not found.
    exit /b 1
)

echo Using VS environment: %VSVARS_PATH%
call "%VSVARS_PATH%"
echo.

echo Building CPIO
echo.

if not exist obj mkdir obj

set CFLAGS=/nologo /O2 /W4 /GS- /Gs999999 /Oi- /Gi- /MT /Zl /Iinclude /Foobj\
set LDFLAGS=/nologo /SUBSYSTEM:CONSOLE /NODEFAULTLIB /ENTRY:mainCRTStartup
set LIBS=kernel32.lib shell32.lib

echo Compiling cpio_util.c...
cl %CFLAGS% /c src\cpio_util.c
if %ERRORLEVEL% NEQ 0 goto error

echo Compiling cpio_newc.c...
cl %CFLAGS% /c src\cpio_newc.c
if %ERRORLEVEL% NEQ 0 goto error

echo Compiling cpio_odc.c...
cl %CFLAGS% /c src\cpio_odc.c
if %ERRORLEVEL% NEQ 0 goto error

echo Compiling cpio_tool.c...
cl %CFLAGS% /c src\cpio_tool.c
if %ERRORLEVEL% NEQ 0 goto error

echo Linking cpio.exe...
link %LDFLAGS% /OUT:cpio.exe obj\cpio_util.obj obj\cpio_newc.obj obj\cpio_odc.obj obj\cpio_tool.obj %LIBS%
if %ERRORLEVEL% NEQ 0 goto error

echo.
echo Build successful! Created cpio.exe
echo.
echo File size:
dir cpio.exe | findstr cpio.exe
echo.
goto end

:error
echo.
echo Build failed!
echo.
pause
exit /b 1

:end
echo.
echo Build successful!
exit /b 0

