# Grid-Watcher Windows Setup Script
# Automates installation and setup

param(
    [switch]$SkipNpcap,
    [switch]$SkipBuild,
    [switch]$Help
)

function Show-Help {
    Write-Host @"
Grid-Watcher Windows Setup Script

USAGE:
    .\setup_windows.ps1 [OPTIONS]

OPTIONS:
    -SkipNpcap      Skip Npcap installation check
    -SkipBuild      Skip building the project
    -Help           Show this help message

EXAMPLES:
    # Full setup (recommended first time)
    .\setup_windows.ps1
    
    # Skip Npcap check (if already installed)
    .\setup_windows.ps1 -SkipNpcap
    
    # Only check dependencies
    .\setup_windows.ps1 -SkipBuild

"@
}

if ($Help) {
    Show-Help
    exit 0
}

Write-Host @"

╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║         GRID-WATCHER WINDOWS SETUP SCRIPT v1.0                ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

# ============================================================================
# 1. Check Prerequisites
# ============================================================================

Write-Host "`n[STEP 1] Checking prerequisites..." -ForegroundColor Yellow

# Check CMake
Write-Host "  Checking CMake..." -NoNewline
try {
    $cmake_ver = & cmake --version 2>&1 | Select-String "version"
    Write-Host " ✓ Found: $cmake_ver" -ForegroundColor Green
} catch {
    Write-Host " ✗ NOT FOUND" -ForegroundColor Red
    Write-Host "`n  Install CMake: https://cmake.org/download/" -ForegroundColor Red
    exit 1
}

# Check Clang
Write-Host "  Checking Clang..." -NoNewline
try {
    $clang_ver = & clang++ --version 2>&1 | Select-String "version"
    Write-Host " ✓ Found: $clang_ver" -ForegroundColor Green
} catch {
    Write-Host " ✗ NOT FOUND" -ForegroundColor Red
    Write-Host "`n  Install LLVM/Clang: https://releases.llvm.org/" -ForegroundColor Red
    exit 1
}

# Check Ninja
Write-Host "  Checking Ninja..." -NoNewline
try {
    $ninja_ver = & ninja --version 2>&1
    Write-Host " ✓ Found: v$ninja_ver" -ForegroundColor Green
} catch {
    Write-Host " ✗ NOT FOUND" -ForegroundColor Red
    Write-Host "`n  Install Ninja: https://ninja-build.org/" -ForegroundColor Red
    exit 1
}

# Check Python
Write-Host "  Checking Python..." -NoNewline
try {
    $python_ver = & python --version 2>&1
    Write-Host " ✓ Found: $python_ver" -ForegroundColor Green
} catch {
    Write-Host " ⚠ NOT FOUND (optional for traffic generator)" -ForegroundColor Yellow
}

# ============================================================================
# 2. Check/Install Npcap
# ============================================================================

if (-not $SkipNpcap) {
    Write-Host "`n[STEP 2] Checking Npcap installation..." -ForegroundColor Yellow
    
    $npcap_paths = @(
        "C:\Windows\System32\Npcap\",
        "C:\Windows\System32\wpcap.dll"
    )
    
    $npcap_found = $false
    foreach ($path in $npcap_paths) {
        if (Test-Path $path) {
            $npcap_found = $true
            break
        }
    }
    
    if ($npcap_found) {
        Write-Host "  ✓ Npcap is installed" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Npcap NOT found" -ForegroundColor Red
        Write-Host "`n  Please download and install Npcap:" -ForegroundColor Yellow
        Write-Host "  https://npcap.com/dist/npcap-1.79.exe" -ForegroundColor Cyan
        Write-Host "`n  Installation options:" -ForegroundColor Yellow
        Write-Host "  ✓ WinPcap API-compatible Mode" -ForegroundColor Green
        Write-Host "  ✓ Install Npcap in WinPcap API-compatible Mode" -ForegroundColor Green
        
        $response = Read-Host "`n  Continue anyway? (y/N)"
        if ($response -ne 'y' -and $response -ne 'Y') {
            exit 1
        }
    }
}

# ============================================================================
# 3. Check Npcap SDK
# ============================================================================

Write-Host "`n[STEP 3] Checking Npcap SDK..." -ForegroundColor Yellow

$sdk_paths = @(
    "C:\Npcap-sdk",
    ".\npcap-sdk",
    "$env:USERPROFILE\npcap-sdk"
)

$sdk_found = $false
$sdk_path = ""

foreach ($path in $sdk_paths) {
    if (Test-Path "$path\Include\pcap.h") {
        $sdk_found = $true
        $sdk_path = $path
        break
    }
}

if (-not $sdk_found) {
    Write-Host "  ✗ Npcap SDK NOT found" -ForegroundColor Red
    Write-Host "`n  Download Npcap SDK:" -ForegroundColor Yellow
    Write-Host "  https://npcap.com/dist/npcap-sdk-1.13.zip" -ForegroundColor Cyan
    Write-Host "`n  Extract to one of:" -ForegroundColor Yellow
    foreach ($path in $sdk_paths) {
        Write-Host "  - $path" -ForegroundColor Cyan
    }
    
    Write-Host "`n  Or set NPCAP_SDK environment variable" -ForegroundColor Yellow
    
    $response = Read-Host "`n  Continue without SDK? (builds may fail) (y/N)"
    if ($response -ne 'y' -and $response -ne 'Y') {
        exit 1
    }
} else {
    Write-Host "  ✓ Npcap SDK found at: $sdk_path" -ForegroundColor Green
    $env:NPCAP_SDK = $sdk_path
}

# ============================================================================
# 4. Create Directory Structure
# ============================================================================

Write-Host "`n[STEP 4] Setting up directory structure..." -ForegroundColor Yellow

$dirs = @(
    "build-real",
    "bin",
    "logs",
    "config"
)

foreach ($dir in $dirs) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Write-Host "  ✓ Created: $dir" -ForegroundColor Green
    } else {
        Write-Host "  ✓ Exists: $dir" -ForegroundColor Gray
    }
}

# ============================================================================
# 5. Build Project
# ============================================================================

if (-not $SkipBuild) {
    Write-Host "`n[STEP 5] Building Grid-Watcher..." -ForegroundColor Yellow
    
    Set-Location build-real
    
    Write-Host "  Configuring with CMake..." -ForegroundColor Cyan
    
    $cmake_args = @(
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_CXX_COMPILER=clang++",
        "-DENABLE_LTO=ON",
        "-DENABLE_NATIVE=ON",
        ".."
    )
    
    if ($sdk_path) {
        $cmake_args += "-DNPCAP_SDK_DIR=$sdk_path"
    }
    
    & cmake @cmake_args
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`n  ✗ CMake configuration failed!" -ForegroundColor Red
        Set-Location ..
        exit 1
    }
    
    Write-Host "  Building..." -ForegroundColor Cyan
    & ninja
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`n  ✗ Build failed!" -ForegroundColor Red
        Set-Location ..
        exit 1
    }
    
    Write-Host "  ✓ Build successful!" -ForegroundColor Green
    Set-Location ..
}

# ============================================================================
# 6. Verify Build
# ============================================================================

Write-Host "`n[STEP 6] Verifying build..." -ForegroundColor Yellow

$executables = @(
    "bin\grid_watcher.exe",
    "bin\grid_watcher_demo.exe",
    "bin\grid_watcher_benchmark.exe"
)

$all_found = $true
foreach ($exe in $executables) {
    if (Test-Path $exe) {
        Write-Host "  ✓ Found: $exe" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Missing: $exe" -ForegroundColor Red
        $all_found = $false
    }
}

if (-not $all_found) {
    Write-Host "`n  ⚠ Some executables are missing. Build may have failed." -ForegroundColor Yellow
}

# ============================================================================
# 7. Setup Complete
# ============================================================================

Write-Host @"

╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║                  SETUP COMPLETE! ✓                            ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Green

Write-Host "QUICK START:" -ForegroundColor Yellow
Write-Host @"

  1. List network interfaces:
     .\bin\grid_watcher.exe --list-interfaces

  2. Start capturing (requires Admin):
     .\bin\grid_watcher.exe --interface "Ethernet"

  3. Generate test traffic:
     python scripts\traffic_gen.py

  4. Open web dashboard:
     - Start API server: .\bin\api_server.exe
     - Open browser: http://localhost:8080/web/dashboard.html

"@ -ForegroundColor Cyan

Write-Host "NOTES:" -ForegroundColor Yellow
Write-Host "  • Run PowerShell as Administrator for packet capture" -ForegroundColor Gray
Write-Host "  • Firewall may block traffic - add exceptions if needed" -ForegroundColor Gray
Write-Host "  • Check logs/ folder for detailed logs" -ForegroundColor Gray

Write-Host "`nPress any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")