#include "../hid_device.h"
#include <lib/toolbox/value_index.h>

enum SettingsIndex {
    SettingsIndexDelimiter,
    SettingsIndexAppendEnter,
    SettingsIndexNdefEnabled,
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

static void hid_device_scene_settings_set_ndef_enabled(VariableItem* item) {
    HidDevice* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, on_off_text[index]);
    app->ndef_enabled = (index == 1);
}

static void hid_device_scene_settings_item_callback(void* context, uint32_t index) {
    HidDevice* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void hid_device_scene_settings_on_enter(void* context) {
    HidDevice* app = context;
    VariableItem* item;

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

    // Enable NDEF toggle
    item = variable_item_list_add(
        app->variable_item_list,
        "Enable NDEF:",
        2,
        hid_device_scene_settings_set_ndef_enabled,
        app);
    variable_item_set_current_value_index(item, app->ndef_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, on_off_text[app->ndef_enabled ? 1 : 0]);

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

    if(event.type == SceneManagerEventTypeBack) {
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
