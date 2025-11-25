#include "../hid_device.h"
#include <lib/toolbox/value_index.h>

enum SettingsIndex {
    SettingsIndexHeader,
    SettingsIndexDelimiter,
    SettingsIndexAppendEnter,
    SettingsIndexUsbDebug,
    SettingsIndexBtEnable,
    SettingsIndexBtPair,
};

const char* const on_off_text[2] = {
    "OFF",
    "ON",
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

static void hid_device_scene_settings_set_usb_debug(VariableItem* item) {
    HidDevice* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, on_off_text[index]);
    app->usb_debug_mode = (index == 1);

    // Note: USB HID changes require app restart, show message if needed
    // For now, just save the setting - it will take effect on next app launch
}

static void hid_device_scene_settings_set_bt_enable(VariableItem* item) {
    HidDevice* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, on_off_text[index]);
    bool new_bt_enabled = (index == 1);

    // Handle BT enable/disable
    if(new_bt_enabled != app->bt_enabled) {
        app->bt_enabled = new_bt_enabled;

        if(app->bt_enabled) {
            // Enable BT - start HID
            hid_device_hid_start_bt(app->hid);
        } else {
            // Disable BT - stop HID
            hid_device_hid_stop_bt(app->hid);
        }

        // Rebuild settings list to show/hide "Pair Bluetooth..." option
        scene_manager_handle_custom_event(app->scene_manager, SettingsIndexBtEnable);
    }
}

static void hid_device_scene_settings_item_callback(void* context, uint32_t index) {
    HidDevice* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void hid_device_scene_settings_on_enter(void* context) {
    HidDevice* app = context;
    VariableItem* item;

    // Header with branding (non-interactive)
    item = variable_item_list_add(
        app->variable_item_list,
        "dangerousthings.com",
        0,
        NULL,
        app);

    // Delimiter selector
    uint8_t delimiter_index = get_delimiter_index(app->delimiter);
    item = variable_item_list_add(
        app->variable_item_list,
        "Delimiter:",
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

    // USB Debug Mode toggle
    item = variable_item_list_add(
        app->variable_item_list,
        "USB Debug Mode:",
        2,
        hid_device_scene_settings_set_usb_debug,
        app);
    variable_item_set_current_value_index(item, app->usb_debug_mode ? 1 : 0);
    variable_item_set_current_value_text(item, on_off_text[app->usb_debug_mode ? 1 : 0]);

    // Enable Bluetooth HID toggle
    item = variable_item_list_add(
        app->variable_item_list,
        "Bluetooth HID:",
        2,
        hid_device_scene_settings_set_bt_enable,
        app);
    variable_item_set_current_value_index(item, app->bt_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, on_off_text[app->bt_enabled ? 1 : 0]);

    // Pair Bluetooth... action (only if BT is enabled)
    if(app->bt_enabled) {
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
        if(event.event == SettingsIndexBtEnable) {
            // BT toggle changed - rebuild list to show/hide Pair BT option
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
}
