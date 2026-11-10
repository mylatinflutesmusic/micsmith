@echo off
setlocal
set ROOT=%~dp0
set CMAKE=%ROOT%..\tools\cmake\bin\cmake.exe

if not exist "%CMAKE%" (
    echo [!] CMake not found at "%CMAKE%"
    exit /b 1
)

"%CMAKE%" -S "%ROOT%." -B "%ROOT%build" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%ROOT%build" --config Release --target KChannelDSPTest
if errorlevel 1 exit /b 1

echo.
echo == DSP tests ==
"%ROOT%build\Release\KChannelDSPTest.exe"
if errorlevel 1 exit /b 1

echo.
echo K CHANNEL DSP TESTS PASSED
