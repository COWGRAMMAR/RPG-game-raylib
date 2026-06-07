#!/bin/bash
set -euo pipefail

# One-click build and run script for Linux
# Flow: setup.sh -> cmake configure -> cmake build -> launch game

# Colors (matching setup.sh conventions)
STEP_COLOR='\033[0;36m'
ERR_COLOR='\033[0;31m'
RESET='\033[0m'

write_step() {
  echo -e "${STEP_COLOR}[RUN] $1${RESET}"
}

write_err() {
  echo -e "${ERR_COLOR}[ERROR] $1${RESET}"
}

# Navigate to script root directory
cd "$(dirname "$0")"

# 1. Run setup.sh to ensure dependencies are installed
write_step "Running setup.sh..."
if [ -f "setup.sh" ]; then
  bash setup.sh || { write_err "setup.sh failed"; exit 1; }
else
  write_err "setup.sh not found. Run this script from the project root."
  exit 1
fi

# 2. Clean previous build artifacts
write_step "Cleaning build directory..."
rm -rf build

# 3. Configure with CMake preset
write_step "Configuring CMake (ninja preset)..."
cmake --preset ninja || { write_err "CMake configuration failed"; exit 1; }

# 4. Build the project
write_step "Building project (ninja preset, target: main)..."
cmake --build --preset ninja --target main || { write_err "CMake build failed"; exit 1; }

# 5. Run the game binary
if [ -f "build/bin/main" ]; then
  write_step "Launching game..."
  ./build/bin/main
else
  write_err "Binary not found at build/bin/main. Build may have failed."
  exit 1
fi
