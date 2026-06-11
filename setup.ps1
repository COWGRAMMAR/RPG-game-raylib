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

if ($raylibReady -and $raylibVersionMatch -and $tilesonReady -and $jsonReady) {
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

function Install-RaylibMedia() {
    $cwd = $PWD.Path
    $mediaDir = Join-Path $cwd "lib\raylib-media"
    $files = @(
        @{ Name = "raymedia.h";    Url = "https://raw.githubusercontent.com/cloudofoz/raylib-media/f4bd988/src/raymedia.h";    Path = Join-Path $mediaDir "raymedia.h" },
        @{ Name = "rmedia.c";      Url = "https://raw.githubusercontent.com/cloudofoz/raylib-media/f4bd988/src/rmedia.c";    Path = Join-Path $mediaDir "rmedia.c" },
        @{ Name = "FindFFMPEG.cmake"; Url = "https://raw.githubusercontent.com/cloudofoz/raylib-media/f4bd988/CMakeModules/FindFFMPEG.cmake"; Path = Join-Path $mediaDir "FindFFMPEG.cmake" }
    )

    Write-Step "Checking for raylib-media..."
    Write-Debug "Media directory: $mediaDir"

    $allExist = $true
    foreach ($f in $files) {
        if (Test-Path $f.Path) {
            $size = (Get-Item $f.Path).Length
            if ($size -gt 1KB) {
                Write-Debug "  $($f.Name) exists ($([math]::Round($size/1KB,1)) KB)"
            } else {
                $allExist = $false
                Write-Debug "  $($f.Name) exists but is too small ($size bytes), will re-download"
            }
        } else {
            $allExist = $false
            Write-Debug "  $($f.Name) missing"
        }
    }

    if ($allExist) {
        Write-Step "raylib-media already installed at $mediaDir"
        return
    }

    Write-Step "raylib-media not found. Downloading from GitHub..."
    Write-Debug "Pinned to commit f4bd988"

    if (-not (Test-Path $mediaDir)) {
        New-Item -ItemType Directory -Path $mediaDir -Force | Out-Null
    }

    foreach ($f in $files) {
        if ((Test-Path $f.Path) -and ((Get-Item $f.Path).Length -gt 1KB)) {
            Write-Debug "  Skipping $($f.Name) (already exists)"
            continue
        }

        Write-Debug "  Downloading $($f.Name)..."
        try {
            Invoke-WebRequest -Uri $f.Url -OutFile $f.Path -UserAgent "PowerShell"
        } catch {
            Write-Err "Download failed for $($f.Name): $_"
            exit 1
        }

        if (-not (Test-Path $f.Path)) {
            Write-Err "Download failed for $($f.Name) - file not created"
            exit 1
        }
    }

    $missingFiles = @()
    foreach ($f in $files) {
        if (-not (Test-Path $f.Path)) {
            $missingFiles += $f.Name
        }
    }

    if ($missingFiles.Count -eq 0) {
        Write-Step "raylib-media installed successfully to $mediaDir" -ForegroundColor Green
    } else {
        Write-Err "Installation verification failed! Missing: $($missingFiles -join ', ')"
        exit 1
    }
}

function Install-FFmpeg() {
    $cwd = $PWD.Path
    $ffmpegDir = Join-Path $cwd "lib\ffmpeg"
    $zipUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.1-latest-win64-lgpl-shared-8.1.zip"
    $zipFile = Join-Path $cwd "ffmpeg.zip"
    $tempDir = Join-Path $cwd "ffmpeg-temp"

    $avcodecHeader = Join-Path $ffmpegDir "include\libavcodec\avcodec.h"
    $avcodecLib = Join-Path $ffmpegDir "lib\avcodec.lib"
    $avcodecDll = Join-Path $ffmpegDir "bin\avcodec-62.dll"  # FFmpeg 8.1 ABI

    Write-Step "Checking for FFmpeg..."
    Write-Debug "FFmpeg directory: $ffmpegDir"

    if ((Test-Path $avcodecHeader) -and (Test-Path $avcodecLib) -and (Test-Path $avcodecDll)) {
        Write-Step "FFmpeg already installed at $ffmpegDir"
        return
    }

    Write-Step "FFmpeg not found. Resolving latest FFmpeg 8.1 download URL..."

    try {
        $releaseData = Invoke-RestMethod -Uri "https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/latest" -UserAgent "PowerShell"
        $asset = $releaseData.assets | Where-Object { $_.name -like "*win64-lgpl-shared*8.1*" } | Select-Object -First 1
        if ($asset) {
            $zipUrl = $asset.browser_download_url
        } else {
            Write-Warning "No 8.1 LGPL shared asset found, trying any win64-lgpl-shared..."
            $asset = $releaseData.assets | Where-Object { $_.name -like "*win64-lgpl-shared*" } | Select-Object -First 1
            if ($asset) {
                $zipUrl = $asset.browser_download_url
            }
        }
    } catch {
        Write-Warning "Could not resolve latest FFmpeg URL, using default: $_"
    }

    Write-Debug "URL: $zipUrl"

    try {
        Invoke-WebRequest -Uri $zipUrl -OutFile $zipFile -UserAgent "PowerShell" -MaximumRedirection 10
    } catch {
        Write-Err "Download failed: $_"
        # Retry once after 3 seconds
        Start-Sleep -Seconds 3
        try {
            Write-Step "Retrying FFmpeg download..."
            Invoke-WebRequest -Uri $zipUrl -OutFile $zipFile -UserAgent "PowerShell" -MaximumRedirection 10
        } catch {
            Write-Err "Download failed on retry: $_"
            exit 1
        }
    }

    if (-not (Test-Path $zipFile)) {
        Write-Err "Download failed - zip file not created"
        exit 1
    }

    Write-Step "Download complete. Extracting..."

    if (Test-Path $tempDir) {
        Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    try {
        Expand-Archive -Path $zipFile -DestinationPath $tempDir -Force
    } catch {
        Write-Err "Extraction failed: $_"
        exit 1
    }

    Write-Debug "Archive extracted to $tempDir"

    $extractedItems = Get-ChildItem $tempDir
    if ($extractedItems.Count -eq 0) {
        Write-Err "Extraction produced no files"
        exit 1
    }

    $sourceRoot = $null
    foreach ($item in $extractedItems) {
        if ($item.PSIsContainer) {
            $sourceRoot = $item.FullName
            break
        }
    }

    if (-not $sourceRoot) {
        Write-Err "Could not find extracted root folder"
        exit 1
    }

    Write-Debug "Source root: $sourceRoot"

    if (Test-Path $ffmpegDir) {
        Remove-Item -Path $ffmpegDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Path $ffmpegDir -Force | Out-Null

    $subDirs = @("include", "lib", "bin")
    foreach ($sub in $subDirs) {
        $src = Join-Path $sourceRoot $sub
        $dst = Join-Path $ffmpegDir $sub
        if (Test-Path $src) {
            Write-Debug "  Moving $sub to $dst"
            if (Test-Path $dst) {
                Remove-Item -Path $dst -Recurse -Force -ErrorAction SilentlyContinue
            }
            Move-Item -Path $src -Destination $dst -Force
        } else {
            Write-Err "Expected directory not found in extracted archive: $src"
            exit 1
        }
    }

    Write-Debug "Cleaning up temp files"
    Remove-Item -Path $zipFile -Force -ErrorAction SilentlyContinue
    Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue

    if ((Test-Path $avcodecHeader) -and (Test-Path $avcodecLib) -and (Test-Path $avcodecDll)) {
        Write-Step "FFmpeg installed successfully to $ffmpegDir" -ForegroundColor Green
    } else {
        Write-Err "Installation verification failed!"
        Write-Debug "  Header ($avcodecHeader): $(Test-Path $avcodecHeader)"
        Write-Debug "  Library ($avcodecLib): $(Test-Path $avcodecLib)"
        Write-Debug "  DLL ($avcodecDll): $(Test-Path $avcodecDll)"
        exit 1
    }
}

Remove-OldRaylib
Install-Raylib
Install-Tileson
Install-NlohmannJson
Install-Doctest
Install-RaylibMedia
Install-FFmpeg
