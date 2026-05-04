#!/bin/bash
# MonoEye Linux Installer
# Registers the OpenXR API layer locally

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
MANIFEST_SRC="$PROJECT_ROOT/manifest/XR_APILAYER_NOVENDOR_monoeye.json"
LAYER_DIR="$HOME/.local/share/openxr/1/api_layers/explicit.d"
MANIFEST_DEST="$LAYER_DIR/XR_APILAYER_NOVENDOR_monoeye.json"

# Detect build directory
if [ -f "$PROJECT_ROOT/build/XR_APILAYER_NOVENDOR_monoeye.so" ]; then
    SO_PATH="$PROJECT_ROOT/build/XR_APILAYER_NOVENDOR_monoeye.so"
elif [ -f "$PROJECT_ROOT/XR_APILAYER_NOVENDOR_monoeye.so" ]; then
    SO_PATH="$PROJECT_ROOT/XR_APILAYER_NOVENDOR_monoeye.so"
else
    echo "ERROR: XR_APILAYER_NOVENDOR_monoeye.so not found. Please build the project first."
    exit 1
fi

echo "Registering MonoEye OpenXR layer..."
echo "  Source manifest: $MANIFEST_SRC"
echo "  Library path: $SO_PATH"

mkdir -p "$LAYER_DIR"

# Create a copy of the manifest with the absolute path to the SO
sed "s|\"library_path\": \".*\"|\"library_path\": \"$SO_PATH\"|" "$MANIFEST_SRC" > "$MANIFEST_DEST"

echo "Registration complete."
echo "Manifest installed to: $MANIFEST_DEST"
echo ""
echo "To enable MonoEye for games, set:"
echo "  export MONOEYE_ENABLE=1"
echo ""
echo "You can verify installation with: xrdb list (if installed)"
