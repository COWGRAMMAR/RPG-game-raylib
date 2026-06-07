# Navigate to script root directory (equivalent to %~dp0 in batch)
Set-Location $PSScriptRoot

# Run setup.ps1 with pwsh/powershell fallback
$setupScript = ".\setup.ps1"
if (Test-Path $setupScript) {
    # Check if pwsh is available first
    $pwshExists = Get-Command pwsh.exe -ErrorAction SilentlyContinue
    if ($pwshExists) {
        pwsh.exe -NoProfile -File $setupScript
    } else {
        powershell.exe -NoProfile -File $setupScript
    }
    
    # Stop if setup failed
    if (-not $?) {
        Write-Error "setup.ps1 failed with exit code $LASTEXITCODE"
        exit 1
    }
} else {
    Write-Warning "setup.ps1 not found, skipping setup step"
}

# Clean previous build directory
if (Test-Path .\build) {
    Remove-Item .\build -Recurse -Force
}

# Configure with CMake preset
cmake --preset ninja
if (-not $?) {
    Write-Error "CMake configuration failed with exit code $LASTEXITCODE"
    exit 1
}

# Build with CMake preset
cmake --build --preset ninja
if (-not $?) {
    Write-Error "CMake build failed with exit code $LASTEXITCODE"
    exit 1
}

# Run the game executable
.\build\bin\main.exe
