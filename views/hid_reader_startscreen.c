#include "../hid_reader.h"
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/elements.h>

#define MODE_COUNT 5

static const char* mode_names[] = {
    "NFC",
    "RFID",
    "NFC -> RFID",
    "RFID -> NFC",
    "Pair Bluetooth",
};

struct HidReaderStartscreen {
    View* view;
    HidReaderStartscreenCallback callback;
    void* context;
};

typedef struct {
    bool usb_connected;
    bool bt_connected;
    uint8_t mode;
    HidReaderDisplayState display_state;
    char status_text[32];
    char uid_text[64];
} HidReaderStartscreenModel;

void hid_reader_startscreen_set_callback(
    HidReaderStartscreen* instance,
    HidReaderStartscreenCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

void hid_reader_startscreen_draw(Canvas* canvas, HidReaderStartscreenModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Contactless HID");

    // HID connection status
    canvas_set_font(canvas, FontSecondary);
    char status_line[32];
    if(model->usb_connected && model->bt_connected) {
        snprintf(status_line, sizeof(status_line), "USB: OK  BT: OK");
    } else if(model->usb_connected) {
        snprintf(status_line, sizeof(status_line), "USB: OK  BT: --");
    } else if(model->bt_connected) {
        snprintf(status_line, sizeof(status_line), "USB: --  BT: OK");
    } else {
        snprintf(status_line, sizeof(status_line), "No HID connection");
    }
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, status_line);

    bool connected = model->usb_connected || model->bt_connected;

    // Mode display with arrows
    canvas_set_font(canvas, FontPrimary);

    if(model->display_state == HidReaderDisplayStateIdle) {
        // Show mode selector
        const char* mode_name = mode_names[model->mode];
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, mode_name);

        // Draw navigation arrows
        canvas_draw_str_aligned(canvas, 8, 28, AlignCenter, AlignTop, "<");
        canvas_draw_str_aligned(canvas, 120, 28, AlignCenter, AlignTop, ">");

        // Bottom buttons
        canvas_set_font(canvas, FontSecondary);
        if(connected && model->mode != HidReaderModePairBluetooth) {
            canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "Scanning...");
        } else if(model->mode == HidReaderModePairBluetooth) {
            elements_button_center(canvas, "Pair");
        } else {
            canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "Connect USB or BT");
        }
    } else if(model->display_state == HidReaderDisplayStateScanning) {
        // Scanning state
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Scanning...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, model->status_text);
    } else if(model->display_state == HidReaderDisplayStateWaiting) {
        // Waiting for second tag
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Waiting...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, model->status_text);
    } else if(model->display_state == HidReaderDisplayStateResult) {
        // Show scanned UID
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, model->uid_text);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, model->status_text);
    } else if(model->display_state == HidReaderDisplayStateSent) {
        // Show "Sent"
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Sent");
    }
}

static void hid_reader_startscreen_model_init(HidReaderStartscreenModel* const model) {
    model->usb_connected = false;
    model->bt_connected = false;
    model->mode = HidReaderModeNfc;
    model->display_state = HidReaderDisplayStateIdle;
    model->status_text[0] = '\0';
    model->uid_text[0] = '\0';
}

bool hid_reader_startscreen_input(InputEvent* event, void* context) {
    furi_assert(context);
    HidReaderStartscreen* instance = context;

    if(event->type == InputTypeRelease || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyBack:
            if(event->type == InputTypeRelease) {
                instance->callback(HidReaderCustomEventStartscreenBack, instance->context);
            }
            break;
        case InputKeyLeft:
            with_view_model(
                instance->view,
                HidReaderStartscreenModel * model,
                {
                    if(model->display_state == HidReaderDisplayStateIdle) {
                        if(model->mode == 0) {
                            model->mode = MODE_COUNT - 1;
                        } else {
                            model->mode--;
                        }
                        instance->callback(HidReaderCustomEventModeChange, instance->context);
                    }
                },
                true);
            break;
        case InputKeyRight:
            with_view_model(
                instance->view,
                HidReaderStartscreenModel * model,
                {
                    if(model->display_state == HidReaderDisplayStateIdle) {
                        model->mode = (model->mode + 1) % MODE_COUNT;
                        instance->callback(HidReaderCustomEventModeChange, instance->context);
                    }
                },
                true);
            break;
        case InputKeyOk:
            if(event->type == InputTypeRelease) {
                with_view_model(
                    instance->view,
                    HidReaderStartscreenModel * model,
                    {
                        if(model->mode == HidReaderModePairBluetooth) {
                            // Pair Bluetooth mode - trigger pairing
                            instance->callback(HidReaderCustomEventStartscreenOk, instance->context);
                        }
                    },
                    false);
            }
            break;
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyMAX:
            break;
        }
    }
    return true;
}

void hid_reader_startscreen_exit(void* context) {
    furi_assert(context);
}

void hid_reader_startscreen_enter(void* context) {
    furi_assert(context);
    HidReaderStartscreen* instance = (HidReaderStartscreen*)context;
    with_view_model(
        instance->view,
        HidReaderStartscreenModel * model,
        { hid_reader_startscreen_model_init(model); },
        true);
}

HidReaderStartscreen* hid_reader_startscreen_alloc() {
    HidReaderStartscreen* instance = malloc(sizeof(HidReaderStartscreen));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(HidReaderStartscreenModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)hid_reader_startscreen_draw);
    view_set_input_callback(instance->view, hid_reader_startscreen_input);
    view_set_enter_callback(instance->view, hid_reader_startscreen_enter);
    view_set_exit_callback(instance->view, hid_reader_startscreen_exit);

    with_view_model(
        instance->view,
        HidReaderStartscreenModel * model,
        { hid_reader_startscreen_model_init(model); },
        true);

    return instance;
}

void hid_reader_startscreen_free(HidReaderStartscreen* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view, HidReaderStartscreenModel * model, { UNUSED(model); }, true);
    view_free(instance->view);
    free(instance);
}

View* hid_reader_startscreen_get_view(HidReaderStartscreen* instance) {
    furi_assert(instance);
    return instance->view;
}

void hid_reader_startscreen_set_connected_status(
    HidReaderStartscreen* instance,
    bool usb_connected,
    bool bt_connected) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        HidReaderStartscreenModel * model,
        {
            model->usb_connected = usb_connected;
            model->bt_connected = bt_connected;
        },
        true);
}

void hid_reader_startscreen_set_mode(
    HidReaderStartscreen* instance,
    uint8_t mode) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        HidReaderStartscreenModel * model,
        {
            model->mode = mode;
        },
        true);
}

uint8_t hid_reader_startscreen_get_mode(HidReaderStartscreen* instance) {
    furi_assert(instance);
    uint8_t mode = 0;
    with_view_model(
        instance->view,
        HidReaderStartscreenModel * model,
        {
            mode = model->mode;
        },
        false);
    return mode;
}

void hid_reader_startscreen_set_display_state(
    HidReaderStartscreen* instance,
    HidReaderDisplayState state) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        HidReaderStartscreenModel * model,
        {
            model->display_state = state;
        },
        true);
}

void hid_reader_startscreen_set_status_text(
    HidReaderStartscreen* instance,
    const char* text) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        HidReaderStartscreenModel * model,
        {
            if(text) {
                snprintf(model->status_text, sizeof(model->status_text), "%s", text);
            } else {
                model->status_text[0] = '\0';
            }
        },
        true);
}

void hid_reader_startscreen_set_uid_text(
    HidReaderStartscreen* instance,
    const char* text) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        HidReaderStartscreenModel * model,
        {
            if(text) {
                snprintf(model->uid_text, sizeof(model->uid_text), "%s", text);
            } else {
                model->uid_text[0] = '\0';
            }
        },
        true);
}
