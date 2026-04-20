#!/bin/bash

# Configuration
PLUGIN_NAME="mouse-monitor-plugin"
BUNDLE_NAME="${PLUGIN_NAME}.plugin"
BINARY_NAME="${PLUGIN_NAME}"
PLUGIN_DEST="${HOME}/Library/Application Support/obs-studio/plugins"
BUILD_DIR="build_macos/rundir/Release"

echo "=== OBS Plugin Deployment Tool (macOS) ==="

# 1. Locate Plugin
if [ ! -d "${BUILD_DIR}/${BUNDLE_NAME}" ]; then
    echo "Error: Could not find ${BUNDLE_NAME} in ${BUILD_DIR}"
    echo "Please ensure you have built the Release configuration."
    exit 1
fi

echo "Found plugin at: ${BUILD_DIR}/${BUNDLE_NAME}"

# 2. Create Destination
mkdir -p "${PLUGIN_DEST}"

# 3. Clean existing
if [ -d "${PLUGIN_DEST}/${BUNDLE_NAME}" ]; then
    echo "Removing existing plugin installation..."
    rm -rf "${PLUGIN_DEST}/${BUNDLE_NAME}"
fi

# 4. Copy Bundle
echo "Installing plugin to: ${PLUGIN_DEST}"
cp -R "${BUILD_DIR}/${BUNDLE_NAME}" "${PLUGIN_DEST}/"

# 5. Fix Library Linkage (Homebrew -> @rpath)
BINARY_PATH="${PLUGIN_DEST}/${BUNDLE_NAME}/Contents/MacOS/${BINARY_NAME}"
echo "Fixing library linkage for: ${BINARY_PATH}"

# List of Qt libraries to fix
QT_LIBS=("QtCore" "QtGui" "QtWidgets")
for LIB in "${QT_LIBS[@]}"; do
    OLD_PATH="/opt/homebrew/opt/qtbase/lib/${LIB}.framework/Versions/A/${LIB}"
    NEW_PATH="@rpath/${LIB}.framework/Versions/A/${LIB}"
    
    # Check if the library is actually linked
    if otool -L "${BINARY_PATH}" | grep -q "${LIB}.framework"; then
        echo "Updating ${LIB} path..."
        install_name_tool -change "${OLD_PATH}" "${NEW_PATH}" "${BINARY_PATH}"
    fi
done

# 5b. Redirect phantom AGL dependency
if otool -L "${BINARY_PATH}" | grep -q "AGL.framework"; then
    echo "Redirecting phantom AGL dependency..."
    install_name_tool -change @rpath/AGL.framework/AGL /usr/lib/libresolv.9.dylib "${BINARY_PATH}"
fi

# 6. Ad-hoc Codesign (Required for macOS 15+)
echo "Performing ad-hoc code signing..."
codesign -s - --force "${BINARY_PATH}"

# 7. Un-quarantine
echo "Bypassing macOS Gatekeeper (un-quarantine)..."
xattr -dr com.apple.quarantine "${PLUGIN_DEST}/${BUNDLE_NAME}"

echo "=== Deployment Successful ==="
echo "Please restart OBS Studio now."
echo "Diagnostic Info:"
otool -L "${BINARY_PATH}" | grep -E "Qt|obs"
