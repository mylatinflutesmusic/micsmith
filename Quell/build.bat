@echo off
rem Configure and build Quell (VST3 + Standalone) with the repository toolchain.
setlocal
set ROOT=%~dp0
set CMAKE=%ROOT%..\tools\cmake\bin\cmake.exe

if not exist "%CMAKE%" (
    echo [!] CMake not found at %CMAKE%
    echo     Download cmake-3.31.6-windows-x86_64.zip into tools\ and extract to tools\cmake
    exit /b 1
)

"%CMAKE%" -S "%ROOT%." -B "%ROOT%build" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%ROOT%build" --config Release --parallel
if errorlevel 1 exit /b 1

echo.
echo Build finished:
echo   VST3       : %ROOT%build\Quell_artefacts\Release\VST3\Quell.vst3
echo   Standalone : %ROOT%build\Quell_artefacts\Release\Standalone\Quell.exe
