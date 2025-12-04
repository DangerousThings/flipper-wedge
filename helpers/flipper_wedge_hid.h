#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

#define HID_DEVICE_BT_KEYS_STORAGE_NAME ".hid_device_bt.keys"

typedef struct HidDeviceHid HidDeviceHid;

typedef void (*HidDeviceHidConnectionCallback)(bool usb_connected, bool bt_connected, void* context);

/** Allocate HID helper
 *
 * @return HidDeviceHid instance
 */
HidDeviceHid* hid_device_hid_alloc(void);

/** Free HID helper
 *
 * @param instance HidDeviceHid instance
 */
void hid_device_hid_free(HidDeviceHid* instance);

/** Initialize USB HID interface
 * Like Bad USB pattern - call at app start or when switching to USB mode
 *
 * @param instance HidDeviceHid instance
 */
void hid_device_hid_init_usb(HidDeviceHid* instance);

/** Deinitialize USB HID interface
 * Like Bad USB pattern - call when switching away from USB mode or at app exit
 *
 * @param instance HidDeviceHid instance
 */
void hid_device_hid_deinit_usb(HidDeviceHid* instance);

/** Initialize BLE HID interface
 * Like Bad USB pattern - call at app start or when switching to BLE mode
 *
 * @param instance HidDeviceHid instance
 */
void hid_device_hid_init_ble(HidDeviceHid* instance);

/** Deinitialize BLE HID interface
 * Like Bad USB pattern - call when switching away from BLE mode or at app exit
 *
 * @param instance HidDeviceHid instance
 */
void hid_device_hid_deinit_ble(HidDeviceHid* instance);

/** Set connection status callback
 *
 * @param instance HidDeviceHid instance
 * @param callback Callback function
 * @param context Callback context
 */
void hid_device_hid_set_connection_callback(
    HidDeviceHid* instance,
    HidDeviceHidConnectionCallback callback,
    void* context);

/** Check if USB HID is connected
 *
 * @param instance HidDeviceHid instance
 * @return true if connected
 */
bool hid_device_hid_is_usb_connected(HidDeviceHid* instance);

/** Check if BT HID is connected
 *
 * @param instance HidDeviceHid instance
 * @return true if connected
 */
bool hid_device_hid_is_bt_connected(HidDeviceHid* instance);

/** Check if any HID connection is available
 *
 * @param instance HidDeviceHid instance
 * @return true if USB or BT is connected
 */
bool hid_device_hid_is_connected(HidDeviceHid* instance);

/** Type a string via HID keyboard
 * Sends to both USB and BT if connected
 *
 * @param instance HidDeviceHid instance
 * @param str String to type
 */
void hid_device_hid_type_string(HidDeviceHid* instance, const char* str);

/** Type a single character via HID keyboard
 *
 * @param instance HidDeviceHid instance
 * @param c Character to type
 */
void hid_device_hid_type_char(HidDeviceHid* instance, char c);

/** Press and release Enter key
 *
 * @param instance HidDeviceHid instance
 */
void hid_device_hid_press_enter(HidDeviceHid* instance);

/** Release all keys
 *
 * @param instance HidDeviceHid instance
 */
void hid_device_hid_release_all(HidDeviceHid* instance);
