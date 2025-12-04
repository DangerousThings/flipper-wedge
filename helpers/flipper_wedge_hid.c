#include "hid_device_hid.h"
#include "hid_device_debug.h"
#include <storage/storage.h>

#define TAG "HidDeviceHid"

#define HID_TYPE_DELAY_MS 2

// MAC address XOR to make Flipper appear as different device in HID mode
#define HID_BT_MAC_XOR 0xF1D0  // "FliD" in hex - unique identifier

struct HidDeviceHid {
    // USB HID
    FuriHalUsbInterface* usb_mode_prev;  // Save previous USB mode for restoration
    bool usb_initialized;

    // Bluetooth HID
    Bt* bt;
    FuriHalBleProfileBase* ble_hid_profile;
    bool bt_initialized;
    bool bt_connected;

    // Callback
    HidDeviceHidConnectionCallback connection_callback;
    void* connection_callback_context;
};

static void hid_device_hid_bt_status_callback(BtStatus status, void* context) {
    furi_assert(context);
    HidDeviceHid* instance = context;

    bool connected = (status == BtStatusConnected);
    bool prev_connected = instance->bt_connected;
    instance->bt_connected = connected;

    FURI_LOG_I(TAG, "BT status: %d (prev=%d, new=%d)", status, prev_connected, connected);

    if(instance->connection_callback) {
        bool usb_connected = hid_device_hid_is_usb_connected(instance);
        instance->connection_callback(usb_connected, connected, instance->connection_callback_context);
    }
}

HidDeviceHid* hid_device_hid_alloc(void) {
    HidDeviceHid* instance = malloc(sizeof(HidDeviceHid));

    instance->usb_mode_prev = NULL;
    instance->usb_initialized = false;
    instance->bt = NULL;
    instance->ble_hid_profile = NULL;
    instance->bt_initialized = false;
    instance->bt_connected = false;
    instance->connection_callback = NULL;
    instance->connection_callback_context = NULL;

    return instance;
}

void hid_device_hid_free(HidDeviceHid* instance) {
    furi_assert(instance);

    // Clean up any initialized interfaces
    if(instance->usb_initialized) {
        hid_device_hid_deinit_usb(instance);
    }
    if(instance->bt_initialized) {
        hid_device_hid_deinit_ble(instance);
    }

    free(instance);
}

void hid_device_hid_init_usb(HidDeviceHid* instance) {
    furi_assert(instance);

    if(instance->usb_initialized) {
        FURI_LOG_W(TAG, "USB HID already initialized");
        return;
    }

    FURI_LOG_I(TAG, "Initializing USB HID");
    hid_device_debug_log(TAG, "Init USB HID");

    // Save current USB mode for restoration (like Bad USB)
    instance->usb_mode_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_hid, NULL) == true);
    instance->usb_initialized = true;

    FURI_LOG_I(TAG, "USB HID initialized");

    // Notify connection callback
    if(instance->connection_callback) {
        bool usb_connected = hid_device_hid_is_usb_connected(instance);
        bool bt_connected = hid_device_hid_is_bt_connected(instance);
        instance->connection_callback(usb_connected, bt_connected, instance->connection_callback_context);
    }
}

void hid_device_hid_deinit_usb(HidDeviceHid* instance) {
    furi_assert(instance);

    if(!instance->usb_initialized) {
        FURI_LOG_W(TAG, "USB HID not initialized");
        return;
    }

    FURI_LOG_I(TAG, "Deinitializing USB HID");
    hid_device_debug_log(TAG, "Deinit USB HID");

    // Restore previous USB mode (like Bad USB)
    if(instance->usb_mode_prev) {
        furi_hal_usb_set_config(instance->usb_mode_prev, NULL);
    }
    instance->usb_initialized = false;

    FURI_LOG_I(TAG, "USB HID deinitialized");

    // Notify connection callback
    if(instance->connection_callback) {
        bool bt_connected = hid_device_hid_is_bt_connected(instance);
        instance->connection_callback(false, bt_connected, instance->connection_callback_context);
    }
}

void hid_device_hid_init_ble(HidDeviceHid* instance) {
    furi_assert(instance);

    if(instance->bt_initialized) {
        FURI_LOG_W(TAG, "BLE HID already initialized");
        return;
    }

    FURI_LOG_I(TAG, "Initializing BLE HID");
    hid_device_debug_log(TAG, "Init BLE HID - opening BT record");

    instance->bt = furi_record_open(RECORD_BT);
    hid_device_debug_log(TAG, "BT record opened");

    // Disconnect from any existing connection before profile switch
    hid_device_debug_log(TAG, "Disconnecting BT...");
    bt_disconnect(instance->bt);
    hid_device_debug_log(TAG, "BT disconnected, waiting 200ms for NVM sync");
    // Wait 200ms for 2nd core to update NVM storage (CRITICAL!)
    furi_delay_ms(200);
    hid_device_debug_log(TAG, "NVM sync complete");

    // Set up key storage
    hid_device_debug_log(TAG, "Setting up BT key storage");
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_migrate(
        storage,
        EXT_PATH("apps/NFC/" HID_DEVICE_BT_KEYS_STORAGE_NAME),
        APP_DATA_PATH(HID_DEVICE_BT_KEYS_STORAGE_NAME));
    bt_keys_storage_set_storage_path(instance->bt, APP_DATA_PATH(HID_DEVICE_BT_KEYS_STORAGE_NAME));
    furi_record_close(RECORD_STORAGE);
    hid_device_debug_log(TAG, "BT key storage configured");

    // Start BLE HID profile with "HID" prefix (max 8 chars)
    // MAC XOR makes Flipper appear as different device, preventing host from
    // using cached pairing credentials from the default Flipper profile
    hid_device_debug_log(TAG, "Starting BLE HID profile...");
    BleProfileHidParams hid_params = {
        .device_name_prefix = "HID",  // Must be <8 chars per firmware limitation
        .mac_xor = HID_BT_MAC_XOR,  // XOR MAC to appear as different device
    };
    instance->ble_hid_profile = bt_profile_start(instance->bt, ble_profile_hid, &hid_params);
    hid_device_debug_log(TAG, "bt_profile_start returned: %p", (void*)instance->ble_hid_profile);

    if(!instance->ble_hid_profile) {
        FURI_LOG_E(TAG, "FATAL: bt_profile_start returned NULL!");
        hid_device_debug_log(TAG, "ERROR: bt_profile_start failed!");
        furi_record_close(RECORD_BT);
        instance->bt = NULL;
        return;  // Fail gracefully instead of crashing
    }

    // Start advertising
    hid_device_debug_log(TAG, "Starting BT advertising");
    furi_hal_bt_start_advertising();

    // Register connection status callback
    hid_device_debug_log(TAG, "Registering BT status callback");
    bt_set_status_changed_callback(instance->bt, hid_device_hid_bt_status_callback, instance);

    instance->bt_initialized = true;

    FURI_LOG_I(TAG, "BLE HID initialized and advertising");
    hid_device_debug_log(TAG, "BLE HID init complete!");
}

void hid_device_hid_deinit_ble(HidDeviceHid* instance) {
    furi_assert(instance);

    if(!instance->bt_initialized || !instance->bt) {
        FURI_LOG_W(TAG, "BLE HID not initialized");
        return;
    }

    FURI_LOG_I(TAG, "Deinitializing BLE HID");
    hid_device_debug_log(TAG, "Deinit BLE HID");

    bt_set_status_changed_callback(instance->bt, NULL, NULL);
    bt_disconnect(instance->bt);
    furi_delay_ms(200);  // CRITICAL delay for NVM sync
    bt_keys_storage_set_default_path(instance->bt);

    FURI_LOG_I(TAG, "Restoring default BT profile");
    furi_check(bt_profile_restore_default(instance->bt));

    // Wait for 2nd core to restart and apply default profile
    furi_delay_ms(200);

    // Explicitly restart advertising with default profile
    furi_hal_bt_start_advertising();

    furi_record_close(RECORD_BT);
    instance->bt = NULL;
    instance->ble_hid_profile = NULL;
    instance->bt_initialized = false;
    instance->bt_connected = false;

    FURI_LOG_I(TAG, "BLE HID deinitialized and default profile restored");

    // Notify connection callback
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
    if(!instance->usb_initialized) return false;
    return furi_hal_hid_is_connected();
}

bool hid_device_hid_is_bt_connected(HidDeviceHid* instance) {
    furi_assert(instance);
    if(!instance->bt_initialized) return false;
    return instance->bt_connected;
}

bool hid_device_hid_is_connected(HidDeviceHid* instance) {
    return hid_device_hid_is_usb_connected(instance) || hid_device_hid_is_bt_connected(instance);
}

void hid_device_hid_type_char(HidDeviceHid* instance, char c) {
    furi_assert(instance);

    uint16_t keycode = HID_ASCII_TO_KEY(c);
    if(keycode == HID_KEYBOARD_NONE) return;

    // Send to USB HID if initialized
    if(instance->usb_initialized && hid_device_hid_is_usb_connected(instance)) {
        furi_hal_hid_kb_press(keycode);
        furi_hal_hid_kb_release(keycode);
    }

    // Send to BT HID if initialized
    if(instance->bt_initialized && hid_device_hid_is_bt_connected(instance) && instance->ble_hid_profile) {
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

    // Send to USB HID if initialized
    if(instance->usb_initialized && hid_device_hid_is_usb_connected(instance)) {
        furi_hal_hid_kb_press(keycode);
        furi_hal_hid_kb_release(keycode);
    }

    // Send to BT HID if initialized
    if(instance->bt_initialized && hid_device_hid_is_bt_connected(instance) && instance->ble_hid_profile) {
        ble_profile_hid_kb_press(instance->ble_hid_profile, keycode);
        ble_profile_hid_kb_release(instance->ble_hid_profile, keycode);
    }
}

void hid_device_hid_release_all(HidDeviceHid* instance) {
    furi_assert(instance);

    // Release all keys on USB HID if initialized
    if(instance->usb_initialized && hid_device_hid_is_usb_connected(instance)) {
        furi_hal_hid_kb_release_all();
    }

    // Release all keys on BT HID if initialized
    if(instance->bt_initialized && hid_device_hid_is_bt_connected(instance) && instance->ble_hid_profile) {
        ble_profile_hid_kb_release_all(instance->ble_hid_profile);
    }
}
