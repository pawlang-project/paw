#!/usr/bin/env python3
"""
Setup LLVM source code for PawLang
Cross-platform script for Windows, Linux, and macOS
"""

import os
import sys
import subprocess
import platform
import shutil
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

def check_existing_source(llvm_src_dir):
    """Check if LLVM source already exists"""
    llvm_path = llvm_src_dir / "llvm"
    if llvm_path.exists():
        print(f"LLVM source already exists at: {llvm_src_dir}")
        
        # Get directory size
        try:
            size = sum(f.stat().st_size for f in llvm_src_dir.rglob('*') if f.is_file())
            size_mb = size / (1024 * 1024)
            print(f"   Size: {size_mb:.2f} MB")
        except:
            print("   Size: unknown")
        
        print()
        response = input("Re-download source? (y/N): ").strip().lower()
        
        if response == 'y':
            print("Removing existing source...")
            shutil.rmtree(llvm_src_dir)
            return False
        else:
            print("Using existing source.")
            return True
    
    return False

def download_with_git(llvm_src_dir):
    """Download LLVM source using git"""
    print("Cloning LLVM project with Git...")
    print("This may take 5-10 minutes...")
    
    try:
        subprocess.run([
            "git", "clone",
            "--depth", "1",
            "--branch", f"llvmorg-{LLVM_VERSION}",
            "https://github.com/llvm/llvm-project.git",
            str(llvm_src_dir)
        ], check=True)
        
        print("Download completed!")
        return True
    except subprocess.CalledProcessError:
        print("Git clone failed.")
        return False
    except FileNotFoundError:
        print("Git not installed.")
        return False

def download_archive(llvm_src_dir):
    """Download LLVM source as archive"""
    print("Downloading archive...")
    print("This may take several minutes...")
    
    url = f"https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-{LLVM_VERSION}.tar.gz"
    archive_name = f"llvmorg-{LLVM_VERSION}.tar.gz"
    
    try:
        # Try to use urllib (built-in)
        import urllib.request
        import tarfile
        
        print(f"   URL: {url}")
        
        # Download
        urllib.request.urlretrieve(url, archive_name)
        print("Download completed!")
        
        print("Extracting archive...")
        print("This may take a few minutes...")
        
        # Extract
        with tarfile.open(archive_name, 'r:gz') as tar:
            tar.extractall()
        
        # Move to target directory
        extracted_dir = f"llvm-project-llvmorg-{LLVM_VERSION}"
        if os.path.exists(extracted_dir):
            os.rename(extracted_dir, llvm_src_dir)
        
        # Clean up
        os.remove(archive_name)
        
        print("Extraction completed!")
        return True
        
    except Exception as e:
        print(f"Download failed: {e}")
        print()
        print("Manual download option:")
        print(f"   1. Visit: {url}")
        print(f"   2. Extract to: {llvm_src_dir}")
        return False

def verify_source(llvm_src_dir):
    """Verify LLVM source"""
    print("Verifying LLVM source...")
    
    llvm_path = llvm_src_dir / "llvm"
    if not llvm_path.exists():
        print(f"Error: LLVM source directory not found: {llvm_path}")
        return False
    
    cmake_path = llvm_path / "CMakeLists.txt"
    if not cmake_path.exists():
        print("Error: LLVM CMakeLists.txt not found")
        return False
    
    print("LLVM source verification passed")
    print(f"   Version: {LLVM_VERSION}")
    
    # Get directory size
    try:
        size = sum(f.stat().st_size for f in llvm_src_dir.rglob('*') if f.is_file())
        size_mb = size / (1024 * 1024)
        print(f"   Size: {size_mb:.2f} MB")
    except:
        print("   Size: unknown")
    
    print(f"   Directory: {llvm_src_dir}")
    return True

def check_command(cmd):
    """Check if a command is available"""
    return shutil.which(cmd) is not None

def check_dependencies():
    """Check build dependencies"""
    print("Checking build dependencies...")
    
    missing = []
    
    # Check CMake
    if not check_command("cmake"):
        missing.append("cmake")
    
    # Check Ninja
    if not check_command("ninja"):
        missing.append("ninja")
    
    # Check C++ compiler
    system = platform.system()
    cxx_compiler = None
    
    if system == "Windows":
        if check_command("cl"):
            cxx_compiler = "Visual Studio (cl.exe)"
        elif check_command("g++"):
            cxx_compiler = "MinGW g++"
        else:
            missing.append("C++ compiler (Visual Studio or MinGW)")
    else:
        if check_command("g++"):
            cxx_compiler = "g++"
        elif check_command("clang++"):
            cxx_compiler = "clang++"
        else:
            missing.append("C++ compiler (g++ or clang++)")
    
    if missing:
        print(f"Error: Missing dependencies: {', '.join(missing)}")
        print()
        print("Install dependencies:")
        
        if system == "Darwin":  # macOS
            print("  brew install cmake ninja")
        elif system == "Linux":
            print("  Ubuntu/Debian: sudo apt install cmake ninja-build build-essential")
            print("  CentOS/RHEL: sudo yum install cmake ninja-build gcc-c++")
            print("  Arch: sudo pacman -S cmake ninja gcc")
        elif system == "Windows":
            print("  CMake: https://cmake.org/download/")
            print("  Ninja: choco install ninja")
            print("  Visual Studio: https://visualstudio.microsoft.com/downloads/")
            print("  Or: choco install cmake ninja visualstudio2022buildtools")
        
        return False
    
    print("Build dependencies check passed")
    
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
    
    if cxx_compiler:
        print(f"   Compiler: {cxx_compiler}")
    
    return True

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description='Setup LLVM source for PawLang')
    parser.add_argument('-y', '--yes', action='store_true', help='Auto-confirm all prompts')
    parser.add_argument('-m', '--method', choices=['git', 'archive'], default='git',
                       help='Download method: git or archive (default: git)')
    args = parser.parse_args()
    
    project_root = get_project_root()
    llvm_dir = project_root / "llvm"
    llvm_src_dir = llvm_dir / LLVM_VERSION
    llvm_build_dir = llvm_dir / "build"
    llvm_install_dir = llvm_dir / "install"
    
    print_header("LLVM Source Setup Script (Cross-Platform)")
    
    print(f"Platform: {platform.system()} {platform.machine()}")
    print(f"Python: {sys.version.split()[0]}")
    print()
    
    print("Preparing to download LLVM", LLVM_VERSION, "source code...")
    print()
    print("Download Information:")
    print(f"   Version: {LLVM_VERSION}")
    print(f"   Target directory: {llvm_src_dir}")
    print("   Compressed size: ~200MB")
    print("   Extracted size: ~2GB")
    print()
    
    # Check existing source
    llvm_path = llvm_src_dir / "llvm"
    if llvm_path.exists():
        print(f"LLVM source already exists at: {llvm_src_dir}")
        try:
            size = sum(f.stat().st_size for f in llvm_src_dir.rglob('*') if f.is_file())
            size_mb = size / (1024 * 1024)
            print(f"   Size: {size_mb:.2f} MB")
        except:
            print("   Size: unknown")
        print()
        
        if not args.yes:
            response = input("Re-download source? (y/N): ").strip().lower()
            if response != 'y':
                print("Using existing source.")
                print()
                print_header("Setup Complete (Using Existing Source)")
                print("Next step: Run build_llvm.py to compile LLVM")
                return 0
        else:
            print("Using existing source (--yes flag).")
            print()
            print_header("Setup Complete (Using Existing Source)")
            print("Next step: Run build_llvm.py to compile LLVM")
            return 0
        
        print("Removing existing source...")
        shutil.rmtree(llvm_src_dir)
    
    # Confirm download
    if not args.yes:
        response = input("Continue download? (Y/n): ").strip().lower()
        if response == 'n':
            print("Cancelled.")
            return 1
    
    # Create directory
    llvm_dir.mkdir(exist_ok=True)
    
    # Select method
    method = args.method
    if not args.yes:
        print()
        print("Download Method:")
        print("   1. Git Clone (Recommended, faster)")
        print("   2. Download Archive (Alternative)")
        print()
        
        method_input = input("Select method (1/2): ").strip()
        method = 'git' if method_input == '1' else 'archive'
    
    success = False
    if method == 'git' or method == '1':
        success = download_with_git(llvm_src_dir)
    else:
        success = download_archive(llvm_src_dir)
    
    if not success:
        return 1
    
    print()
    
    # Verify
    if not verify_source(llvm_src_dir):
        return 1
    
    print()
    
    # Check dependencies
    if not check_dependencies():
        print()
        print("Note: You can still continue. Install dependencies before building.")
    
    print()
    
    # Create build directories
    llvm_build_dir.mkdir(exist_ok=True)
    llvm_install_dir.mkdir(exist_ok=True)
    
    print_header("LLVM Source Setup Completed Successfully!")
    
    print("Setup information:")
    print(f"   Version: {LLVM_VERSION}")
    print(f"   Source: {llvm_src_dir}")
    print(f"   Build: {llvm_build_dir}")
    print(f"   Install: {llvm_install_dir}")
    print()
    print("Next steps:")
    print("   1. Build LLVM:")
    print("      python scripts/build_llvm.py")
    print()
    print("   2. Build PawLang:")
    print("      zig build")
    print()
    print("   3. Test LLVM backend:")
    if platform.system() == "Windows":
        print("      .\\zig-out\\bin\\pawc.exe examples\\hello.paw --backend=llvm")
    else:
        print("      ./zig-out/bin/pawc examples/hello.paw --backend=llvm")
    print()
    
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}")
        sys.exit(1)

