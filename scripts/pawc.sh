#!/bin/bash
# PawLang Compiler Launcher Script
# Automatically sets up library paths for LLVM dependencies

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BIN_DIR="$SCRIPT_DIR/bin"
LIB_DIR="$SCRIPT_DIR/lib"

# Set library path for LLVM dependencies
if [ -d "$LIB_DIR" ]; then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        export DYLD_LIBRARY_PATH="$LIB_DIR:$DYLD_LIBRARY_PATH"
    else
        # Linux
        export LD_LIBRARY_PATH="$LIB_DIR:$LD_LIBRARY_PATH"
    fi
fi

# Run the actual pawc compiler
exec "$BIN_DIR/pawc" "$@"

