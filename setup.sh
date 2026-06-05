#!/bin/bash
set -euo pipefail

# Configuration
RAYLIB_VERSION="6.0"
TILESON_VERSION="v1.4.0"
JSON_VERSION="v3.12.0"

# Colors
STEP_COLOR='\033[0;36m'
DEBUG_COLOR='\033[0;90m'
ERR_COLOR='\033[0;31m'
RESET='\033[0m'

# Logging functions
write_step() {
  echo -e "${STEP_COLOR}[SETUP] $1${RESET}"
}

write_debug() {
  echo -e "${DEBUG_COLOR}  [DEBUG] $1${RESET}"
}

write_err() {
  echo -e "${ERR_COLOR}[ERROR] $1${RESET}"
}

get_installed_raylib_version() {
  local header="lib/raylib/include/raylib.h"
  if [[ -f "$header" ]]; then
    grep -oP 'RAYLIB_VERSION\s+"\K[0-9]+\.[0-9]+' "$header" 2>/dev/null || echo ""
  fi
}

# Download function (curl or wget)
download_file() {
  local url="$1"
  local output="$2"
  write_debug "Downloading $url to $output"
  if command -v curl &> /dev/null; then
    curl -L -o "$output" "$url" --user-agent "bash-script"
  elif command -v wget &> /dev/null; then
    wget -O "$output" "$url" --user-agent "bash-script"
  else
    write_err "No download tool found (curl or wget required)"
    exit 1
  fi
}

# OS Detection
OS_TYPE=$(uname -s)
case "$OS_TYPE" in
  Linux)
    RAYLIB_URL="${RAYLIB_URL:-https://github.com/raysan5/raylib/releases/download/${RAYLIB_VERSION}/raylib-${RAYLIB_VERSION}_linux_amd64.tar.gz}"
    RAYLIB_ARCHIVE="raylib.tar.gz"
    RAYLIB_EXTRACT_DIR="raylib-${RAYLIB_VERSION}_linux_amd64"
    ;;
  Darwin)
    RAYLIB_URL="${RAYLIB_URL:-https://github.com/raysan5/raylib/releases/download/${RAYLIB_VERSION}/raylib-${RAYLIB_VERSION}_macos.tar.gz}"
    RAYLIB_ARCHIVE="raylib.tar.gz"
    RAYLIB_EXTRACT_DIR="raylib-${RAYLIB_VERSION}_macos"
    ;;
  MINGW*|MSYS*|CYGWIN*)
    RAYLIB_URL="${RAYLIB_URL:-https://github.com/raysan5/raylib/releases/download/${RAYLIB_VERSION}/raylib-${RAYLIB_VERSION}_win64_mingw-w64.zip}"
    RAYLIB_ARCHIVE="raylib.zip"
    RAYLIB_EXTRACT_DIR="raylib-${RAYLIB_VERSION}_win64_mingw-w64"
    ;;
  *)
    write_err "Unsupported OS: $OS_TYPE"
    exit 1
    ;;
esac

TILESON_URL="https://github.com/SSBMTonberry/tileson/releases/download/${TILESON_VERSION}/tileson.hpp"
JSON_URL="https://github.com/nlohmann/json/releases/download/${JSON_VERSION}/include.zip"

# Clean junk files from existing raylib install
if [[ -d "lib/raylib" ]]; then
  write_debug "Cleaning junk files from existing raylib install"
  rm -f "lib/raylib/CHANGELOG" "lib/raylib/LICENSE" "lib/raylib/README.md"
fi

# Check if all dependencies are already installed
INSTALLED_RAYLIB_VER=""
if [[ -f "lib/raylib/include/raylib.h" && -f "lib/raylib/lib/libraylib.a" ]]; then
  INSTALLED_RAYLIB_VER=$(get_installed_raylib_version)
fi
if [[ -n "$INSTALLED_RAYLIB_VER" && "$INSTALLED_RAYLIB_VER" != "$RAYLIB_VERSION" ]]; then
  write_step "raylib version mismatch: installed $INSTALLED_RAYLIB_VER, needed $RAYLIB_VERSION. Reinstalling..."
  rm -rf "lib/raylib"
fi
if [[ -f "lib/raylib/include/raylib.h" && -f "lib/raylib/lib/libraylib.a" && "$INSTALLED_RAYLIB_VER" == "$RAYLIB_VERSION" && -f "lib/tileson/tileson.hpp" && -f "lib/json/include/nlohmann/json.hpp" ]]; then
  write_step "All required libraries already installed"
  exit 0
fi

# Remove old raylib folder (root/raylib)
remove_old_raylib() {
  local old_raylib="raylib"
  if [[ -d "$old_raylib" ]]; then
    write_step "Removing old bundled raylib folder..."
    write_debug "Removing $old_raylib"
    rm -rf "$old_raylib"
    write_step "Old raylib folder removed"
  fi
}

# Install raylib
install_raylib() {
  local install_dir="lib/raylib"
  local zip_path="$RAYLIB_ARCHIVE"
  local temp_extract="$RAYLIB_EXTRACT_DIR"

  write_step "Checking for raylib..."
  write_debug "Install directory: $install_dir"

  # Check if already installed
  if [[ -f "$install_dir/include/raylib.h" && -f "$install_dir/lib/libraylib.a" ]]; then
    local cur_ver
    cur_ver=$(get_installed_raylib_version)
    if [[ "$cur_ver" == "$RAYLIB_VERSION" ]]; then
      write_step "raylib already installed at $install_dir"
      return
    fi
    write_step "raylib version mismatch at $install_dir (installed: $cur_ver, needed: $RAYLIB_VERSION). Removing..."
    rm -rf "$install_dir"
  fi

  write_step "raylib not found. Downloading raylib $RAYLIB_VERSION from GitHub..."
  write_debug "URL: $RAYLIB_URL"
  write_debug "Download path: $zip_path"

  download_file "$RAYLIB_URL" "$zip_path"

  if [[ ! -f "$zip_path" ]]; then
    write_err "Download failed - file not created"
    exit 1
  fi

  write_step "Download complete. Extracting..."
  write_debug "Extracting to: $temp_extract"

  # Clean up existing dirs
  if [[ -d "$install_dir" ]]; then
    write_debug "Removing existing $install_dir"
    rm -rf "$install_dir"
  fi
  if [[ -d "$temp_extract" ]]; then
    write_debug "Removing existing $temp_extract"
    rm -rf "$temp_extract"
  fi

  # Extract archive
  write_debug "Extracting archive..."
  if [[ "$zip_path" == *.zip ]]; then
    unzip -o "$zip_path" -d . || { write_err "Extraction failed"; exit 1; }
  else
    tar -xzf "$zip_path" || { write_err "Extraction failed"; exit 1; }
  fi

  # Move to install dir
  if [[ -d "$temp_extract" ]]; then
    write_debug "Moving $temp_extract to $install_dir"
    mkdir -p "$(dirname "$install_dir")"
    mv "$temp_extract" "$install_dir"
  else
    write_err "Extracted folder not found: $temp_extract"
    write_debug "Contents of current directory:"
    ls -la
    exit 1
  fi

  # Clean up junk files
  write_debug "Removing junk files (CHANGELOG, LICENSE, README.md)"
  rm -f "$install_dir/CHANGELOG" "$install_dir/LICENSE" "$install_dir/README.md"

  # Verify installation
  if [[ -f "$install_dir/include/raylib.h" && -f "$install_dir/lib/libraylib.a" ]]; then
    write_step "raylib installed successfully to $install_dir"
  else
    write_err "Installation verification failed!"
    write_debug "Header exists: $([[ -f "$install_dir/include/raylib.h" ]] && echo "yes" || echo "no")"
    write_debug "Library exists: $([[ -f "$install_dir/lib/libraylib.a" ]] && echo "yes" || echo "no")"
    exit 1
  fi

  # Clean up archive
  rm -f "$zip_path"
}

# Install tileson
install_tileson() {
  local tileson_dir="lib/tileson"
  local tileson_file="$tileson_dir/tileson.hpp"

  write_step "Checking for tileson..."
  write_debug "Tileson path: $tileson_file"

  if [[ -f "$tileson_file" ]]; then
    write_step "tileson.hpp already exists at $tileson_dir"
    return
  fi

  write_step "tileson.hpp not found. Downloading tileson $TILESON_VERSION from GitHub..."
  write_debug "URL: $TILESON_URL"
  write_debug "Download path: $tileson_file"

  mkdir -p "$tileson_dir"
  download_file "$TILESON_URL" "$tileson_file"

  if [[ -f "$tileson_file" ]]; then
    # Get file size (cross-platform)
    local file_size
    if command -v stat &> /dev/null; then
      file_size=$(stat -c%s "$tileson_file" 2>/dev/null || stat -f%z "$tileson_file" 2>/dev/null)
    else
      file_size=$(wc -c < "$tileson_file")
    fi
    if [[ $file_size -gt 102400 ]]; then # 100KB
      write_step "tileson installed successfully to $tileson_dir ($(($file_size / 1024)) KB)"
    else
      write_err "Downloaded file too small ($file_size bytes), likely failed"
      rm -f "$tileson_file"
      exit 1
    fi
  else
    write_err "Download failed - file not created"
    exit 1
  fi
}

# Install nlohmann-json
install_nlohmann_json() {
  local json_dir="lib/json"
  local json_temp="json-temp"
  local zip_file="include.zip"

  write_step "Checking for nlohmann-json..."
  write_debug "Install directory: $json_dir"

  if [[ -f "$json_dir/include/nlohmann/json.hpp" ]]; then
    write_step "nlohmann-json already installed at $json_dir"
    return
  fi

  write_step "nlohmann-json not found. Downloading nlohmann-json $JSON_VERSION from GitHub..."
  write_debug "URL: $JSON_URL"
  write_debug "Download path: $zip_file"

  download_file "$JSON_URL" "$zip_file"

  if [[ ! -f "$zip_file" ]]; then
    write_err "Download failed - file not created"
    exit 1
  fi

  write_step "Download complete. Extracting..."

  # Clean up existing dirs
  if [[ -d "$json_temp" ]]; then
    write_debug "Removing existing $json_temp"
    rm -rf "$json_temp"
  fi
  if [[ -d "$json_dir" ]]; then
    write_debug "Removing existing $json_dir"
    rm -rf "$json_dir"
  fi

  # Extract
  write_debug "Extracting to: $json_temp"
  unzip -o "$zip_file" -d "$json_temp" || { write_err "Extraction failed"; exit 1; }

  # Move nlohmann headers
  local source_dir="$json_temp/include/nlohmann"
  local dest_dir="$json_dir/include/nlohmann"

  if [[ ! -d "$source_dir" ]]; then
    write_err "Extracted nlohmann folder not found at: $source_dir"
    write_debug "Contents of $json_temp/include:"
    ls -la "$json_temp/include"
    exit 1
  fi

  write_debug "Moving nlohmann headers to $dest_dir"
  mkdir -p "$(dirname "$dest_dir")"
  mv "$source_dir" "$dest_dir"

  # Clean up temp files
  write_debug "Cleaning up temp files"
  rm -rf "$json_temp"
  rm -f "$zip_file"

  # Verify
  if [[ -f "$json_dir/include/nlohmann/json.hpp" ]]; then
    write_step "nlohmann-json installed successfully to $json_dir"
  else
    write_err "Installation verification failed!"
    write_debug "Header exists: $([[ -f "$json_dir/include/nlohmann/json.hpp" ]] && echo "yes" || echo "no")"
    exit 1
  fi
}

# Main execution
remove_old_raylib
install_raylib
install_tileson
install_nlohmann_json
