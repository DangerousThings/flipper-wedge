#!/bin/bash
# Build Contactless HID Device for a specific firmware

APP_NAME="contactless_hid_device"
APP_DIR="/home/work/contactless hid device"

# Default values
FIRMWARE="official"
BRANCH="release"
TAG=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --branch)
            BRANCH="$2"
            shift 2
            ;;
        --tag)
            TAG="$2"
            shift 2
            ;;
        official|ofw|unleashed|ul|momentum|mntm|xtreme|xfw|roguemaster|rm)
            FIRMWARE="$1"
            shift
            ;;
        *)
            echo "Unknown argument: $1"
            echo ""
            echo "Usage: $0 [firmware] [--branch BRANCH] [--tag TAG]"
            echo ""
            echo "Firmware options:"
            echo "  official:     ofw"
            echo "  unleashed:    ul"
            echo "  momentum:     mntm, xtreme, xfw"
            echo "  roguemaster:  rm"
            echo ""
            echo "Flags:"
            echo "  --branch BRANCH  Git branch to checkout (default: release)"
            echo "  --tag TAG        Git tag to checkout (overrides --branch)"
            echo ""
            echo "Examples:"
            echo "  $0 official"
            echo "  $0 official --branch dev"
            echo "  $0 official --tag 1.2.3"
            exit 1
            ;;
    esac
done

# Git checkout function with version change detection
checkout_firmware_version() {
    local fw_path=$1
    local branch=$2
    local tag=$3
    local fw_name=$4

    echo "📥 Fetching firmware updates..."
    cd "$fw_path"

    # Store current state
    local current_ref=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || git rev-parse --short HEAD)

    # Fetch latest
    git fetch --all --tags --quiet

    # Determine target
    if [ -n "$tag" ]; then
        target="tags/$tag"
        target_name="tag $tag"
    else
        target="origin/$branch"
        target_name="branch $branch"
    fi

    # Validate target exists
    if ! git rev-parse --verify "$target" >/dev/null 2>&1; then
        echo "❌ Error: $target_name not found in ${fw_name} firmware"
        exit 1
    fi

    # Checkout
    git checkout "$target" --quiet

    # Detect change and alert
    local new_ref=$(git rev-parse --short HEAD)
    local old_commit=$(git rev-parse "$current_ref" 2>/dev/null || echo "unknown")
    local new_commit=$(git rev-parse HEAD)

    if [ "$old_commit" != "$new_commit" ]; then
        echo -e "\033[0;31m⚠️  FIRMWARE VERSION CHANGED: $current_ref → $target_name ($(git rev-parse --short HEAD))\033[0m"
    fi

    # Update submodules
    echo "🔄 Updating submodules..."
    git submodule update --init --recursive --quiet
}

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

# Checkout requested firmware version
checkout_firmware_version "$FW_PATH" "$BRANCH" "$TAG" "$FW_NAME"

echo ""
echo "🔨 Building ${APP_NAME} for ${FW_NAME} firmware..."
cd "${FW_PATH}"
./fbt fap_${APP_NAME}

# Determine version tag for output directory
if [ -n "$TAG" ]; then
    VERSION_DIR="$TAG"
else
    VERSION_DIR="$BRANCH"
fi

# Create output directory structure
OUTPUT_DIR="${APP_DIR}/dist/${FIRMWARE}/${VERSION_DIR}"
mkdir -p "${OUTPUT_DIR}"

# Copy FAP to organized location
SOURCE_FAP="${FW_PATH}/build/f7-firmware-D/.extapps/${APP_NAME}.fap"
DEST_FAP="${OUTPUT_DIR}/${APP_NAME}.fap"

if [ -f "$SOURCE_FAP" ]; then
    cp "$SOURCE_FAP" "$DEST_FAP"
    echo ""
    echo "✅ Build complete for ${FW_NAME}!"
    echo "📦 FAP copied to: ${DEST_FAP}"
else
    echo ""
    echo "❌ Error: Build succeeded but FAP not found at ${SOURCE_FAP}"
    exit 1
fi
