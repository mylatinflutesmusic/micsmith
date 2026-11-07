@echo off
setlocal EnableExtensions

rem Build and run the local Quell test workflow. The DAW round-trip needs
rem REAPER and installs the freshly built VST3 into the per-user VST3 folder
rem so that it tests the current code rather than an older installation.

set "ROOT=%~dp0"
set "CMAKE=%ROOT%..\tools\cmake\bin\cmake.exe"
set "BUILD=%ROOT%build"
set "HOST=%BUILD%\QuellHostTest_artefacts\Release\QuellHostTest.exe"
set "VST3=%BUILD%\Quell_artefacts\Release\VST3\Quell.vst3"
set "INSTALLED=%LOCALAPPDATA%\Programs\Common\VST3\Quell.vst3"

if not exist "%CMAKE%" (
    echo [!] CMake not found at "%CMAKE%"
    exit /b 1
)

echo.
echo == Build test targets ==
"%CMAKE%" --build "%BUILD%" --config Release --target QuellDSPTest QuellHostTest Quell_VST3
if errorlevel 1 goto fail

echo.
echo == DSP tests ==
"%BUILD%\Release\QuellDSPTest.exe"
if errorlevel 1 goto fail

echo.
echo == Host validation ==
"%HOST%" "%VST3%"
if errorlevel 1 goto fail

if exist "%BUILD%\daw-real\dry.wav" (
    echo.
    echo == Real-mix default regression ==
    "%HOST%" --defaults-mix "%BUILD%\daw-real\dry.wav"
    if errorlevel 1 goto fail
) else (
    echo.
    echo [skip] Real-mix default regression: "%BUILD%\daw-real\dry.wav" not found.
)

echo.
echo == Install built VST3 for the REAPER test ==
if not exist "%LOCALAPPDATA%\Programs\Common\VST3" mkdir "%LOCALAPPDATA%\Programs\Common\VST3"
if exist "%INSTALLED%" rmdir /s /q "%INSTALLED%"
xcopy "%VST3%" "%INSTALLED%\" /E /I /Y /Q >nul
if errorlevel 1 (
    echo [!] Could not install the built VST3. Close Quell/REAPER and retry.
    goto fail
)

if not exist "%INSTALLED%\Contents\x86_64-win\Quell.vst3" (
    echo [!] Installed VST3 was not created at "%INSTALLED%".
    goto fail
)

echo.
echo == REAPER preset round-trip ==
python "%ROOT%tools\daw\run_preset_reopen_test.py"
if errorlevel 1 goto fail

echo.
echo ALL QUELL TESTS PASSED
exit /b 0

:fail
echo.
echo QUELL TESTS FAILED
exit /b 1
