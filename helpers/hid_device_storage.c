#include "hid_device_storage.h"

static Storage* hid_device_open_storage() {
    return furi_record_open(RECORD_STORAGE);
}

static void hid_device_close_storage() {
    furi_record_close(RECORD_STORAGE);
}

static void hid_device_close_config_file(FlipperFormat* file) {
    if(file == NULL) return;
    flipper_format_file_close(file);
    flipper_format_free(file);
}

void hid_device_save_settings(void* context) {
    HidDevice* app = context;

    FURI_LOG_D(TAG, "Saving Settings");
    Storage* storage = hid_device_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);

    // Overwrite wont work, so delete first
    if(storage_file_exists(storage, HID_DEVICE_SETTINGS_SAVE_PATH)) {
        storage_simply_remove(storage, HID_DEVICE_SETTINGS_SAVE_PATH);
    }

    // Open File, create if not exists
    if(!storage_common_stat(storage, HID_DEVICE_SETTINGS_SAVE_PATH, NULL) == FSE_OK) {
        FURI_LOG_D(
            TAG, "Config file %s is not found. Will create new.", HID_DEVICE_SETTINGS_SAVE_PATH);
        if(storage_common_stat(storage, CONFIG_FILE_DIRECTORY_PATH, NULL) == FSE_NOT_EXIST) {
            FURI_LOG_D(
                TAG, "Directory %s doesn't exist. Will create new.", CONFIG_FILE_DIRECTORY_PATH);
            if(!storage_simply_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH)) {
                FURI_LOG_E(TAG, "Error creating directory %s", CONFIG_FILE_DIRECTORY_PATH);
            }
        }
    }

    if(!flipper_format_file_open_new(fff_file, HID_DEVICE_SETTINGS_SAVE_PATH)) {
        //totp_close_config_file(fff_file);
        FURI_LOG_E(TAG, "Error creating new file %s", HID_DEVICE_SETTINGS_SAVE_PATH);
        hid_device_close_storage();
        return;
    }

    // Store Settings
    flipper_format_write_header_cstr(
        fff_file, HID_DEVICE_SETTINGS_HEADER, HID_DEVICE_SETTINGS_FILE_VERSION);
    flipper_format_write_string_cstr(
        fff_file, HID_DEVICE_SETTINGS_KEY_DELIMITER, app->delimiter);
    flipper_format_write_bool(
        fff_file, HID_DEVICE_SETTINGS_KEY_APPEND_ENTER, &app->append_enter, 1);
    uint32_t mode = app->mode;
    flipper_format_write_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_MODE, &mode, 1);
    flipper_format_write_bool(
        fff_file, HID_DEVICE_SETTINGS_KEY_BT_ENABLED, &app->bt_enabled, 1);
    flipper_format_write_bool(
        fff_file, HID_DEVICE_SETTINGS_KEY_USB_DEBUG, &app->usb_debug_mode, 1);

    if(!flipper_format_rewind(fff_file)) {
        hid_device_close_config_file(fff_file);
        FURI_LOG_E(TAG, "Rewind error");
        hid_device_close_storage();
        return;
    }

    hid_device_close_config_file(fff_file);
    hid_device_close_storage();
}

void hid_device_read_settings(void* context) {
    HidDevice* app = context;
    Storage* storage = hid_device_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);

    if(storage_common_stat(storage, HID_DEVICE_SETTINGS_SAVE_PATH, NULL) != FSE_OK) {
        hid_device_close_config_file(fff_file);
        hid_device_close_storage();
        return;
    }
    uint32_t file_version;
    FuriString* temp_str = furi_string_alloc();

    if(!flipper_format_file_open_existing(fff_file, HID_DEVICE_SETTINGS_SAVE_PATH)) {
        FURI_LOG_E(TAG, "Cannot open file %s", HID_DEVICE_SETTINGS_SAVE_PATH);
        hid_device_close_config_file(fff_file);
        hid_device_close_storage();
        furi_string_free(temp_str);
        return;
    }

    if(!flipper_format_read_header(fff_file, temp_str, &file_version)) {
        FURI_LOG_E(TAG, "Missing Header Data");
        hid_device_close_config_file(fff_file);
        hid_device_close_storage();
        furi_string_free(temp_str);
        return;
    }

    furi_string_free(temp_str);

    if(file_version < HID_DEVICE_SETTINGS_FILE_VERSION) {
        FURI_LOG_I(TAG, "old config version, will be removed.");
        hid_device_close_config_file(fff_file);
        hid_device_close_storage();
        return;
    }

    FuriString* delimiter_str = furi_string_alloc();
    if(flipper_format_read_string(fff_file, HID_DEVICE_SETTINGS_KEY_DELIMITER, delimiter_str)) {
        strncpy(app->delimiter, furi_string_get_cstr(delimiter_str), HID_DEVICE_DELIMITER_MAX_LEN - 1);
        app->delimiter[HID_DEVICE_DELIMITER_MAX_LEN - 1] = '\0';
    }
    furi_string_free(delimiter_str);

    flipper_format_read_bool(
        fff_file, HID_DEVICE_SETTINGS_KEY_APPEND_ENTER, &app->append_enter, 1);

    uint32_t mode = HidDeviceModeNfc;  // Default to NFC mode
    if(flipper_format_read_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_MODE, &mode, 1)) {
        // Validate mode is within valid range
        if(mode < HidDeviceModeCount) {
            app->mode = mode;
        }
    }

    // Read BT enabled setting (default to true for backward compatibility)
    bool bt_enabled = true;
    if(flipper_format_read_bool(fff_file, HID_DEVICE_SETTINGS_KEY_BT_ENABLED, &bt_enabled, 1)) {
        app->bt_enabled = bt_enabled;
    }

    // Read USB debug mode setting (default to false)
    bool usb_debug = false;
    if(flipper_format_read_bool(fff_file, HID_DEVICE_SETTINGS_KEY_USB_DEBUG, &usb_debug, 1)) {
        app->usb_debug_mode = usb_debug;
    }

    flipper_format_rewind(fff_file);

    hid_device_close_config_file(fff_file);
    hid_device_close_storage();
}
