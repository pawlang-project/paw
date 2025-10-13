@echo off
REM LLVM Auto-Download Script for Windows (Batch version)
REM For systems without PowerShell or prefer batch

setlocal enabledelayedexpansion

set LLVM_VERSION=21.1.3
set BASE_URL=https://github.com/pawlang-project/llvm-build/releases/download/llvm-%LLVM_VERSION%

echo ==========================================
echo    LLVM Auto-Download for Windows
echo ==========================================
echo.

REM Detect architecture
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set TARGET=windows-x86_64
) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set TARGET=windows-aarch64
) else if "%PROCESSOR_ARCHITECTURE%"=="x86" (
    set TARGET=windows-x86
) else (
    echo [31mUnsupported architecture: %PROCESSOR_ARCHITECTURE%[0m
    echo.
    echo Supported architectures:
    echo   - x86_64 (AMD64)
    echo   - ARM64
    echo   - x86 (32-bit)
    exit /b 1
)

echo Detected platform: %TARGET%
echo.

REM Check if already exists
set VENDOR_DIR=vendor\llvm\%TARGET%
if exist "%VENDOR_DIR%\install" (
    echo [33mLLVM already exists: %VENDOR_DIR%\install\[0m
    set /p confirm="Re-download? [y/N]: "
    if /i not "!confirm!"=="y" (
        echo Cancelled
        exit /b 0
    )
    rmdir /s /q "%VENDOR_DIR%"
)

REM Download
set FILENAME=llvm-%LLVM_VERSION%-%TARGET%.tar.gz
set URL=%BASE_URL%/%FILENAME%

echo [36mDownloading LLVM %LLVM_VERSION% for %TARGET%...[0m
echo    URL: %URL%
echo.

REM Use PowerShell for download (more reliable than certutil)
powershell -Command "(New-Object System.Net.WebClient).DownloadFile('%URL%', '%FILENAME%')"

if errorlevel 1 (
    echo [31mDownload failed[0m
    exit /b 1
)

echo.
echo [36mExtracting...[0m

REM Create directory
if not exist "%VENDOR_DIR%" mkdir "%VENDOR_DIR%"

REM Extract using tar (Windows 10+)
tar -xzf "%FILENAME%" -C "%VENDOR_DIR%\"

if errorlevel 1 (
    echo [31mExtraction failed[0m
    echo [33mPlease ensure you have tar (Windows 10+) or 7-Zip installed[0m
    exit /b 1
)

REM Clean up
del "%FILENAME%"

echo.
echo ==========================================
echo    [32mLLVM Installed Successfully![0m
echo ==========================================
echo.
echo Installation location: %VENDOR_DIR%\install\
echo.
echo Included tools:
echo   - clang.exe - C/C++ compiler
echo   - llc.exe - LLVM compiler
echo   - lld.exe - LLVM linker
echo.
echo Verify installation:
echo   %VENDOR_DIR%\install\bin\clang.exe --version
echo.
echo [32mNow you can build PawLang:[0m
echo    zig build
echo.
echo [32mUse LLVM backend:[0m
echo    .\zig-out\bin\pawc.exe hello.paw --backend=llvm
echo.

endlocal

