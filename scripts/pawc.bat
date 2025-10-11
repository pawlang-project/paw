@echo off
REM PawLang Compiler Launcher Script for Windows
REM This script automatically adds the bin directory to PATH for self-contained distribution

SET SCRIPT_DIR=%~dp0
SET BIN_DIR=%SCRIPT_DIR%..\bin
SET LIB_DIR=%SCRIPT_DIR%..\lib

REM Add bin directory to PATH (DLLs are in bin directory on Windows)
SET PATH=%BIN_DIR%;%PATH%

REM Execute pawc with all arguments
"%BIN_DIR%\pawc.exe" %*

