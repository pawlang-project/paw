#!/bin/bash

# PawLang Compiler Launcher Script for Unix-like systems
# This script automatically sets up library paths for self-contained distribution

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BIN_DIR="$SCRIPT_DIR/../bin"
LIB_DIR="$SCRIPT_DIR/../lib"

# Add library directory to library path if it exists
if [ -d "$LIB_DIR" ]; then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        export DYLD_LIBRARY_PATH="$LIB_DIR:$DYLD_LIBRARY_PATH"
    else
        # Linux and other Unix-like systems
        export LD_LIBRARY_PATH="$LIB_DIR:$LD_LIBRARY_PATH"
    fi
fi

# Execute pawc with all arguments
exec "$BIN_DIR/pawc" "$@"

