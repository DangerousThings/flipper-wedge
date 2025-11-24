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
#include "scenes/hid_reader_scene.h"
#include "views/hid_reader_startscreen.h"
#include "helpers/hid_reader_storage.h"
#include "helpers/hid_reader_hid.h"
#include "helpers/hid_reader_nfc.h"
#include "helpers/hid_reader_rfid.h"
#include "helpers/hid_reader_format.h"
#include "contactless_hid_reader_icons.h"

#define TAG "HidReader"

#define HID_READER_VERSION "1.0"
#define HID_READER_TEXT_STORE_SIZE 128
#define HID_READER_TEXT_STORE_COUNT 3
#define HID_READER_DELIMITER_MAX_LEN 8
#define HID_READER_OUTPUT_MAX_LEN 256

// Scan modes
typedef enum {
    HidReaderModeAny,           // Any tag (NFC or RFID) - first detected wins
    HidReaderModeNfc,           // NFC only
    HidReaderModeRfid,          // RFID only
    HidReaderModeNfcThenRfid,   // NFC -> RFID combo
    HidReaderModeRfidThenNfc,   // RFID -> NFC combo
    HidReaderModePairBluetooth, // Pair BT HID
    HidReaderModeCount,
} HidReaderMode;

// Scan state machine
typedef enum {
    HidReaderScanStateIdle,           // Not scanning
    HidReaderScanStateScanning,       // Actively scanning for first tag
    HidReaderScanStateWaitingSecond,  // In combo mode, waiting for second tag
    HidReaderScanStateDisplaying,     // Showing result before "Sent"
    HidReaderScanStateCooldown,       // Brief pause after output
} HidReaderScanState;

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    SceneManager* scene_manager;
    VariableItemList* variable_item_list;
    HidReaderStartscreen* hid_reader_startscreen;
    DialogsApp* dialogs;
    FuriString* file_path;
    uint32_t haptic;
    uint32_t speaker;
    uint32_t led;
    uint32_t save_settings;
    ButtonMenu* button_menu;
    NumberInput* number_input;
    int32_t current_number;
    TextInput* text_input;
    char text_store[HID_READER_TEXT_STORE_COUNT][HID_READER_TEXT_STORE_SIZE + 1];

    // HID module
    HidReaderHid* hid;
    bool bt_enabled;

    // NFC module
    HidReaderNfc* nfc;

    // RFID module
    HidReaderRfid* rfid;

    // Scan mode and state
    HidReaderMode mode;
    HidReaderScanState scan_state;

    // Scanned data
    uint8_t nfc_uid[HID_READER_NFC_UID_MAX_LEN];
    uint8_t nfc_uid_len;
    char ndef_text[HID_READER_NDEF_MAX_LEN];
    uint8_t rfid_uid[HID_READER_RFID_UID_MAX_LEN];
    uint8_t rfid_uid_len;

    // Settings
    char delimiter[HID_READER_DELIMITER_MAX_LEN];
    bool append_enter;
    bool ndef_enabled;

    // Timers
    FuriTimer* timeout_timer;
    FuriTimer* display_timer;

    // Output buffer
    char output_buffer[HID_READER_OUTPUT_MAX_LEN];
} HidReader;

typedef enum {
    HidReaderViewIdStartscreen,
    HidReaderViewIdMenu,
    HidReaderViewIdTextInput,
    HidReaderViewIdNumberInput,
    HidReaderViewIdSettings,
} HidReaderViewId;

typedef enum {
    HidReaderHapticOff,
    HidReaderHapticOn,
} HidReaderHapticState;

typedef enum {
    HidReaderSpeakerOff,
    HidReaderSpeakerOn,
} HidReaderSpeakerState;

typedef enum {
    HidReaderLedOff,
    HidReaderLedOn,
} HidReaderLedState;

typedef enum {
    HidReaderSettingsOff,
    HidReaderSettingsOn,
} HidReaderSettingsStoreState;
