#include "hid_device_haptic.h"
#include "../hid_device.h"

void hid_device_play_happy_bump(void* context) {
    HidDevice* app = context;

    // Get vibration duration based on setting
    uint32_t duration_ms;
    switch(app->vibration_level) {
        case HidDeviceVibrationOff:
            return;  // No vibration
        case HidDeviceVibrationLow:
            duration_ms = 30;
            break;
        case HidDeviceVibrationMedium:
            duration_ms = 60;
            break;
        case HidDeviceVibrationHigh:
            duration_ms = 100;
            break;
        default:
            duration_ms = 30;  // Default to low
            break;
    }

    notification_message(app->notification, &sequence_set_vibro_on);
    furi_thread_flags_wait(0, FuriFlagWaitAny, duration_ms);
    notification_message(app->notification, &sequence_reset_vibro);
}

void hid_device_play_bad_bump(void* context) {
    HidDevice* app = context;
    notification_message(app->notification, &sequence_set_vibro_on);
    furi_thread_flags_wait(0, FuriFlagWaitAny, 100);
    notification_message(app->notification, &sequence_reset_vibro);
}

void hid_device_play_long_bump(void* context) {
    HidDevice* app = context;
    for(int i = 0; i < 4; i++) {
        notification_message(app->notification, &sequence_set_vibro_on);
        furi_thread_flags_wait(0, FuriFlagWaitAny, 50);
        notification_message(app->notification, &sequence_reset_vibro);
        furi_thread_flags_wait(0, FuriFlagWaitAny, 100);
    }
}
