#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/button_menu.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include "scenes/hid_device_scene.h"
#include "views/hid_device_startscreen.h"
#include "helpers/hid_device_storage.h"
#include "helpers/hid_device_hid.h"
#include "helpers/hid_device_hid_worker.h"
#include "helpers/hid_device_nfc.h"
#include "helpers/hid_device_rfid.h"
#include "helpers/hid_device_format.h"
#include "helpers/hid_device_log.h"
#include "contactless_hid_device_icons.h"

#define TAG "HidDevice"

#define HID_DEVICE_VERSION "1.0"
#define HID_DEVICE_TEXT_STORE_SIZE 128
#define HID_DEVICE_TEXT_STORE_COUNT 3
#define HID_DEVICE_DELIMITER_MAX_LEN 8
#define HID_DEVICE_OUTPUT_MAX_LEN 1200  // Increased to support large NDEF text (1024) + UIDs + delimiters

// Scan modes
typedef enum {
    HidDeviceModeNfc,           // NFC only (UID)
    HidDeviceModeRfid,          // RFID only (UID)
    HidDeviceModeNdef,          // NDEF only (text records)
    HidDeviceModeNfcThenRfid,   // NFC -> RFID combo
    HidDeviceModeRfidThenNfc,   // RFID -> NFC combo
    HidDeviceModeCount,
} HidDeviceMode;

// Mode startup behavior
typedef enum {
    HidDeviceModeStartupRemember,       // Remember last used mode
    HidDeviceModeStartupDefaultNfc,     // Always start with NFC mode
    HidDeviceModeStartupDefaultRfid,    // Always start with RFID mode
    HidDeviceModeStartupDefaultNdef,    // Always start with NDEF mode
    HidDeviceModeStartupDefaultNfcRfid, // Always start with NFC+RFID mode
    HidDeviceModeStartupDefaultRfidNfc, // Always start with RFID+NFC mode
    HidDeviceModeStartupCount,
} HidDeviceModeStartup;

// Output mode
typedef enum {
    HidDeviceOutputUsb,      // USB HID only
    HidDeviceOutputBle,      // Bluetooth LE HID only
    HidDeviceOutputCount,
} HidDeviceOutput;

// Scan state machine
typedef enum {
    HidDeviceScanStateIdle,           // Not scanning
    HidDeviceScanStateScanning,       // Actively scanning for first tag
    HidDeviceScanStateWaitingSecond,  // In combo mode, waiting for second tag
    HidDeviceScanStateDisplaying,     // Showing result before "Sent"
    HidDeviceScanStateCooldown,       // Brief pause after output
} HidDeviceScanState;

// Vibration levels
typedef enum {
    HidDeviceVibrationOff,      // No vibration
    HidDeviceVibrationLow,      // 30ms
    HidDeviceVibrationMedium,   // 60ms
    HidDeviceVibrationHigh,     // 100ms
    HidDeviceVibrationCount,
} HidDeviceVibration;

// NDEF text maximum length limits
typedef enum {
    HidDeviceNdefMaxLen250,      // 250 characters (recommended for fast typing)
    HidDeviceNdefMaxLen500,      // 500 characters (covers most use cases)
    HidDeviceNdefMaxLen1000,     // 1000 characters (maximum, may take several seconds to type)
    HidDeviceNdefMaxLenCount,
} HidDeviceNdefMaxLen;

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    SceneManager* scene_manager;
    VariableItemList* variable_item_list;
    HidDeviceStartscreen* hid_device_startscreen;
    DialogsApp* dialogs;
    FuriString* file_path;
    ButtonMenu* button_menu;
    NumberInput* number_input;
    int32_t current_number;
    TextInput* text_input;
    char text_store[HID_DEVICE_TEXT_STORE_COUNT][HID_DEVICE_TEXT_STORE_SIZE + 1];

    // HID module (managed by worker thread)
    HidDeviceHidWorker* hid_worker;
    HidDeviceOutput output_mode;
    bool usb_debug_mode;  // Deprecated: kept for backward compatibility reading only

    // NFC module
    HidDeviceNfc* nfc;

    // RFID module
    HidDeviceRfid* rfid;

    // Scan mode and state
    HidDeviceMode mode;
    HidDeviceModeStartup mode_startup_behavior;
    HidDeviceScanState scan_state;

    // Scanned data
    uint8_t nfc_uid[HID_DEVICE_NFC_UID_MAX_LEN];
    uint8_t nfc_uid_len;
    char ndef_text[HID_DEVICE_NDEF_MAX_LEN];
    HidDeviceNfcError nfc_error;
    uint8_t rfid_uid[HID_DEVICE_RFID_UID_MAX_LEN];
    uint8_t rfid_uid_len;

    // Settings
    char delimiter[HID_DEVICE_DELIMITER_MAX_LEN];
    bool append_enter;
    HidDeviceVibration vibration_level;
    HidDeviceNdefMaxLen ndef_max_len;  // Maximum NDEF text length to type
    bool log_to_sd;        // Log scanned UIDs to SD card
    bool restart_pending;  // True if output mode changed and restart is required

    // Output mode switching (async to avoid UI thread blocking on bt_profile_start)
    bool output_switch_pending;
    HidDeviceOutput output_switch_target;

    // Timers
    FuriTimer* timeout_timer;
    FuriTimer* display_timer;

    // Output buffer
    char output_buffer[HID_DEVICE_OUTPUT_MAX_LEN];
} HidDevice;

typedef enum {
    HidDeviceViewIdStartscreen,
    HidDeviceViewIdMenu,
    HidDeviceViewIdTextInput,
    HidDeviceViewIdNumberInput,
    HidDeviceViewIdSettings,
    HidDeviceViewIdBtPair,
    HidDeviceViewIdOutputRestart,  // Deprecated: no longer used (dynamic switching works)
} HidDeviceViewId;

/** Switch output mode dynamically (USB <-> BLE)
 * Stops workers, deinits current HID, switches mode, inits new HID, restarts workers
 * Like Bad USB's dynamic switching pattern
 *
 * @param app HidDevice instance
 * @param new_mode New output mode to switch to
 */
void hid_device_switch_output_mode(HidDevice* app, HidDeviceOutput new_mode);

/** Get HID instance from worker
 * Helper macro to access HID interface managed by worker thread
 */
#define hid_device_get_hid(app) hid_device_hid_worker_get_hid((app)->hid_worker)
