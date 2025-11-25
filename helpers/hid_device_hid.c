#include "hid_device_hid.h"
#include <storage/storage.h>

#define TAG "HidDeviceHid"

#define HID_TYPE_DELAY_MS 2

struct HidDeviceHid {
    // USB HID
    FuriHalUsbInterface* usb_mode_prev;  // Save previous USB mode for restoration
    bool usb_started;

    // Bluetooth HID
    Bt* bt;
    FuriHalBleProfileBase* ble_hid_profile;
    bool bt_started;
    bool bt_connected;

    // Callback
    HidDeviceHidConnectionCallback connection_callback;
    void* connection_callback_context;
};

static void hid_device_hid_bt_status_callback(BtStatus status, void* context) {
    furi_assert(context);
    HidDeviceHid* instance = context;

    bool connected = (status == BtStatusConnected);
    instance->bt_connected = connected;

    FURI_LOG_I(TAG, "BT HID connection status: %s", connected ? "connected" : "disconnected");

    if(instance->connection_callback) {
        bool usb_connected = hid_device_hid_is_usb_connected(instance);
        instance->connection_callback(usb_connected, connected, instance->connection_callback_context);
    }
}

HidDeviceHid* hid_device_hid_alloc(void) {
    HidDeviceHid* instance = malloc(sizeof(HidDeviceHid));

    instance->usb_mode_prev = NULL;
    instance->usb_started = false;
    instance->bt = NULL;
    instance->ble_hid_profile = NULL;
    instance->bt_started = false;
    instance->bt_connected = false;
    instance->connection_callback = NULL;
    instance->connection_callback_context = NULL;

    return instance;
}

void hid_device_hid_free(HidDeviceHid* instance) {
    furi_assert(instance);

    hid_device_hid_stop(instance);
    free(instance);
}

void hid_device_hid_start(HidDeviceHid* instance, bool enable_usb, bool enable_bt) {
    furi_assert(instance);

    // Start USB HID if requested
    if(enable_usb && !instance->usb_started) {
        FURI_LOG_I(TAG, "Starting USB HID");
        // Save current USB mode for restoration (like official HID app)
        instance->usb_mode_prev = furi_hal_usb_get_config();
        furi_hal_usb_unlock();
        furi_check(furi_hal_usb_set_config(&usb_hid, NULL) == true);
        instance->usb_started = true;
    }

    // Start Bluetooth HID if requested
    if(enable_bt && !instance->bt_started) {
        FURI_LOG_I(TAG, "Starting BT HID");
        instance->bt = furi_record_open(RECORD_BT);

        // Disconnect from any existing connection before profile switch
        bt_disconnect(instance->bt);
        // Wait 200ms for 2nd core to update NVM storage (CRITICAL!)
        furi_delay_ms(200);

        // Set up key storage
        Storage* storage = furi_record_open(RECORD_STORAGE);
        storage_common_migrate(
            storage,
            EXT_PATH("apps/NFC/" HID_DEVICE_BT_KEYS_STORAGE_NAME),
            APP_DATA_PATH(HID_DEVICE_BT_KEYS_STORAGE_NAME));
        bt_keys_storage_set_storage_path(instance->bt, APP_DATA_PATH(HID_DEVICE_BT_KEYS_STORAGE_NAME));
        furi_record_close(RECORD_STORAGE);

        // Start BLE HID profile
        instance->ble_hid_profile = bt_profile_start(instance->bt, ble_profile_hid, NULL);
        furi_check(instance->ble_hid_profile);

        // Start advertising
        furi_hal_bt_start_advertising();

        // Register connection status callback
        bt_set_status_changed_callback(instance->bt, hid_device_hid_bt_status_callback, instance);

        instance->bt_started = true;
    }

    FURI_LOG_I(TAG, "HID services started (USB: %d, BT: %d)", instance->usb_started, instance->bt_started);
}

void hid_device_hid_stop(HidDeviceHid* instance) {
    furi_assert(instance);

    // Stop Bluetooth HID
    if(instance->bt_started && instance->bt) {
        FURI_LOG_I(TAG, "Stopping BT HID");
        bt_set_status_changed_callback(instance->bt, NULL, NULL);
        bt_disconnect(instance->bt);
        furi_delay_ms(200);
        bt_keys_storage_set_default_path(instance->bt);
        furi_check(bt_profile_restore_default(instance->bt));
        furi_record_close(RECORD_BT);
        instance->bt = NULL;
        instance->ble_hid_profile = NULL;
        instance->bt_started = false;
        instance->bt_connected = false;
    }

    // Stop USB HID - restore previous USB mode (like official HID app)
    if(instance->usb_started) {
        FURI_LOG_I(TAG, "Stopping USB HID, restoring previous USB mode");
        // Restore to saved previous mode (don't lock - official app doesn't)
        if(instance->usb_mode_prev) {
            furi_hal_usb_set_config(instance->usb_mode_prev, NULL);
        }
        instance->usb_started = false;
    }

    FURI_LOG_I(TAG, "HID services stopped");
}

void hid_device_hid_start_bt(HidDeviceHid* instance) {
    furi_assert(instance);

    // Don't start if already started
    if(instance->bt_started) {
        FURI_LOG_I(TAG, "BT HID already started");
        return;
    }

    FURI_LOG_I(TAG, "Starting BT HID");
    instance->bt = furi_record_open(RECORD_BT);

    // Disconnect from any existing connection before profile switch
    bt_disconnect(instance->bt);
    // Wait 200ms for 2nd core to update NVM storage (CRITICAL!)
    furi_delay_ms(200);

    // Set up key storage
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_migrate(
        storage,
        EXT_PATH("apps/NFC/" HID_DEVICE_BT_KEYS_STORAGE_NAME),
        APP_DATA_PATH(HID_DEVICE_BT_KEYS_STORAGE_NAME));
    bt_keys_storage_set_storage_path(instance->bt, APP_DATA_PATH(HID_DEVICE_BT_KEYS_STORAGE_NAME));
    furi_record_close(RECORD_STORAGE);

    // Start BLE HID profile
    instance->ble_hid_profile = bt_profile_start(instance->bt, ble_profile_hid, NULL);
    furi_check(instance->ble_hid_profile);

    // Start advertising
    furi_hal_bt_start_advertising();

    // Register connection status callback
    bt_set_status_changed_callback(instance->bt, hid_device_hid_bt_status_callback, instance);

    instance->bt_started = true;
    FURI_LOG_I(TAG, "BT HID started");
}

void hid_device_hid_stop_bt(HidDeviceHid* instance) {
    furi_assert(instance);

    // Don't stop if not started
    if(!instance->bt_started || !instance->bt) {
        FURI_LOG_I(TAG, "BT HID not started, nothing to stop");
        return;
    }

    FURI_LOG_I(TAG, "Stopping BT HID");
    bt_set_status_changed_callback(instance->bt, NULL, NULL);
    bt_disconnect(instance->bt);
    furi_delay_ms(200);  // CRITICAL delay for NVM sync
    bt_keys_storage_set_default_path(instance->bt);
    furi_check(bt_profile_restore_default(instance->bt));
    furi_record_close(RECORD_BT);
    instance->bt = NULL;
    instance->ble_hid_profile = NULL;
    instance->bt_started = false;
    instance->bt_connected = false;

    FURI_LOG_I(TAG, "BT HID stopped");

    // Notify connection callback about BT disconnection
    if(instance->connection_callback) {
        bool usb_connected = hid_device_hid_is_usb_connected(instance);
        instance->connection_callback(usb_connected, false, instance->connection_callback_context);
    }
}

void hid_device_hid_set_connection_callback(
    HidDeviceHid* instance,
    HidDeviceHidConnectionCallback callback,
    void* context) {
    furi_assert(instance);
    instance->connection_callback = callback;
    instance->connection_callback_context = context;
}

bool hid_device_hid_is_usb_connected(HidDeviceHid* instance) {
    furi_assert(instance);
    if(!instance->usb_started) return false;
    return furi_hal_hid_is_connected();
}

bool hid_device_hid_is_bt_connected(HidDeviceHid* instance) {
    furi_assert(instance);
    if(!instance->bt_started) return false;
    return instance->bt_connected;
}

bool hid_device_hid_is_connected(HidDeviceHid* instance) {
    return hid_device_hid_is_usb_connected(instance) || hid_device_hid_is_bt_connected(instance);
}

void hid_device_hid_type_char(HidDeviceHid* instance, char c) {
    furi_assert(instance);

    uint16_t keycode = HID_ASCII_TO_KEY(c);
    if(keycode == HID_KEYBOARD_NONE) return;

    // Send to USB HID
    if(hid_device_hid_is_usb_connected(instance)) {
        furi_hal_hid_kb_press(keycode);
        furi_hal_hid_kb_release(keycode);
    }

    // Send to BT HID
    if(hid_device_hid_is_bt_connected(instance) && instance->ble_hid_profile) {
        ble_profile_hid_kb_press(instance->ble_hid_profile, keycode);
        ble_profile_hid_kb_release(instance->ble_hid_profile, keycode);
    }

    furi_delay_ms(HID_TYPE_DELAY_MS);
}

void hid_device_hid_type_string(HidDeviceHid* instance, const char* str) {
    furi_assert(instance);
    furi_assert(str);

    while(*str) {
        hid_device_hid_type_char(instance, *str);
        str++;
    }
}

void hid_device_hid_press_enter(HidDeviceHid* instance) {
    furi_assert(instance);

    uint16_t keycode = HID_KEYBOARD_RETURN;

    // Send to USB HID
    if(hid_device_hid_is_usb_connected(instance)) {
        furi_hal_hid_kb_press(keycode);
        furi_hal_hid_kb_release(keycode);
    }

    // Send to BT HID
    if(hid_device_hid_is_bt_connected(instance) && instance->ble_hid_profile) {
        ble_profile_hid_kb_press(instance->ble_hid_profile, keycode);
        ble_profile_hid_kb_release(instance->ble_hid_profile, keycode);
    }
}

void hid_device_hid_release_all(HidDeviceHid* instance) {
    furi_assert(instance);

    // Release all keys on USB HID
    if(hid_device_hid_is_usb_connected(instance)) {
        furi_hal_hid_kb_release_all();
    }

    // Release all keys on BT HID
    if(hid_device_hid_is_bt_connected(instance) && instance->ble_hid_profile) {
        ble_profile_hid_kb_release_all(instance->ble_hid_profile);
    }
}
