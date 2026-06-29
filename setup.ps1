# setup.ps1 - Auto-download raylib if not present
# Downloads raylib 6.0 from GitHub releases to lib/raylib-windows/

param(
    [string]$RaylibVersion = "6.0",
    [string]$RepoUrl = "https://github.com/raysan5/raylib/releases/download/6.0/raylib-6.0_win64_mingw-w64.zip",
    [string]$ZipFile = "raylib.zip"
)

function Write-Step($message) {
    Write-Host "[SETUP] $message" -ForegroundColor Cyan
}

function Write-Debug($message) {
    Write-Host "  [DEBUG] $message" -ForegroundColor Gray
}

function Get-InstalledRaylibVersion {
    param([string]$basePath)
    $headerPath = Join-Path $basePath "lib\raylib-windows\include\raylib.h"
    if (Test-Path $headerPath) {
        $content = Get-Content $headerPath -Raw
        if ($content -match 'RAYLIB_VERSION\s+"(\d+\.\d+)"') {
            return $Matches[1]
        }
    }
    return $null
}

function Write-Err($message) {
    Write-Host "[ERROR] $message" -ForegroundColor Red
}

# Early exit if all libs already exist
$cwd = $PWD.Path
$raylibDir = Join-Path $cwd "lib\raylib-windows"
$tilesonDir = Join-Path $cwd "lib\tileson"
$jsonDir = Join-Path $cwd "lib\json"
$raylibReady = (Test-Path (Join-Path $raylibDir "include\raylib.h")) -and (Test-Path (Join-Path $raylibDir "lib\libraylib.a"))
$raylibInstalledVersion = if ($raylibReady) { Get-InstalledRaylibVersion -basePath $cwd } else { $null }
$raylibVersionMatch = if ($raylibInstalledVersion) { $raylibInstalledVersion -eq $RaylibVersion } else { $false }
$tilesonReady = Test-Path (Join-Path $tilesonDir "tileson.hpp")
$jsonReady = Test-Path (Join-Path $jsonDir "include\nlohmann\json.hpp")

# Check mpv development files (video playback backend)
$mpvDir = Join-Path $cwd "lib\mpv"
$mpvReady = (Test-Path (Join-Path $mpvDir "include\mpv\client.h")) -and
            (Test-Path (Join-Path $mpvDir "lib\mpv.dll.a")) -and
            (Test-Path (Join-Path $mpvDir "bin\mpv-2.dll"))

# Clean up junk files from existing raylib install (if any)
if ($raylibReady) {
    $junkFiles = @("CHANGELOG", "LICENSE", "README.md")
    foreach ($junk in $junkFiles) {
        $junkPath = Join-Path $raylibDir $junk
        if (Test-Path $junkPath) {
            Remove-Item -Path $junkPath -Force -ErrorAction SilentlyContinue
        }
    }
}

if ($raylibReady -and $raylibVersionMatch -and $tilesonReady -and $jsonReady -and $mpvReady) {
    Write-Step "All required libraries already installed" -ForegroundColor Green
    exit 0
}

# Version mismatch — force reinstall
if ($raylibReady -and -not $raylibVersionMatch) {
    Write-Step "raylib version mismatch: installed $raylibInstalledVersion, needed $RaylibVersion. Reinstalling..."
    Remove-Item -Path $raylibDir -Recurse -Force -ErrorAction SilentlyContinue
}

function Install-Raylib() {
    $cwd = $PWD.Path
    $installDir = Join-Path $cwd "lib\raylib-windows"
    $zipPath = Join-Path $cwd $ZipFile
    $tempExtract = Join-Path $cwd "raylib-$($RaylibVersion)_win64_mingw-w64"
    
    Write-Step "Checking for raylib..."
    Write-Debug "Install directory: $installDir"
    
    if ((Test-Path (Join-Path $installDir "include\raylib.h")) -and 
        (Test-Path (Join-Path $installDir "lib\libraylib.a"))) {
        $curVersion = Get-InstalledRaylibVersion -basePath $cwd
        if ($curVersion -eq $RaylibVersion) {
            Write-Step "raylib already installed at $installDir"
            return
        }
        Write-Step "raylib version mismatch at $installDir (installed: $curVersion, needed: $RaylibVersion). Removing..."
        Remove-Item -Path $installDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    
    Write-Step "raylib not found. Downloading raylib $RaylibVersion from GitHub..."
    Write-Debug "URL: $RepoUrl"
    Write-Debug "Download path: $zipPath"
    
    Write-Debug "Downloading..."
    try {
        Invoke-WebRequest -Uri $RepoUrl -OutFile $zipPath -UserAgent "PowerShell"
    } catch {
        Write-Err "Download failed: $_"
        exit 1
    }
    
    if (-not (Test-Path $zipPath)) {
        Write-Err "Download failed - file not created"
        exit 1
    }
    
    Write-Step "Download complete. Extracting..."
    Write-Debug "Extracting to: $cwd"
    
    if (Test-Path $installDir) {
        Write-Debug "Removing existing $installDir"
        Remove-Item -Path $installDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path $tempExtract) {
        Write-Debug "Removing existing $tempExtract"
        Remove-Item -Path $tempExtract -Recurse -Force -ErrorAction SilentlyContinue
    }
    
    try {
        Expand-Archive -Path $zipPath -DestinationPath $cwd -Force
        Write-Debug "Archive extracted successfully"
    } catch {
        Write-Err "Extraction failed: $_"
        exit 1
    }
    
    if (Test-Path $tempExtract) {
        Write-Debug "Moving $tempExtract to $installDir"
        $libDir = Split-Path $installDir
        if (-not (Test-Path $libDir)) {
            New-Item -ItemType Directory -Path $libDir -Force | Out-Null
        }
        Move-Item -Path $tempExtract -Destination $installDir -Force
    } else {
        Write-Err "Extracted folder not found: $tempExtract"
        Write-Debug "Contents of ${cwd}:"
        Get-ChildItem $cwd | ForEach-Object { Write-Debug "  - $($_.Name)" }
        exit 1
    }
    
    Write-Debug "Cleaning up zip file"
    Remove-Item -Path $zipPath -Force -ErrorAction SilentlyContinue

    Write-Debug "Removing junk files (CHANGELOG, LICENSE, README.md)"
    Remove-Item -Path (Join-Path $installDir "CHANGELOG") -Force -ErrorAction SilentlyContinue
    Remove-Item -Path (Join-Path $installDir "LICENSE") -Force -ErrorAction SilentlyContinue
    Remove-Item -Path (Join-Path $installDir "README.md") -Force -ErrorAction SilentlyContinue
    
    $headerPath = Join-Path $installDir "include\raylib.h"
    $libPath = Join-Path $installDir "lib\libraylib.a"
    
    if ((Test-Path $headerPath) -and (Test-Path $libPath)) {
        Write-Step "raylib installed successfully to $installDir" -ForegroundColor Green
        $oldRaylibDir = Join-Path $cwd "lib/raylib"
        if (Test-Path $oldRaylibDir) {
            Remove-Item -Recurse -Force $oldRaylibDir -ErrorAction SilentlyContinue
            Write-Step "Removed old lib/raylib/ directory"
        }
    } else {
        Write-Err "Installation verification failed!"
        Write-Debug "Header exists: $(Test-Path $headerPath)"
        Write-Debug "Library exists: $(Test-Path $libPath)"
        exit 1
    }
}

function Install-Tileson() {
    $cwd = $PWD.Path
    $tilesonDir = Join-Path $cwd "lib\tileson"
    $tilesonFile = Join-Path $tilesonDir "tileson.hpp"
    $tilesonUrl = "https://github.com/SSBMTonberry/tileson/releases/download/v1.4.0/tileson.hpp"

    Write-Step "Checking for tileson..."
    Write-Debug "Tileson path: $tilesonFile"

    if (Test-Path $tilesonFile) {
        Write-Step "tileson.hpp already exists at $tilesonDir"
        return
    }

    Write-Step "tileson.hpp not found. Downloading tileson v1.4.0 from GitHub..."
    Write-Debug "URL: $tilesonUrl"
    Write-Debug "Download path: $tilesonFile"

    if (-not (Test-Path $tilesonDir)) {
        New-Item -ItemType Directory -Path $tilesonDir -Force | Out-Null
    }

    try {
        Invoke-WebRequest -Uri $tilesonUrl -OutFile $tilesonFile -UserAgent "PowerShell"
    } catch {
        Write-Err "Download failed: $_"
        exit 1
    }

    if (Test-Path $tilesonFile) {
        $fileSize = (Get-Item $tilesonFile).Length
        if ($fileSize -gt 100KB) {
            Write-Step "tileson installed successfully to $tilesonDir ($([math]::Round($fileSize/1KB, 1)) KB)" -ForegroundColor Green
        } else {
            Write-Err "Downloaded file too small ($fileSize bytes), likely failed"
            Remove-Item -Path $tilesonFile -Force -ErrorAction SilentlyContinue
            exit 1
        }
    } else {
        Write-Err "Download failed - file not created"
        exit 1
    }
}

function Install-NlohmannJson() {
    $cwd = $PWD.Path
    $jsonDir = Join-Path $cwd "lib\json"
    $jsonTemp = Join-Path $cwd "json-temp"
    $zipFile = Join-Path $cwd "include.zip"
    $jsonUrl = "https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip"

    Write-Step "Checking for nlohmann-json..."
    Write-Debug "Install directory: $jsonDir"

    if (Test-Path (Join-Path $jsonDir "include\nlohmann\json.hpp")) {
        Write-Step "nlohmann-json already installed at $jsonDir"
        return
    }

    Write-Step "nlohmann-json not found. Downloading nlohmann-json v3.12.0 from GitHub..."
    Write-Debug "URL: $jsonUrl"
    Write-Debug "Download path: $zipFile"

    Write-Debug "Downloading..."
    try {
        Invoke-WebRequest -Uri $jsonUrl -OutFile $zipFile -UserAgent "PowerShell"
    } catch {
        Write-Err "Download failed: $_"
        exit 1
    }

    if (-not (Test-Path $zipFile)) {
        Write-Err "Download failed - file not created"
        exit 1
    }

    Write-Step "Download complete. Extracting..."

    if (Test-Path $jsonTemp) {
        Write-Debug "Removing existing $jsonTemp"
        Remove-Item -Path $jsonTemp -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path $jsonDir) {
        Write-Debug "Removing existing $jsonDir"
        Remove-Item -Path $jsonDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    Write-Debug "Extracting to: $jsonTemp"
    try {
        Expand-Archive -Path $zipFile -DestinationPath $jsonTemp -Force
    } catch {
        Write-Err "Extraction failed: $_"
        exit 1
    }

    $sourceDir = Join-Path $jsonTemp "include\nlohmann"
    $destDir = Join-Path $jsonDir "include\nlohmann"

    if (-not (Test-Path $sourceDir)) {
        Write-Err "Extracted nlohmann folder not found at: $sourceDir"
        Write-Debug "Contents of ${jsonTemp}\include:"
        Get-ChildItem (Join-Path $jsonTemp "include") | ForEach-Object { Write-Debug "  - $($_.Name)" }
        exit 1
    }

    Write-Debug "Moving nlohmann headers to $destDir"
    if (-not (Test-Path (Split-Path $destDir))) {
        New-Item -ItemType Directory -Path (Split-Path $destDir) -Force | Out-Null
    }
    Move-Item -Path $sourceDir -Destination $destDir -Force

    Write-Debug "Cleaning up temp files"
    Remove-Item -Path $jsonTemp -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -Path $zipFile -Force -ErrorAction SilentlyContinue

    $headerPath = Join-Path $jsonDir "include\nlohmann\json.hpp"
    if (Test-Path $headerPath) {
        Write-Step "nlohmann-json installed successfully to $jsonDir" -ForegroundColor Green
    } else {
        Write-Err "Installation verification failed!"
        Write-Debug "Header exists: $(Test-Path $headerPath)"
        exit 1
    }
}

function Remove-OldRaylib() {
    $cwd = $PWD.Path
    $oldRaylib = Join-Path $cwd "raylib"

    if (Test-Path $oldRaylib) {
        Write-Step "Removing old bundled raylib folder..."
        Write-Debug "Removing $oldRaylib"
        Remove-Item -Path $oldRaylib -Recurse -Force -ErrorAction SilentlyContinue
        Write-Step "Old raylib folder removed" -ForegroundColor Green
    }
}

function Install-Doctest() {
    $cwd = $PWD.Path
    $doctestDir = Join-Path $cwd "lib\doctest"
    $doctestFile = Join-Path $doctestDir "doctest.h"
    $doctestUrl = "https://github.com/doctest/doctest/releases/download/v2.4.11/doctest.h"

    Write-Step "Checking for doctest..."
    Write-Debug "Doctest path: $doctestFile"

    if (Test-Path $doctestFile) {
        Write-Step "doctest.h already exists at $doctestDir"
        return
    }

    Write-Step "doctest.h not found. Downloading doctest v2.4.11 from GitHub..."
    Write-Debug "URL: $doctestUrl"

    if (-not (Test-Path $doctestDir)) {
        New-Item -ItemType Directory -Path $doctestDir -Force | Out-Null
    }

    try {
        Invoke-WebRequest -Uri $doctestUrl -OutFile $doctestFile -UserAgent "PowerShell"
    } catch {
        Write-Err "Download failed: $_"
        exit 1
    }

    if (Test-Path $doctestFile) {
        $fileSize = (Get-Item $doctestFile).Length
        if ($fileSize -gt 50KB) {
            Write-Step "doctest installed successfully to $doctestDir ($([math]::Round($fileSize/1KB, 1)) KB)" -ForegroundColor Green
        } else {
            Write-Err "Downloaded file too small ($fileSize bytes), likely failed"
            Remove-Item -Path $doctestFile -Force -ErrorAction SilentlyContinue
            exit 1
        }
    } else {
        Write-Err "Download failed - file not created"
        exit 1
    }
}

function Install-Mpv() {
    $cwd = $PWD.Path
    $mpvDir = Join-Path $cwd "lib\mpv"
    $mpvHeader = Join-Path $mpvDir "include\mpv\client.h"
    $mpvLib = Join-Path $mpvDir "libmpv.dll.a"
    $mpvDll = Join-Path $mpvDir "libmpv-2.dll"

    Write-Step "Checking for mpv development files..."
    Write-Debug "MPV directory: $mpvDir"

    if ((Test-Path $mpvHeader) -and (Test-Path $mpvLib) -and (Test-Path $mpvDll)) {
        Write-Step "mpv already installed at $mpvDir"
        return
    }

    # Ensure 7-Zip is available (shinchiro builds are .7z archives)
    $7zPath = Get-Command "7z" -ErrorAction SilentlyContinue
    if (-not $7zPath) {
        Write-Step "7-Zip not found. Installing via scoop..."
        scoop install 7zip
        $7zPath = Get-Command "7z" -ErrorAction SilentlyContinue
        if (-not $7zPath) {
            Write-Err "Failed to install 7-Zip. Install manually (scoop install 7zip) and re-run."
            exit 1
        }
    }

    Write-Step "mpv not found. Resolving latest mpv-dev release from GitHub..."
    Write-Debug "Source: shinchiro/mpv-winbuild-cmake"

    try {
        $releaseData = Invoke-RestMethod -Uri "https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/latest" -UserAgent "PowerShell"
        $asset = $releaseData.assets | Where-Object { $_.name -like "mpv-dev-x86_64-*.7z" } | Select-Object -First 1
        if (-not $asset) {
            Write-Err "No mpv-dev-x86_64 asset found in latest shinchiro release"
            exit 1
        }
        $downloadUrl = $asset.browser_download_url
        Write-Debug "Found: $($asset.name)"
    } catch {
        Write-Err "Failed to fetch mpv release info: $_"
        exit 1
    }

    $zipFile = Join-Path $cwd "mpv-dev.7z"

    Write-Debug "URL: $downloadUrl"

    try {
        Write-Step "Downloading mpv-dev..."
        Invoke-WebRequest -Uri $downloadUrl -OutFile $zipFile -UserAgent "PowerShell" -MaximumRedirection 10
    } catch {
        Write-Err "Download failed: $_"
        exit 1
    }

    if (-not (Test-Path $zipFile)) {
        Write-Err "Download failed - file not created"
        exit 1
    }

    Write-Step "Download complete. Extracting..."
    Write-Debug "Archive: $zipFile"

    if (Test-Path $mpvDir) {
        Remove-Item -Path $mpvDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Path $mpvDir -Force | Out-Null

    # Extract directly to mpvDir (archive has include/ + libmpv.dll.a + libmpv-2.dll at root)
    & $7zPath.Source x "$zipFile" -o"$mpvDir" -y -bso0 -bsp0
    if ($LASTEXITCODE -ne 0) {
        Write-Err "7-Zip extraction failed (exit code: $LASTEXITCODE)"
        exit 1
    }

    Remove-Item -Path $zipFile -Force -ErrorAction SilentlyContinue

    # Verify
    if ((Test-Path $mpvHeader) -and (Test-Path $mpvLib) -and (Test-Path $mpvDll)) {
        Write-Step "mpv installed successfully to $mpvDir" -ForegroundColor Green
    } else {
        Write-Err "Installation verification failed!"
        Write-Debug "  Header ($mpvHeader): $(Test-Path $mpvHeader)"
        Write-Debug "  Library ($mpvLib): $(Test-Path $mpvLib)"
        Write-Debug "  DLL ($mpvDll): $(Test-Path $mpvDll)"
        exit 1
    }
}

Remove-OldRaylib
Install-Raylib
Install-Tileson
Install-NlohmannJson
Install-Doctest
Install-Mpv
