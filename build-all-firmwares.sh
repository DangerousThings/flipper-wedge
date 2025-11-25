#!/bin/bash
# Build Contactless HID Device for multiple Flipper Zero firmwares

set -e  # Exit on error

APP_NAME="contactless_hid_device"
APP_DIR="/home/work/contactless hid reader"
DIST_DIR="${APP_DIR}/dist"

# Firmware directories
FIRMWARE_OFFICIAL="/home/work/flipperzero-firmware"
FIRMWARE_UNLEASHED="/home/work/unleashed-firmware"
FIRMWARE_MOMENTUM="/home/work/Momentum-Firmware"
FIRMWARE_ROGUEMASTER="/home/work/roguemaster-firmware"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Create dist directory
mkdir -p "${DIST_DIR}"

build_for_firmware() {
    local fw_name=$1
    local fw_path=$2

    if [ ! -d "$fw_path" ]; then
        echo -e "${RED}⏭️  Skipping ${fw_name}: firmware not found at ${fw_path}${NC}"
        return
    fi

    echo -e "${BLUE}🔨 Building for ${fw_name}...${NC}"

    # Ensure symlink exists
    if [ ! -L "${fw_path}/applications_user/${APP_NAME}" ]; then
        ln -s "${APP_DIR}" "${fw_path}/applications_user/${APP_NAME}"
    fi

    # Build
    cd "${fw_path}"
    ./fbt fap_${APP_NAME}

    # Copy FAP to dist folder
    local fap_file=$(find .fap -name "${APP_NAME}.fap" | head -n 1)
    if [ -f "$fap_file" ]; then
        cp "$fap_file" "${DIST_DIR}/${APP_NAME}_${fw_name}.fap"
        echo -e "${GREEN}✅ Built: ${APP_NAME}_${fw_name}.fap${NC}"
    else
        echo -e "${RED}❌ Failed to find FAP file for ${fw_name}${NC}"
    fi

    echo ""
}

echo "========================================"
echo "Building Contactless HID Device"
echo "========================================"
echo ""

# Build for each firmware
build_for_firmware "official" "$FIRMWARE_OFFICIAL"
build_for_firmware "unleashed" "$FIRMWARE_UNLEASHED"
build_for_firmware "momentum" "$FIRMWARE_MOMENTUM"
build_for_firmware "roguemaster" "$FIRMWARE_ROGUEMASTER"

echo "========================================"
echo -e "${GREEN}Build complete!${NC}"
echo "FAP files saved to: ${DIST_DIR}"
echo "========================================"
ls -lh "${DIST_DIR}"
