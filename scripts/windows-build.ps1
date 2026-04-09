# obs-chalk Windows build script
# Run from PowerShell as Administrator (winget installs need elevation)
#
# Usage: .\scripts\windows-build.ps1
#   First run: installs build tools, clones, builds, installs
#   Subsequent runs: pulls latest, rebuilds, installs

param(
    [switch]$SkipInstall,
    [string]$RepoDir = "$env:USERPROFILE\dev\obs-chalk",
    [string]$OBSPluginDir = "$env:ProgramData\obs-studio\plugins\obs-chalk"
)

$ErrorActionPreference = "Stop"

# --- Step 1: Install build tools if needed ---
if (-not $SkipInstall) {
    Write-Host "`n=== Checking build tools ===" -ForegroundColor Cyan

    $vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $hasVS = $false
    if (Test-Path $vsWherePath) {
        $vcTools = & $vsWherePath -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($vcTools) { $hasVS = $true }
    }

    if (-not $hasVS) {
        Write-Host "Installing Visual Studio 2022 Build Tools..." -ForegroundColor Yellow
        winget install Microsoft.VisualStudio.2022.BuildTools `
            --override "--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --passive --wait" `
            --accept-source-agreements --accept-package-agreements
        Write-Host "VS Build Tools installed. You may need to restart PowerShell." -ForegroundColor Green
    } else {
        Write-Host "VS Build Tools already installed." -ForegroundColor Green
    }

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Host "Installing CMake..." -ForegroundColor Yellow
        winget install Kitware.CMake --accept-source-agreements --accept-package-agreements
        # Add CMake to current session PATH
        $env:PATH += ";$env:ProgramFiles\CMake\bin"
        Write-Host "CMake installed." -ForegroundColor Green
    } else {
        Write-Host "CMake already installed." -ForegroundColor Green
    }

    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Write-Host "Installing Git..." -ForegroundColor Yellow
        winget install Git.Git --accept-source-agreements --accept-package-agreements
        $env:PATH += ";$env:ProgramFiles\Git\bin"
        Write-Host "Git installed." -ForegroundColor Green
    } else {
        Write-Host "Git already installed." -ForegroundColor Green
    }
}

# --- Step 2: Clone or pull ---
Write-Host "`n=== Getting source ===" -ForegroundColor Cyan

if (Test-Path "$RepoDir\.git") {
    Write-Host "Repo exists, pulling latest..."
    Push-Location $RepoDir
    git pull origin main
    Pop-Location
} else {
    Write-Host "Cloning repo..."
    $parentDir = Split-Path $RepoDir -Parent
    if (-not (Test-Path $parentDir)) { New-Item -ItemType Directory -Path $parentDir -Force | Out-Null }
    git clone https://github.com/JamesTeague/obs-chalk.git $RepoDir
}

# --- Step 3: Build ---
Write-Host "`n=== Building obs-chalk ===" -ForegroundColor Cyan

Push-Location $RepoDir

# Find vcvarsall.bat to set up MSVC environment
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vsWhere -latest -property installationPath 2>$null
if (-not $vsPath) {
    Write-Host "ERROR: Could not find Visual Studio installation." -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host "Using VS at: $vsPath"

# Configure (downloads OBS deps on first run — ~1GB)
Write-Host "Configuring (first run downloads OBS deps)..." -ForegroundColor Yellow
cmake --preset windows-x64
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: CMake configure failed." -ForegroundColor Red
    Pop-Location
    exit 1
}

# Build
Write-Host "Building..." -ForegroundColor Yellow
cmake --build --preset windows-x64 --config RelWithDebInfo
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Build failed." -ForegroundColor Red
    Pop-Location
    exit 1
}

Pop-Location

# --- Step 4: Install to OBS ---
Write-Host "`n=== Installing to OBS ===" -ForegroundColor Cyan

$dllSource = "$RepoDir\build_x64\RelWithDebInfo\obs-chalk.dll"
if (-not (Test-Path $dllSource)) {
    # Check alternative output paths
    $dllSource = Get-ChildItem -Path "$RepoDir\build_x64" -Filter "obs-chalk.dll" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}

if (-not $dllSource -or -not (Test-Path $dllSource)) {
    Write-Host "ERROR: Could not find obs-chalk.dll in build output." -ForegroundColor Red
    Write-Host "Check build_x64/ for the actual output location." -ForegroundColor Yellow
    exit 1
}

$destDir = "$OBSPluginDir\bin\64bit"
New-Item -ItemType Directory -Path $destDir -Force | Out-Null
Copy-Item $dllSource -Destination "$destDir\obs-chalk.dll" -Force

# Copy data directory if it exists (locale, etc.)
$dataSource = "$RepoDir\data"
if (Test-Path $dataSource) {
    $dataDest = "$OBSPluginDir\data"
    New-Item -ItemType Directory -Path $dataDest -Force | Out-Null
    Copy-Item "$dataSource\*" -Destination $dataDest -Recurse -Force
}

Write-Host "`n=== Done ===" -ForegroundColor Green
Write-Host "Installed to: $destDir\obs-chalk.dll"
Write-Host "Restart OBS to load the plugin."
