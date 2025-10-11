@echo off
REM PawLang Compiler Launcher Script for Windows
REM Automatically adds DLL directory to PATH

REM Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"
set "BIN_DIR=%SCRIPT_DIR%..\bin"

REM Add bin directory to PATH for DLL loading
set "PATH=%BIN_DIR%;%PATH%"

REM Run the actual pawc compiler
"%BIN_DIR%\pawc.exe" %*

