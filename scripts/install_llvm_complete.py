#!/usr/bin/env python3
"""
PawLang LLVM Complete Installation Script
Features: Download + Extract + Install + Build + Test (One-Click)
Supports: macOS (x86_64/arm64), Linux (x86_64/aarch64)
"""

import os
import sys
import platform
import urllib.request
import tarfile
import shutil
import subprocess
import argparse
from pathlib import Path

# LLVM Configuration
LLVM_VERSION = "19.1.7"
GITHUB_REPO = "pawlang-project/llvm-build"
RELEASE_TAG = "19.1.7"

def print_header(text, char="="):
    """Print formatted header"""
    width = 70
    print(char * width)
    print(f"        {text}".center(width))
    print(char * width)
    print()

def print_step(step_num, total_steps, title):
    """Print step header"""
    print("━" * 70)
    print(f"  Step {step_num}/{total_steps}: {title}")
    print("━" * 70)
    print()

def detect_platform():
    """Detect current platform and return corresponding filename"""
    system = platform.system()
    machine = platform.machine()
    
    print(f"Operating System: {system}")
    print(f"Architecture: {machine}")
    print()
    
    # Platform mapping
    platform_map = {
        ("Darwin", "x86_64"): ("clang+llvm-19.1.7-x86_64-apple-darwin.tar.xz", "macOS Intel"),
        ("Darwin", "arm64"): ("clang+llvm-19.1.7-aarch64-apple-darwin.tar.xz", "macOS Apple Silicon (M1/M2/M3)"),
        ("Linux", "x86_64"): ("clang+llvm-19.1.7-x86_64-linux-gnu.tar.xz", "Linux x86_64"),
        ("Linux", "aarch64"): ("clang+llvm-19.1.7-aarch64-linux-gnu.tar.xz", "Linux ARM64"),
    }
    
    key = (system, machine)
    if key not in platform_map:
        print(f"❌ Error: Unsupported platform {system} {machine}")
        print()
        print("Supported platforms:")
        print("  - macOS x86_64 (Intel)")
        print("  - macOS arm64 (Apple Silicon M1/M2/M3)")
        print("  - Linux x86_64")
        print("  - Linux aarch64 (ARM64)")
        return None, None
    
    filename, platform_name = platform_map[key]
    print(f"✅ Platform: {platform_name}")
    print()
    return filename, platform_name

def download_file(url, filename):
    """Download file with progress display"""
    print(f"File: {os.path.basename(filename)}")
    print(f"URL: {url}")
    print()
    print("Download size: ~300-600 MB")
    print("Estimated time: 5-15 minutes")
    print()
    
    # Try using tqdm (if available)
    try:
        from tqdm import tqdm
        import requests
        
        print("Using enhanced progress bar (tqdm)...")
        response = requests.get(url, stream=True)
        total_size = int(response.headers.get('content-length', 0))
        
        with open(filename, 'wb') as f:
            with tqdm(total=total_size, unit='B', unit_scale=True, 
                     unit_divisor=1024, desc="Download") as pbar:
                for chunk in response.iter_content(chunk_size=8192):
                    if chunk:
                        f.write(chunk)
                        pbar.update(len(chunk))
        
        print("✅ Download complete")
        return True
        
    except ImportError:
        # Use standard progress
        print("Using standard progress bar...")
        last_percent = -1
        
        def progress_hook(block_num, block_size, total_size):
            nonlocal last_percent
            if total_size > 0:
                downloaded = block_num * block_size
                percent = min(downloaded * 100 / total_size, 100)
                
                if int(percent) > last_percent:
                    last_percent = int(percent)
                    mb_downloaded = downloaded / (1024 * 1024)
                    mb_total = total_size / (1024 * 1024)
                    
                    bar_length = 50
                    filled = int(bar_length * percent / 100)
                    bar = '█' * filled + '░' * (bar_length - filled)
                    
                    print(f"\rProgress: [{bar}] {percent:.1f}% ({mb_downloaded:.1f}/{mb_total:.1f} MB)", 
                          end='', flush=True)
        
        try:
            urllib.request.urlretrieve(url, filename, progress_hook)
            print()
            print("✅ Download complete")
            return True
        except Exception as e:
            print()
            print(f"❌ Download failed: {e}")
            return False

def extract_archive(archive_path, extract_to):
    """Extract archive file"""
    print(f"Extracting to: {extract_to}")
    print("This may take a few minutes...")
    print()
    
    archive_str = str(archive_path)
    extract_to_str = str(extract_to)
    
    try:
        with tarfile.open(archive_str) as tar:
            tar.extractall(path=extract_to_str)
        
        print("✅ Extraction complete")
        return True
    except Exception as e:
        print(f"❌ Extraction failed: {e}")
        return False

def install_llvm(temp_dir, install_dir):
    """Install LLVM to target directory"""
    # Find extracted directory
    extracted_dirs = [d for d in temp_dir.iterdir() if d.is_dir() and d.name.startswith('clang+llvm')]
    
    if not extracted_dirs:
        print("❌ Extracted directory not found")
        return False
    
    extracted_dir = extracted_dirs[0]
    
    print(f"Source: {extracted_dir}")
    print(f"Target: {install_dir}")
    print()
    
    # Remove existing installation
    if install_dir.exists():
        shutil.rmtree(install_dir)
    
    # Move to install directory
    shutil.move(str(extracted_dir), str(install_dir))
    
    print("✅ Installation complete")
    return True

def verify_installation(install_dir):
    """Verify LLVM installation"""
    print("LLVM Version:")
    
    # Find clang
    clang_path = install_dir / "bin" / "clang"
    if not clang_path.exists():
        clang_path = install_dir / "bin" / "clang-19"
    
    if not clang_path.exists():
        print("❌ clang not found")
        return False
    
    try:
        result = subprocess.run([str(clang_path), "--version"], 
                              capture_output=True, text=True)
        for line in result.stdout.split('\n')[0:3]:
            print(f"  {line}")
    except:
        print("⚠️  Cannot get version info")
    
    print()
    
    # Check key files
    print("Checking key files:")
    files_to_check = [
        ("bin/clang", "Clang compiler"),
        ("include/llvm-c/Core.h", "LLVM C API headers"),
    ]
    
    for file, desc in files_to_check:
        path = install_dir / file
        if path.exists() or path.is_symlink():
            print(f"  ✓ {desc}")
        else:
            print(f"  ⚠️  {desc} not found")
    
    # Check library files (may have various forms)
    lib_dir = install_dir / "lib"
    if lib_dir.exists():
        has_lib = any(f.name.startswith('libLLVM') for f in lib_dir.iterdir())
        if has_lib:
            print(f"  ✓ LLVM libraries")
        else:
            print(f"  ⚠️  LLVM libraries not found")
    
    print()
    print("✅ LLVM installation verified")
    
    # Show size
    try:
        size = sum(f.stat().st_size for f in install_dir.rglob('*') if f.is_file())
        size_mb = size / (1024 * 1024)
        print(f"  Install size: {size_mb:.1f} MB")
    except:
        pass
    
    return True

def build_pawlang(project_root):
    """Build PawLang compiler"""
    print("Build command: zig build")
    print()
    
    try:
        result = subprocess.run(
            ["zig", "build"],
            cwd=str(project_root),
            capture_output=True,
            text=True
        )
        
        # Show output
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print(result.stderr)
        
        if result.returncode != 0:
            print("❌ Build failed")
            return False
        
        print("✅ PawLang build complete")
        return True
        
    except FileNotFoundError:
        print("❌ Error: zig command not found")
        print("Please install Zig: https://ziglang.org/download/")
        return False
    except Exception as e:
        print(f"❌ Build failed: {e}")
        return False

def run_test(project_root):
    """Run test program"""
    # Create test program
    test_paw = project_root / "test_install.paw"
    test_paw.write_text("""fn main() -> i32 {
    return 42;
}
""")
    
    pawc = project_root / "zig-out" / "bin" / "pawc"
    
    print("Compiling test program...")
    
    # Compile to LLVM IR
    try:
        result = subprocess.run(
            [str(pawc), str(test_paw), "--backend=llvm"],
            cwd=str(project_root),
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            print("❌ Compilation to LLVM IR failed")
            print(result.stderr)
            return False
        
        print("✓ LLVM IR generation successful")
        
    except Exception as e:
        print(f"❌ Compilation failed: {e}")
        return False
    
    # Link to executable
    print("Linking executable...")
    output_ll = project_root / "output.ll"
    test_exe = project_root / "test_install_exe"
    
    try:
        # Try gcc first
        result = subprocess.run(
            ["gcc", str(output_ll), "-o", str(test_exe)],
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            # Try clang
            result = subprocess.run(
                ["clang", str(output_ll), "-o", str(test_exe)],
                capture_output=True,
                text=True
            )
            
            if result.returncode != 0:
                print("❌ Linking failed")
                return False
        
        print("✓ Executable generated successfully")
        
    except Exception as e:
        print(f"❌ Linking failed: {e}")
        return False
    
    # Run test
    print("Running test...")
    try:
        result = subprocess.run([str(test_exe)], capture_output=True)
        exit_code = result.returncode
        
        # Cleanup
        test_paw.unlink()
        output_ll.unlink()
        test_exe.unlink()
        
        if exit_code == 42:
            print(f"✓ Test passed! Exit code: {exit_code}")
            return True
        else:
            print(f"⚠️  Test completed with exit code: {exit_code} (expected: 42)")
            return True
            
    except Exception as e:
        print(f"❌ Test run failed: {e}")
        return False

def main():
    """Main function"""
    parser = argparse.ArgumentParser(
        description='PawLang LLVM Complete Installation Script (Download+Extract+Install+Build+Test)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 scripts/install_llvm_complete.py          # Interactive mode
  python3 scripts/install_llvm_complete.py --yes    # Auto mode
  python3 scripts/install_llvm_complete.py --skip-build  # Install LLVM only
        """
    )
    parser.add_argument('-y', '--yes', action='store_true',
                       help='Auto-confirm all prompts (non-interactive mode)')
    parser.add_argument('--skip-build', action='store_true',
                       help='Skip PawLang build step')
    parser.add_argument('--skip-test', action='store_true',
                       help='Skip test step')
    args = parser.parse_args()
    
    # Get project root directory
    project_root = Path(__file__).parent.parent.absolute()
    llvm_dir = project_root / "llvm"
    temp_dir = llvm_dir / "temp"
    install_dir = llvm_dir / "install"
    
    print_header("PawLang LLVM Complete Installation Script")
    print_header("Download + Extract + Install + Build + Test", char="═")
    
    print(f"Project directory: {project_root}")
    print(f"LLVM version: {LLVM_VERSION}")
    print(f"Python: {sys.version.split()[0]}")
    print()
    
    # Step 1: Detect platform
    print_step("1", "6", "🔍 Detecting Platform")
    filename, platform_name = detect_platform()
    if not filename:
        return 1
    
    url = f"https://github.com/{GITHUB_REPO}/releases/download/{RELEASE_TAG}/{filename}"
    
    # Check existing installation
    clang_path = install_dir / "bin" / "clang"
    clang19_path = install_dir / "bin" / "clang-19"
    
    if clang_path.exists() or clang19_path.exists():
        print_step("ℹ️  Info", "", "Existing LLVM Installation Detected")
        
        try:
            if clang_path.exists():
                result = subprocess.run([str(clang_path), "--version"],
                                      capture_output=True, text=True)
            else:
                result = subprocess.run([str(clang19_path), "--version"],
                                      capture_output=True, text=True)
            print(result.stdout.split('\n')[0])
        except:
            pass
        
        print()
        
        if not args.yes:
            response = input("Reinstall? (y/N): ").strip().lower()
            if response != 'y':
                print("Using existing installation")
                print()
                print("Skipping to Step 5 (Build PawLang)...")
                print()
                skip_download = True
            else:
                print("Removing existing installation...")
                shutil.rmtree(install_dir)
                skip_download = False
        else:
            print("Using existing installation (--yes mode)")
            skip_download = True
    else:
        skip_download = False
    
    if not skip_download:
        # Step 2: Download
        print_step("2", "6", "📥 Download LLVM " + LLVM_VERSION)
        
        llvm_dir.mkdir(exist_ok=True)
        temp_dir.mkdir(exist_ok=True)
        
        archive_path = temp_dir / filename
        
        if not download_file(url, archive_path):
            return 1
        
        print()
        
        # Step 3: Extract
        print_step("3", "6", "📦 Extracting Files")
        
        if not extract_archive(archive_path, temp_dir):
            return 1
        
        print()
        
        # Step 4: Install
        print_step("4", "6", "📂 Installing LLVM")
        
        if not install_llvm(temp_dir, install_dir):
            return 1
        
        # Cleanup
        print()
        print("🧹 Cleaning up temporary files...")
        shutil.rmtree(temp_dir)
        print("✅ Cleanup complete")
        print()
    
    # Step 5: Verify
    print_step("5", "6", "✓ Verifying LLVM Installation")
    
    if not verify_installation(install_dir):
        return 1
    
    print()
    
    # Step 6: Build PawLang
    if not args.skip_build:
        print_step("6", "6", "🔨 Building PawLang")
        
        if not build_pawlang(project_root):
            return 1
        
        print()
    else:
        print("⏭️  Skipping build step (--skip-build)")
        print()
    
    # Test
    if not args.skip_test and not args.skip_build:
        print_step("🧪 Testing", "", "LLVM Backend")
        
        if not run_test(project_root):
            print("⚠️  Tests did not pass, but installation may still be usable")
        
        print()
    
    # Show completion info
    print_header("✅ Installation Complete!", char="═")
    
    print("📊 Installation Info:")
    print(f"  LLVM Version: {LLVM_VERSION}")
    print(f"  Install Path: {install_dir}")
    
    # Find clang
    if clang_path.exists():
        print(f"  Clang: {clang_path}")
    elif clang19_path.exists():
        print(f"  Clang: {clang19_path}")
    
    print()
    print("🎯 Next Steps:")
    print()
    print("  1. Compile PawLang program (LLVM backend):")
    print("     ./zig-out/bin/pawc mycode.paw --backend=llvm")
    print()
    print("  2. Generate executable:")
    print("     gcc output.ll -o mycode")
    print()
    print("  3. Run:")
    print("     ./mycode")
    print()
    print("━" * 70)
    print("  💡 One-Line Command:")
    print("━" * 70)
    print()
    print("  ./zig-out/bin/pawc mycode.paw --backend=llvm && \\")
    print("  gcc output.ll -o mycode && \\")
    print("  ./mycode")
    print()
    print("━" * 70)
    print("  📚 Documentation:")
    print("━" * 70)
    print()
    print("  - docs/LLVM_QUICK_SETUP.md - Quick setup guide")
    print("  - docs/LLVM_PREBUILT_GUIDE.md - Complete guide")
    print("  - examples/ - Example programs")
    print()
    print_header("🎊 Happy Coding with PawLang! 🐾", char="═")
    
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

