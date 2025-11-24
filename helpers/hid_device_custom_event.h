#pragma once

typedef enum {
    HidDeviceCustomEventStartscreenUp,
    HidDeviceCustomEventStartscreenDown,
    HidDeviceCustomEventStartscreenLeft,
    HidDeviceCustomEventStartscreenRight,
    HidDeviceCustomEventStartscreenBack,
    HidDeviceCustomEventTestType,

    // Scan events
    HidDeviceCustomEventNfcDetected,
    HidDeviceCustomEventRfidDetected,
    HidDeviceCustomEventScanTimeout,
    HidDeviceCustomEventDisplayDone,
    HidDeviceCustomEventCooldownDone,

    // Mode change
    HidDeviceCustomEventModeChange,

    // Settings
    HidDeviceCustomEventOpenSettings,
} HidDeviceCustomEvent;

enum HidDeviceCustomEventType {
    // Reserve first 100 events for button types and indexes, starting from 0
    HidDeviceCustomEventMenuVoid,
    HidDeviceCustomEventMenuSelected,
};

#pragma pack(push, 1)
typedef union {
    uint32_t packed_value;
    struct {
        uint16_t type;
        int16_t value;
    } content;
} HidDeviceCustomEventMenu;
#pragma pack(pop)

static inline uint32_t hid_device_custom_menu_event_pack(uint16_t type, int16_t value) {
    HidDeviceCustomEventMenu event = {.content = {.type = type, .value = value}};
    return event.packed_value;
}
static inline void
    hid_device_custom_menu_event_unpack(uint32_t packed_value, uint16_t* type, int16_t* value) {
    HidDeviceCustomEventMenu event = {.packed_value = packed_value};
    if(type) *type = event.content.type;
    if(value) *value = event.content.value;
}

static inline uint16_t hid_device_custom_menu_event_get_type(uint32_t packed_value) {
    uint16_t type;
    hid_device_custom_menu_event_unpack(packed_value, &type, NULL);
    return type;
}

static inline int16_t hid_device_custom_menu_event_get_value(uint32_t packed_value) {
    int16_t value;
    hid_device_custom_menu_event_unpack(packed_value, NULL, &value);
    return value;
}
