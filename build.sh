#!/bin/bash
# Build Contactless HID Device for a specific firmware

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
        echo ""
        echo "Aliases:"
        echo "  official:     ofw"
        echo "  unleashed:    ul"
        echo "  momentum:     mntm, xtreme, xfw"
        echo "  roguemaster:  rm"
        echo ""
        echo "Default: official"
        exit 1
        ;;
esac

if [ ! -d "$FW_PATH" ]; then
    echo "❌ Error: ${FW_NAME} firmware not found at ${FW_PATH}"
    echo "Clone it with:"
    case $FIRMWARE in
        unleashed|ul)
            echo "  git clone --recursive https://github.com/DarkFlippers/unleashed-firmware.git ${FW_PATH}"
            ;;
        momentum|mntm|xtreme|xfw)
            echo "  git clone --recursive https://github.com/Next-Flip/Momentum-Firmware.git ${FW_PATH}"
            ;;
        roguemaster|rm)
            echo "  git clone --recursive https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git ${FW_PATH}"
            ;;
    esac
    exit 1
fi

# Ensure symlink exists
if [ ! -L "${FW_PATH}/applications_user/${APP_NAME}" ]; then
    echo "Creating symlink..."
    ln -s "${APP_DIR}" "${FW_PATH}/applications_user/${APP_NAME}"
fi

echo "🔨 Building ${APP_NAME} for ${FW_NAME} firmware..."
cd "${FW_PATH}"
./fbt fap_${APP_NAME}

echo ""
echo "✅ Build complete for ${FW_NAME}!"
echo "FAP location: ${FW_PATH}/.fap/${APP_NAME}.fap"
