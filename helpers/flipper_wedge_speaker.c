#include "hid_device_speaker.h"
#include "../hid_device.h"

#define NOTE_INPUT 587.33f

void hid_device_play_input_sound(void* context) {
    HidDevice* app = context;
    UNUSED(app);
    float volume = 1.0f;
    if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(30)) {
        furi_hal_speaker_start(NOTE_INPUT, volume);
    }
}

void hid_device_stop_all_sound(void* context) {
    HidDevice* app = context;
    UNUSED(app);
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}
