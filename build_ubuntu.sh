#!/usr/bin/env bash
# StudioFros - Tape Delay: build + install on Ubuntu/Debian
# Usage:  ./build_ubuntu.sh            (installs dependencies, builds, copies VST3 to ~/.vst3)
#         SKIP_DEPS=1 ./build_ubuntu.sh   (skip the apt step)
set -e
cd "$(dirname "$0")"

if [ -z "$SKIP_DEPS" ]; then
  sudo apt update
  sudo apt install -y build-essential cmake git libasound2-dev libjack-jackd2-dev \
    libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev \
    libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev libcurl4-openssl-dev
fi

mkdir -p "$HOME/.vst3"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo
echo "== VST3 installed in ~/.vst3 =="
ls -la "$HOME/.vst3" | grep -i studiofros || echo "(not found - check the build output above)"
echo
echo "== Standalone =="
ls -la build/StudioFros_artefacts/Release/Standalone 2>/dev/null || echo "(not found)"
echo
echo "Run the standalone with:"
echo "  \"$PWD/build/StudioFros_artefacts/Release/Standalone/StudioFros - Tape Delay\""
echo "In REAPER: Options > Preferences > Plug-ins > VST > Re-scan"
