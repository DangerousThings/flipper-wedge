#!/bin/bash
# Build and deploy Contactless HID Device to connected Flipper

APP_NAME="contactless_hid_device"
APP_DIR="/home/work/contactless hid reader"

# Default to official firmware
FIRMWARE=${1:-official}

case $FIRMWARE in
    official|ofw)
        FW_PATH="/home/work/flipperzero-firmware"
        FW_NAME="Official"
        ;;
    unleashed|ul)
        FW_PATH="/home/work/unleashed-firmware"
        FW_NAME="Unleashed"
        ;;
    momentum|mntm|xtreme|xfw)
        FW_PATH="/home/work/Momentum-Firmware"
        FW_NAME="Momentum"
        ;;
    roguemaster|rm)
        FW_PATH="/home/work/roguemaster-firmware"
        FW_NAME="RogueMaster"
        ;;
    *)
        echo "Usage: $0 [official|unleashed|momentum|roguemaster]"
        exit 1
        ;;
esac

if [ ! -d "$FW_PATH" ]; then
    echo "❌ Error: ${FW_NAME} firmware not found at ${FW_PATH}"
    exit 1
fi

# Ensure symlink exists
if [ ! -L "${FW_PATH}/applications_user/${APP_NAME}" ]; then
    echo "Creating symlink..."
    ln -s "${APP_DIR}" "${FW_PATH}/applications_user/${APP_NAME}"
fi

echo "🔨 Building and deploying ${APP_NAME} for ${FW_NAME} firmware..."
cd "${FW_PATH}"
./fbt launch APPSRC=applications_user/${APP_NAME}

echo ""
echo "✅ Deployed to Flipper!"
