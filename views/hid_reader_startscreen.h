#pragma once

#include <gui/view.h>
#include "../helpers/hid_reader_custom_event.h"

// Forward declaration
typedef struct HidReaderStartscreen HidReaderStartscreen;

// Scan states for display
typedef enum {
    HidReaderDisplayStateIdle,
    HidReaderDisplayStateScanning,
    HidReaderDisplayStateWaiting,
    HidReaderDisplayStateResult,
    HidReaderDisplayStateSent,
} HidReaderDisplayState;

typedef void (*HidReaderStartscreenCallback)(HidReaderCustomEvent event, void* context);

void hid_reader_startscreen_set_callback(
    HidReaderStartscreen* hid_reader_startscreen,
    HidReaderStartscreenCallback callback,
    void* context);

View* hid_reader_startscreen_get_view(HidReaderStartscreen* hid_reader_static);

HidReaderStartscreen* hid_reader_startscreen_alloc();

void hid_reader_startscreen_free(HidReaderStartscreen* hid_reader_static);

void hid_reader_startscreen_set_connected_status(
    HidReaderStartscreen* instance,
    bool usb_connected,
    bool bt_connected);

void hid_reader_startscreen_set_mode(
    HidReaderStartscreen* instance,
    uint8_t mode);

uint8_t hid_reader_startscreen_get_mode(HidReaderStartscreen* instance);

void hid_reader_startscreen_set_display_state(
    HidReaderStartscreen* instance,
    HidReaderDisplayState state);

void hid_reader_startscreen_set_status_text(
    HidReaderStartscreen* instance,
    const char* text);

void hid_reader_startscreen_set_uid_text(
    HidReaderStartscreen* instance,
    const char* text);
