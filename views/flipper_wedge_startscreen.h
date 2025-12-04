#pragma once

#include <gui/view.h>
#include "../helpers/hid_device_custom_event.h"

// Forward declaration
typedef struct HidDeviceStartscreen HidDeviceStartscreen;

// Scan states for display
typedef enum {
    HidDeviceDisplayStateIdle,
    HidDeviceDisplayStateScanning,
    HidDeviceDisplayStateWaiting,
    HidDeviceDisplayStateResult,
    HidDeviceDisplayStateSent,
} HidDeviceDisplayState;

typedef void (*HidDeviceStartscreenCallback)(HidDeviceCustomEvent event, void* context);

void hid_device_startscreen_set_callback(
    HidDeviceStartscreen* hid_device_startscreen,
    HidDeviceStartscreenCallback callback,
    void* context);

View* hid_device_startscreen_get_view(HidDeviceStartscreen* hid_device_static);

HidDeviceStartscreen* hid_device_startscreen_alloc();

void hid_device_startscreen_free(HidDeviceStartscreen* hid_device_static);

void hid_device_startscreen_set_connected_status(
    HidDeviceStartscreen* instance,
    bool usb_connected,
    bool bt_connected);

void hid_device_startscreen_set_mode(
    HidDeviceStartscreen* instance,
    uint8_t mode);

uint8_t hid_device_startscreen_get_mode(HidDeviceStartscreen* instance);

void hid_device_startscreen_set_display_state(
    HidDeviceStartscreen* instance,
    HidDeviceDisplayState state);

void hid_device_startscreen_set_status_text(
    HidDeviceStartscreen* instance,
    const char* text);

void hid_device_startscreen_set_uid_text(
    HidDeviceStartscreen* instance,
    const char* text);
