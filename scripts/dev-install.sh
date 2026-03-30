#!/bin/bash
# Build and install obs-chalk to OBS for testing.
# Always reconfigures to pick up source file changes (Xcode generator quirk).
# Usage: ./scripts/dev-install.sh

set -euo pipefail

PLUGIN_NAME="obs-chalk"
OBS_PLUGIN_DIR="/Applications/OBS.app/Contents/PlugIns"

# Always reconfigure — Xcode generator doesn't reliably detect new/changed sources
cmake --preset macos 2>&1 | grep -E "error|warning|Configuring done" || true

# Build
echo "Building..."
cmake --build build_macos --config Debug 2>&1 | tail -5

# Install
echo "Installing to OBS..."
rm -rf "$OBS_PLUGIN_DIR/$PLUGIN_NAME.plugin"
cp -r "build_macos/Debug/$PLUGIN_NAME.plugin" "$OBS_PLUGIN_DIR/$PLUGIN_NAME.plugin"

echo "Done. Restart OBS to load the updated plugin."
