@echo off
rem Build Windows 7-compatible VST2 binaries (32-bit and 64-bit).
rem Requires JUCE 7.0.12 in ..\JUCE7 and the legacy VST2 SDK headers in
rem third_party\vst2sdk. The VST2 format is legacy Steinberg technology.
setlocal
set ROOT=%~dp0
set CMAKE=%ROOT%..\tools\cmake\bin\cmake.exe
set JUCE7=%ROOT%..\JUCE7
set VST2SDK=%ROOT%third_party\vst2sdk

if not exist "%CMAKE%" (
    echo [!] CMake not found at %CMAKE%
    exit /b 1
)

if not exist "%JUCE7%\CMakeLists.txt" (
    echo [!] JUCE 7 not found at %JUCE7%
    echo     Clone juce-framework/JUCE at tag 7.0.12 there.
    exit /b 1
)

if not exist "%VST2SDK%\pluginterfaces\vst2.x\aeffect.h" (
    echo [!] VST2 SDK headers not found at %VST2SDK%
    exit /b 1
)

set COMMON=-S "%ROOT%." -G "Visual Studio 17 2022" -DKCHANNEL_JUCE_DIR="%JUCE7%" -DKCHANNEL_VST2_SDK="%VST2SDK%" -DKCHANNEL_FORMATS=VST -DKCHANNEL_WINDOWS7_COMPAT=ON

echo.
echo === Configuring 32-bit VST2 ===
"%CMAKE%" %COMMON% -B "%ROOT%build-vst2-win32" -A Win32
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%ROOT%build-vst2-win32" --config Release --target KChannel_VST --parallel
if errorlevel 1 exit /b 1

echo.
echo === Configuring 64-bit VST2 ===
"%CMAKE%" %COMMON% -B "%ROOT%build-vst2-win64" -A x64
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%ROOT%build-vst2-win64" --config Release --target KChannel_VST --parallel
if errorlevel 1 exit /b 1

echo.
echo Build finished:
echo   32-bit: %ROOT%build-vst2-win32\KChannel_artefacts\Release\VST\K Channel.dll
echo   64-bit: %ROOT%build-vst2-win64\KChannel_artefacts\Release\VST\K Channel.dll
