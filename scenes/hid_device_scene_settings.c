#include "../hid_device.h"
#include <lib/toolbox/value_index.h>

enum SettingsIndex {
    SettingsIndexHeader,
    SettingsIndexOutput,
    SettingsIndexBtPair,
    SettingsIndexDelimiter,
    SettingsIndexAppendEnter,
    SettingsIndexModeStartup,
    SettingsIndexVibration,
};

const char* const on_off_text[2] = {
    "OFF",
    "ON",
};

// Vibration level options
const char* const vibration_text[4] = {
    "OFF",
    "Low",
    "Medium",
    "High",
};

// Mode startup behavior options
const char* const mode_startup_text[6] = {
    "Remember",
    "NFC",
    "RFID",
    "NDEF",
    "NFC+RFID",
    "RFID+NFC",
};

// Output mode options
const char* const output_text[3] = {
    "USB",
    "Both",
    "BLE",
};

// Delimiter options - display names
const char* const delimiter_names[] = {
    "(empty)",
    ":",
    "-",
    "_",
    "space",
    ",",
    ";",
    "|",
};

// Delimiter options - actual values
const char* const delimiter_values[] = {
    "",    // empty
    ":",
    "-",
    "_",
    " ",   // space
    ",",
    ";",
    "|",
};

#define DELIMITER_OPTIONS_COUNT 8

// Helper function to find delimiter index
static uint8_t get_delimiter_index(const char* delimiter) {
    for(uint8_t i = 0; i < DELIMITER_OPTIONS_COUNT; i++) {
        if(strcmp(delimiter, delimiter_values[i]) == 0) {
            return i;
        }
    }
    return 0; // Default to empty if not found
}

static void hid_device_scene_settings_set_delimiter(VariableItem* item) {
    HidDevice* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    // Update delimiter in app
    strncpy(app->delimiter, delimiter_values[index], HID_DEVICE_DELIMITER_MAX_LEN - 1);
    app->delimiter[HID_DEVICE_DELIMITER_MAX_LEN - 1] = '\0';

    // Update display text
    variable_item_set_current_value_text(item, delimiter_names[index]);
}

static void hid_device_scene_settings_set_append_enter(VariableItem* item) {
    HidDevice* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, on_off_text[index]);
    app->append_enter = (index == 1);
}

static void hid_device_scene_settings_set_mode_startup(VariableItem* item) {
    HidDevice* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, mode_startup_text[index]);
    app->mode_startup_behavior = (HidDeviceModeStartup)index;
}

static void hid_device_scene_settings_set_vibration(VariableItem* item) {
    HidDevice* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, vibration_text[index]);
    app->vibration_level = (HidDeviceVibration)index;
}

static void hid_device_scene_settings_set_output(VariableItem* item) {
    HidDevice* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, output_text[index]);
    HidDeviceOutput new_output_mode = (HidDeviceOutput)index;

    // Handle output mode change
    if(new_output_mode != app->output_mode) {
        bool old_usb_enabled = (app->output_mode == HidDeviceOutputUsb || app->output_mode == HidDeviceOutputBoth);
        bool new_usb_enabled = (new_output_mode == HidDeviceOutputUsb || new_output_mode == HidDeviceOutputBoth);
        bool old_bt_enabled = (app->output_mode == HidDeviceOutputBle || app->output_mode == HidDeviceOutputBoth);
        bool new_bt_enabled = (new_output_mode == HidDeviceOutputBle || new_output_mode == HidDeviceOutputBoth);

        // Check if USB status changed (requires app restart)
        // USB can't be dynamically enabled/disabled without restarting the app
        if(old_usb_enabled != new_usb_enabled) {
            // Save the new setting first
            app->output_mode = new_output_mode;
            hid_device_save_settings(app);

            // Show restart prompt
            scene_manager_next_scene(app->scene_manager, HidDeviceSceneUsbDebugRestart);
            return;  // Don't rebuild settings list yet
        }

        app->output_mode = new_output_mode;

        // Start/stop BLE HID if needed (when not requiring restart)
        if(new_bt_enabled && !old_bt_enabled) {
            // Enable BT - start HID
            hid_device_hid_start_bt(app->hid);
        } else if(!new_bt_enabled && old_bt_enabled) {
            // Disable BT - stop HID
            hid_device_hid_stop_bt(app->hid);
        }

        // Rebuild settings list to show/hide "Pair Bluetooth..." option
        scene_manager_handle_custom_event(app->scene_manager, SettingsIndexOutput);
    }
}

static void hid_device_scene_settings_item_callback(void* context, uint32_t index) {
    HidDevice* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void hid_device_scene_settings_on_enter(void* context) {
    HidDevice* app = context;
    VariableItem* item;

    // Keep display backlight on while in settings
    notification_message(app->notification, &sequence_display_backlight_enforce_on);

    // Header with branding (non-interactive)
    item = variable_item_list_add(
        app->variable_item_list,
        "dangerousthings.com",
        0,
        NULL,
        app);

    // Output mode selector
    item = variable_item_list_add(
        app->variable_item_list,
        "Output:",
        HidDeviceOutputCount,
        hid_device_scene_settings_set_output,
        app);
    variable_item_set_current_value_index(item, app->output_mode);
    variable_item_set_current_value_text(item, output_text[app->output_mode]);

    // Pair Bluetooth... action (only if output mode includes BLE)
    bool bt_enabled = (app->output_mode == HidDeviceOutputBle || app->output_mode == HidDeviceOutputBoth);
    if(bt_enabled) {
        // Get BT connection status to show in label
        bool bt_connected = hid_device_hid_is_bt_connected(app->hid);
        const char* bt_status = bt_connected ? "Connected" : "Not paired";

        item = variable_item_list_add(
            app->variable_item_list,
            "Pair Bluetooth...",
            1,
            NULL,  // No change callback
            app);
        variable_item_set_current_value_text(item, bt_status);
    }

    // Byte Delimiter selector
    uint8_t delimiter_index = get_delimiter_index(app->delimiter);
    item = variable_item_list_add(
        app->variable_item_list,
        "Byte Delimiter:",
        DELIMITER_OPTIONS_COUNT,
        hid_device_scene_settings_set_delimiter,
        app);
    variable_item_set_current_value_index(item, delimiter_index);
    variable_item_set_current_value_text(item, delimiter_names[delimiter_index]);

    // Append Enter toggle
    item = variable_item_list_add(
        app->variable_item_list,
        "Append Enter:",
        2,
        hid_device_scene_settings_set_append_enter,
        app);
    variable_item_set_current_value_index(item, app->append_enter ? 1 : 0);
    variable_item_set_current_value_text(item, on_off_text[app->append_enter ? 1 : 0]);

    // Mode startup behavior selector
    item = variable_item_list_add(
        app->variable_item_list,
        "Start Mode:",
        HidDeviceModeStartupCount,
        hid_device_scene_settings_set_mode_startup,
        app);
    variable_item_set_current_value_index(item, app->mode_startup_behavior);
    variable_item_set_current_value_text(item, mode_startup_text[app->mode_startup_behavior]);

    // Vibration level selector
    item = variable_item_list_add(
        app->variable_item_list,
        "Vibration:",
        HidDeviceVibrationCount,
        hid_device_scene_settings_set_vibration,
        app);
    variable_item_set_current_value_index(item, app->vibration_level);
    variable_item_set_current_value_text(item, vibration_text[app->vibration_level]);

    // Set callback for when user clicks on an item
    variable_item_list_set_enter_callback(
        app->variable_item_list,
        hid_device_scene_settings_item_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidDeviceViewIdSettings);
}

bool hid_device_scene_settings_on_event(void* context, SceneManagerEvent event) {
    HidDevice* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SettingsIndexOutput) {
            // Output mode changed - rebuild list to show/hide Pair BT option
            variable_item_list_reset(app->variable_item_list);
            hid_device_scene_settings_on_enter(context);
            consumed = true;
        } else if(event.event == SettingsIndexBtPair) {
            // User clicked "Pair Bluetooth..." - navigate to pairing scene
            scene_manager_next_scene(app->scene_manager, HidDeviceSceneBtPair);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Save settings when leaving
        hid_device_save_settings(app);
    }

    return consumed;
}

void hid_device_scene_settings_on_exit(void* context) {
    HidDevice* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);

    // Return backlight to auto mode
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
}
