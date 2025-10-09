# Compile PawLang programs using local LLVM toolchain
# Usage: .\scripts\compile_with_local_llvm.ps1 <file.paw> [output_name]

param(
    [Parameter(Mandatory=$true)]
    [string]$SourceFile,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputName = "output"
)

$ErrorActionPreference = "Stop"

$PROJECT_ROOT = Split-Path -Parent $PSScriptRoot
$PAWC = Join-Path $PROJECT_ROOT "zig-out\bin\pawc.exe"
$CLANG = Join-Path $PROJECT_ROOT "llvm\install\bin\clang.exe"
$LLC = Join-Path $PROJECT_ROOT "llvm\install\bin\llc.exe"

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "        PawLang + Local LLVM Compilation                       " -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# Check if source file exists
if (-not (Test-Path $SourceFile)) {
    Write-Host "Error: Source file not found: $SourceFile" -ForegroundColor Red
    exit 1
}

# Check if pawc exists
if (-not (Test-Path $PAWC)) {
    Write-Host "Error: PawLang compiler not found: $PAWC" -ForegroundColor Red
    Write-Host ""
    Write-Host "Build it with:" -ForegroundColor Yellow
    Write-Host "   zig build"
    exit 1
}

# Check if local clang exists
if (-not (Test-Path $CLANG)) {
    Write-Host "Error: Local Clang not found: $CLANG" -ForegroundColor Red
    Write-Host ""
    Write-Host "Build LLVM first:" -ForegroundColor Yellow
    Write-Host "   .\scripts\setup_llvm_windows.ps1"
    Write-Host "   .\scripts\build_llvm_local.ps1"
    exit 1
}

Write-Host "Source:  $SourceFile" -ForegroundColor Cyan
Write-Host "Output:  $OutputName.exe" -ForegroundColor Cyan
Write-Host ""

# Step 1: Compile PawLang to LLVM IR
Write-Host "Step 1/2: Compiling PawLang to LLVM IR..." -ForegroundColor Yellow

try {
    & $PAWC $SourceFile --backend=llvm
    
    if ($LASTEXITCODE -ne 0) {
        throw "PawLang compilation failed"
    }
    
    if (-not (Test-Path "output.ll")) {
        throw "LLVM IR file not generated"
    }
    
    Write-Host "Generated output.ll" -ForegroundColor Green
    Write-Host ""
    
    # Step 2: Compile LLVM IR to executable
    Write-Host "Step 2/2: Compiling LLVM IR to executable..." -ForegroundColor Yellow
    
    & $CLANG "output.ll" -o "$OutputName.exe"
    
    if ($LASTEXITCODE -ne 0) {
        throw "LLVM compilation failed"
    }
    
    Write-Host "Generated $OutputName.exe" -ForegroundColor Green
    Write-Host ""
    
    Write-Host "================================================================" -ForegroundColor Green
    Write-Host "Compilation Complete!" -ForegroundColor Green
    Write-Host "================================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Run your program:" -ForegroundColor Cyan
    Write-Host "   .\$OutputName.exe"
    Write-Host ""
    Write-Host "Tools used:" -ForegroundColor Cyan
    Write-Host "   PawLang: $PAWC"
    $clangVersion = & $CLANG --version | Select-Object -First 1
    Write-Host "   Clang: $clangVersion"
    Write-Host ""
    
} catch {
    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Red
    Write-Host "Compilation failed: $_" -ForegroundColor Red
    Write-Host "================================================================" -ForegroundColor Red
    Write-Host ""
    exit 1
}




