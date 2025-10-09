#!/usr/bin/env python3
"""
Build LLVM locally for PawLang
Cross-platform script for Windows, Linux, and macOS
"""

import os
import sys
import subprocess
import platform
import multiprocessing
import argparse
from pathlib import Path

LLVM_VERSION = "19.1.6"

def get_project_root():
    """Get the project root directory"""
    return Path(__file__).parent.parent.absolute()

def print_header(text):
    """Print formatted header"""
    print("=" * 64)
    print(f"        {text}")
    print("=" * 64)
    print()

def check_command(cmd):
    """Check if a command is available"""
    import shutil
    return shutil.which(cmd) is not None

def get_cpu_count():
    """Get the number of CPU cores"""
    try:
        return multiprocessing.cpu_count()
    except:
        return 4

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description='Build LLVM for PawLang')
    parser.add_argument('--interactive', action='store_true', 
                       help='Enable interactive mode (prompts for confirmation)')
    parser.add_argument('-j', '--jobs', type=int, default=0,
                       help='Number of parallel jobs (default: auto-detect CPU cores)')
    parser.add_argument('--clean', action='store_true',
                       help='Clean build directory before building')
    args = parser.parse_args()
    
    project_root = get_project_root()
    llvm_src = project_root / "llvm" / LLVM_VERSION / "llvm"
    llvm_build = project_root / "llvm" / "build"
    llvm_install = project_root / "llvm" / "install"
    
    print_header("Building LLVM 19.1.6 for PawLang")
    
    # Check if source exists
    if not llvm_src.exists():
        print(f"Error: LLVM source not found at: {llvm_src}")
        print()
        print("Please download LLVM source first:")
        print("   python scripts/setup_llvm.py")
        return 1
    
    # Check dependencies
    print("Checking build dependencies...")
    
    if not check_command("cmake"):
        print("Error: CMake not found")
        print("Install CMake first")
        return 1
    
    if not check_command("ninja"):
        print("Error: Ninja not found")
        print("Install Ninja first")
        return 1
    
    system = platform.system()
    cxx_compiler = None
    
    if system == "Windows":
        if check_command("cl"):
            cxx_compiler = "Visual Studio (cl.exe)"
        elif check_command("g++"):
            cxx_compiler = "MinGW g++"
        else:
            print("Error: No C++ compiler found (Visual Studio or MinGW)")
            return 1
    else:
        if check_command("g++"):
            cxx_compiler = "g++"
        elif check_command("clang++"):
            cxx_compiler = "clang++"
        else:
            print("Error: No C++ compiler found (g++ or clang++)")
            return 1
    
    print("Build dependencies OK")
    
    # Show versions
    try:
        cmake_version = subprocess.check_output(["cmake", "--version"],
                                               stderr=subprocess.DEVNULL).decode().split('\n')[0]
        print(f"   CMake: {cmake_version}")
    except:
        pass
    
    try:
        ninja_version = subprocess.check_output(["ninja", "--version"],
                                               stderr=subprocess.DEVNULL).decode().strip()
        print(f"   Ninja: {ninja_version}")
    except:
        pass
    
    print(f"   Compiler: {cxx_compiler}")
    print()
    
    # Clean build directory if requested
    if args.clean and llvm_build.exists():
        print(f"Cleaning build directory: {llvm_build}")
        import shutil
        shutil.rmtree(llvm_build)
        print("Build directory cleaned.")
        print()
    
    # Create build directories
    llvm_build.mkdir(exist_ok=True)
    llvm_install.mkdir(exist_ok=True)
    
    # Get CPU count
    cpu_count = args.jobs if args.jobs > 0 else get_cpu_count()
    
    print("=" * 64)
    print("WARNING: LLVM build takes 30-60 minutes and ~10GB disk space")
    print("=" * 64)
    print()
    print("Build Configuration:")
    print(f"   Source:  {llvm_src}")
    print(f"   Build:   {llvm_build}")
    print(f"   Install: {llvm_install}")
    print(f"   Cores:   {cpu_count}")
    print()
    
    if args.interactive:
        response = input("Continue build? (Y/n): ").strip().lower()
        if response == 'n':
            print("Build cancelled.")
            return 0
    else:
        print("Starting build (non-interactive mode)...")
        print("Use --interactive flag to enable prompts.")
        print()
    
    # Change to build directory
    os.chdir(llvm_build)
    
    # Configure
    print()
    print("Step 1/3: Configuring CMake...")
    print()
    
    cmake_args = [
        "cmake", str(llvm_src),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={llvm_install}",
        "-DLLVM_ENABLE_PROJECTS=clang",
        "-DLLVM_ENABLE_ASSERTIONS=OFF",
        "-DLLVM_ENABLE_RTTI=ON",
        "-DLLVM_BUILD_TOOLS=ON",
        "-DLLVM_BUILD_EXAMPLES=OFF",
        "-DLLVM_BUILD_TESTS=OFF",
        "-DLLVM_INCLUDE_TESTS=OFF",
        "-DLLVM_INCLUDE_EXAMPLES=OFF",
        "-DLLVM_INCLUDE_DOCS=OFF",
        "-DLLVM_ENABLE_BINDINGS=OFF",
        "-G", "Ninja"
    ]
    
    # Platform-specific targets
    if system == "Darwin":
        cmake_args.append("-DLLVM_TARGETS_TO_BUILD=AArch64;X86")
    else:
        cmake_args.append("-DLLVM_TARGETS_TO_BUILD=X86")
    
    try:
        subprocess.run(cmake_args, check=True)
    except subprocess.CalledProcessError as e:
        print("CMake configuration failed")
        return 1
    
    # Build
    print()
    print(f"Step 2/3: Building LLVM (using {cpu_count} cores)...")
    print("This will take 30-60 minutes. Please be patient...")
    print()
    
    try:
        subprocess.run(["ninja", "-j", str(cpu_count)], check=True)
    except subprocess.CalledProcessError:
        print("Ninja build failed")
        return 1
    
    # Install
    print()
    print("Step 3/3: Installing...")
    print()
    
    try:
        subprocess.run(["ninja", "install"], check=True)
    except subprocess.CalledProcessError:
        print("Ninja install failed")
        return 1
    
    print()
    print_header("LLVM Build Completed Successfully!")
    
    print("Installation Information:")
    print(f"   Location: {llvm_install}")
    
    # Try to get version
    llvm_config = llvm_install / "bin" / ("llvm-config.exe" if system == "Windows" else "llvm-config")
    if llvm_config.exists():
        try:
            version = subprocess.check_output([str(llvm_config), "--version"],
                                            stderr=subprocess.DEVNULL).decode().strip()
            print(f"   Version: {version}")
        except:
            pass
    
    print()
    print("Next Steps:")
    print("   1. Build PawLang with LLVM support:")
    print("      zig build")
    print()
    print("   2. Test LLVM backend:")
    if system == "Windows":
        print("      .\\zig-out\\bin\\pawc.exe examples\\hello.paw --backend=llvm")
    else:
        print("      ./zig-out/bin/pawc examples/hello.paw --backend=llvm")
    print()
    
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\nBuild interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        print()
        print("Troubleshooting:")
        print("   - Check disk space (need ~10GB free)")
        print("   - Check build log in: llvm/build")
        print("   - Try clean build: Delete llvm/build directory")
        sys.exit(1)

