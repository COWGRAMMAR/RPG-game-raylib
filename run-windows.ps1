# run-windows.ps1 - One-click build & run
# Navigate to script root directory (equivalent to %~dp0 in batch)
Set-Location $PSScriptRoot

function Write-Step($message) {
    Write-Host "[RUN] $message" -ForegroundColor Cyan
}

function Write-Err($message) {
    Write-Host "[ERROR] $message" -ForegroundColor Red
}

Write-Step "Starting one-click build and run..."

# Run setup.ps1 with pwsh/powershell fallback
$setupScript = ".\setup.ps1"
if (Test-Path $setupScript) {
    Write-Step "Running setup.ps1..."
    $pwshExists = Get-Command pwsh.exe -ErrorAction SilentlyContinue
    if ($pwshExists) {
        pwsh.exe -NoProfile -File $setupScript
    } else {
        powershell.exe -NoProfile -File $setupScript
    }
    if (-not $?) {
        Write-Err "setup.ps1 failed with exit code $LASTEXITCODE"
        exit 1
    }
} else {
    Write-Warning "setup.ps1 not found, skipping setup step"
}

# Clean previous build directory
if (Test-Path .\build) {
    Write-Step "Cleaning previous build directory..."
    Remove-Item .\build -Recurse -Force
}

# Configure with CMake preset
Write-Step "Configuring with CMake (ninja preset)..."
cmake --preset ninja
if (-not $?) {
    Write-Err "CMake configuration failed with exit code $LASTEXITCODE"
    exit 1
}

# Build with CMake preset
Write-Step "Building with CMake (ninja preset)..."
cmake --build --preset ninja
if (-not $?) {
    Write-Err "CMake build failed with exit code $LASTEXITCODE"
    exit 1
}

# Run the game executable
Write-Step "Build complete. Running game..."
if (Test-Path "build/bin/main.exe") {
    & "build/bin/main.exe"
} else {
    Write-Err "Binary not found: build/bin/main.exe"
    exit 1
}
