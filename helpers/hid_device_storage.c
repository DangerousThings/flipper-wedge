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
    uint32_t mode_startup = app->mode_startup_behavior;
    flipper_format_write_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_MODE_STARTUP, &mode_startup, 1);
    uint32_t output_mode = app->output_mode;
    flipper_format_write_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_OUTPUT_MODE, &output_mode, 1);
    // Note: USB Debug Mode no longer saved (deprecated in favor of Output selector)
    uint32_t vibration = app->vibration_level;
    flipper_format_write_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_VIBRATION, &vibration, 1);
    flipper_format_write_bool(fff_file, HID_DEVICE_SETTINGS_KEY_LOG_TO_SD, &app->log_to_sd, 1);

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

    uint32_t saved_mode = HidDeviceModeNfc;  // Default to NFC mode
    flipper_format_read_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_MODE, &saved_mode, 1);

    // Read mode startup behavior (default to Remember)
    uint32_t mode_startup = HidDeviceModeStartupRemember;
    if(flipper_format_read_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_MODE_STARTUP, &mode_startup, 1)) {
        // Validate mode startup is within valid range
        if(mode_startup < HidDeviceModeStartupCount) {
            app->mode_startup_behavior = (HidDeviceModeStartup)mode_startup;
        }
    }

    // Apply mode startup behavior logic
    if(app->mode_startup_behavior == HidDeviceModeStartupRemember) {
        // Use saved mode (if valid)
        if(saved_mode < HidDeviceModeCount) {
            app->mode = (HidDeviceMode)saved_mode;
        }
    } else {
        // Use default mode based on startup behavior
        switch(app->mode_startup_behavior) {
            case HidDeviceModeStartupDefaultNfc:
                app->mode = HidDeviceModeNfc;
                break;
            case HidDeviceModeStartupDefaultRfid:
                app->mode = HidDeviceModeRfid;
                break;
            case HidDeviceModeStartupDefaultNdef:
                app->mode = HidDeviceModeNdef;
                break;
            case HidDeviceModeStartupDefaultNfcRfid:
                app->mode = HidDeviceModeNfcThenRfid;
                break;
            case HidDeviceModeStartupDefaultRfidNfc:
                app->mode = HidDeviceModeRfidThenNfc;
                break;
            default:
                app->mode = HidDeviceModeNfc;
                break;
        }
    }

    // Read output mode setting (default to USB)
    // Note: We removed "Both" mode. Old settings migration:
    // Old 0 (USB) -> New 0 (USB)
    // Old 1 (Both) -> New 0 (USB) - default to USB for backward compat
    // Old 2 (BLE) -> New 1 (BLE)
    uint32_t output_mode = HidDeviceOutputUsb;  // Default to USB
    if(flipper_format_read_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_OUTPUT_MODE, &output_mode, 1)) {
        // Migrate old "Both" (1) to USB, old BLE (2) to new BLE (1)
        if(output_mode == 0) {
            // Old USB -> New USB
            app->output_mode = HidDeviceOutputUsb;
        } else if(output_mode == 1) {
            // Old "Both" -> New USB (safer default)
            app->output_mode = HidDeviceOutputUsb;
        } else if(output_mode == 2) {
            // Old BLE -> New BLE (now index 1)
            app->output_mode = HidDeviceOutputBle;
        } else {
            // Invalid value - default to USB
            FURI_LOG_W(TAG, "Invalid output mode %lu, defaulting to USB", output_mode);
            app->output_mode = HidDeviceOutputUsb;
        }
    }

    // Final validation: ensure output_mode is within valid range
    if(app->output_mode >= HidDeviceOutputCount) {
        FURI_LOG_E(TAG, "Output mode %d out of range, forcing to USB", app->output_mode);
        app->output_mode = HidDeviceOutputUsb;
    }

    // Read USB debug mode setting (backward compatibility only - no longer used)
    // Old configs may have this setting; we read it to avoid errors but don't apply it
    bool usb_debug = false;
    flipper_format_read_bool(fff_file, HID_DEVICE_SETTINGS_KEY_USB_DEBUG, &usb_debug, 1);
    // Note: usb_debug_mode is deprecated; Output selector (USB/BLE/Both) replaces this functionality

    // Read vibration level setting (default to Medium for backward compatibility)
    uint32_t vibration = HidDeviceVibrationMedium;
    if(flipper_format_read_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_VIBRATION, &vibration, 1)) {
        // Validate vibration level is within valid range
        if(vibration < HidDeviceVibrationCount) {
            app->vibration_level = (HidDeviceVibration)vibration;
        }
    }

    // Read log to SD setting (default to OFF for privacy/performance)
    flipper_format_read_bool(fff_file, HID_DEVICE_SETTINGS_KEY_LOG_TO_SD, &app->log_to_sd, 1);

    flipper_format_rewind(fff_file);

    hid_device_close_config_file(fff_file);
    hid_device_close_storage();
}
