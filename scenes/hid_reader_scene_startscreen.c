#include "../hid_reader.h"
#include "../helpers/hid_reader_custom_event.h"
#include "../views/hid_reader_startscreen.h"
#include "../helpers/hid_reader_haptic.h"
#include "../helpers/hid_reader_led.h"

// Forward declarations
static void hid_reader_scene_startscreen_start_scanning(HidReader* app);
static void hid_reader_scene_startscreen_stop_scanning(HidReader* app);

void hid_reader_scene_startscreen_callback(HidReaderCustomEvent event, void* context) {
    furi_assert(context);
    HidReader* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

// NFC callback - called when an NFC tag is detected
static void hid_reader_scene_startscreen_nfc_callback(HidReaderNfcData* data, void* context) {
    furi_assert(context);
    HidReader* app = context;

    FURI_LOG_I("HidReaderScene", "NFC callback: uid_len=%d, has_ndef=%d", data->uid_len, data->has_ndef);

    // Store the NFC data
    app->nfc_uid_len = data->uid_len;
    memcpy(app->nfc_uid, data->uid, data->uid_len);
    if(data->has_ndef && app->ndef_enabled) {
        snprintf(app->ndef_text, sizeof(app->ndef_text), "%s", data->ndef_text);
    } else {
        app->ndef_text[0] = '\0';
    }

    // Send event to main thread
    FURI_LOG_D("HidReaderScene", "NFC callback: sending custom event");
    view_dispatcher_send_custom_event(app->view_dispatcher, HidReaderCustomEventNfcDetected);
}

// RFID callback - called when an RFID tag is detected
static void hid_reader_scene_startscreen_rfid_callback(HidReaderRfidData* data, void* context) {
    furi_assert(context);
    HidReader* app = context;

    // Store the RFID data
    app->rfid_uid_len = data->uid_len;
    memcpy(app->rfid_uid, data->uid, data->uid_len);

    // Send event to main thread
    view_dispatcher_send_custom_event(app->view_dispatcher, HidReaderCustomEventRfidDetected);
}

static void hid_reader_scene_startscreen_update_status(HidReader* app) {
    bool usb_connected = hid_reader_hid_is_usb_connected(app->hid);
    bool bt_connected = hid_reader_hid_is_bt_connected(app->hid);
    hid_reader_startscreen_set_connected_status(
        app->hid_reader_startscreen, usb_connected, bt_connected);
}

static void hid_reader_scene_startscreen_output_and_reset(HidReader* app) {
    FURI_LOG_I("HidReaderScene", "output_and_reset: nfc_uid_len=%d, rfid_uid_len=%d", app->nfc_uid_len, app->rfid_uid_len);

    // Format the output
    bool nfc_first = (app->mode == HidReaderModeNfc ||
                      app->mode == HidReaderModeNfcThenRfid);

    hid_reader_format_output(
        app->nfc_uid_len > 0 ? app->nfc_uid : NULL,
        app->nfc_uid_len,
        app->rfid_uid_len > 0 ? app->rfid_uid : NULL,
        app->rfid_uid_len,
        app->ndef_text,
        app->delimiter,
        nfc_first,
        app->output_buffer,
        sizeof(app->output_buffer));

    // Show the UID briefly
    hid_reader_startscreen_set_uid_text(app->hid_reader_startscreen, app->output_buffer);
    hid_reader_startscreen_set_display_state(app->hid_reader_startscreen, HidReaderDisplayStateResult);

    // Type the output via HID
    if(hid_reader_hid_is_connected(app->hid)) {
        hid_reader_hid_type_string(app->hid, app->output_buffer);
        if(app->append_enter) {
            hid_reader_hid_press_enter(app->hid);
        }
    }

    // Haptic and LED feedback
    if(app->haptic) {
        hid_reader_play_happy_bump(app);
    }
    if(app->led) {
        hid_reader_led_set_rgb(app, 0, 255, 0);  // Green flash
    }

    // Brief delay then show "Sent"
    furi_delay_ms(200);
    hid_reader_startscreen_set_display_state(app->hid_reader_startscreen, HidReaderDisplayStateSent);
    furi_delay_ms(200);

    // Reset LED
    if(app->led) {
        hid_reader_led_reset(app);
    }

    // Clear scanned data
    app->nfc_uid_len = 0;
    app->rfid_uid_len = 0;
    app->ndef_text[0] = '\0';

    // Back to idle and restart scanning
    hid_reader_startscreen_set_display_state(app->hid_reader_startscreen, HidReaderDisplayStateIdle);
    app->scan_state = HidReaderScanStateIdle;

    // Brief cooldown before restarting
    FURI_LOG_D("HidReaderScene", "output_and_reset: cooldown before restart");
    furi_delay_ms(300);
    FURI_LOG_I("HidReaderScene", "output_and_reset: restarting scanning");
    hid_reader_scene_startscreen_start_scanning(app);
}

static void hid_reader_scene_startscreen_start_scanning(HidReader* app) {
    // Don't scan if no HID connection or in Bluetooth pairing mode
    if(!hid_reader_hid_is_connected(app->hid)) {
        FURI_LOG_D("HidReaderScene", "start_scanning: no HID connection, skipping");
        return;
    }
    if(app->mode == HidReaderModePairBluetooth) {
        FURI_LOG_D("HidReaderScene", "start_scanning: BT pair mode, skipping");
        return;
    }

    FURI_LOG_I("HidReaderScene", "start_scanning: mode=%d, current scan_state=%d", app->mode, app->scan_state);
    app->scan_state = HidReaderScanStateScanning;
    // Keep display in Idle state to show mode selector while scanning

    // Start appropriate reader(s) based on mode
    switch(app->mode) {
    case HidReaderModeNfc:
        hid_reader_nfc_set_callback(app->nfc, hid_reader_scene_startscreen_nfc_callback, app);
        hid_reader_nfc_start(app->nfc, app->ndef_enabled);
        break;
    case HidReaderModeRfid:
        hid_reader_rfid_set_callback(app->rfid, hid_reader_scene_startscreen_rfid_callback, app);
        hid_reader_rfid_start(app->rfid);
        break;
    case HidReaderModeNfcThenRfid:
        // Start with NFC
        hid_reader_nfc_set_callback(app->nfc, hid_reader_scene_startscreen_nfc_callback, app);
        hid_reader_nfc_start(app->nfc, app->ndef_enabled);
        break;
    case HidReaderModeRfidThenNfc:
        // Start with RFID
        hid_reader_rfid_set_callback(app->rfid, hid_reader_scene_startscreen_rfid_callback, app);
        hid_reader_rfid_start(app->rfid);
        break;
    default:
        break;
    }
}

static void hid_reader_scene_startscreen_stop_scanning(HidReader* app) {
    FURI_LOG_I("HidReaderScene", "stop_scanning: current scan_state=%d", app->scan_state);

    hid_reader_nfc_stop(app->nfc);
    hid_reader_rfid_stop(app->rfid);
    app->scan_state = HidReaderScanStateIdle;
    hid_reader_startscreen_set_display_state(app->hid_reader_startscreen, HidReaderDisplayStateIdle);
    FURI_LOG_D("HidReaderScene", "stop_scanning: done, scan_state now Idle");
}

void hid_reader_scene_startscreen_on_enter(void* context) {
    furi_assert(context);
    HidReader* app = context;
    hid_reader_startscreen_set_callback(
        app->hid_reader_startscreen, hid_reader_scene_startscreen_callback, app);

    // Set initial mode on the view
    hid_reader_startscreen_set_mode(app->hid_reader_startscreen, app->mode);

    // Update HID connection status
    hid_reader_scene_startscreen_update_status(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidReaderViewIdStartscreen);

    // Start scanning if HID is connected
    hid_reader_scene_startscreen_start_scanning(app);
}

bool hid_reader_scene_startscreen_on_event(void* context, SceneManagerEvent event) {
    HidReader* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case HidReaderCustomEventModeChange:
            // Stop current scanning and restart with new mode
            hid_reader_scene_startscreen_stop_scanning(app);

            // Get the new mode from the view (the view already updated it)
            app->mode = hid_reader_startscreen_get_mode(app->hid_reader_startscreen);
            FURI_LOG_I("HidReaderScene", "Mode changed to: %d", app->mode);
            hid_reader_scene_startscreen_start_scanning(app);
            consumed = true;
            break;

        case HidReaderCustomEventNfcDetected:
            // NFC tag detected
            FURI_LOG_I("HidReaderScene", "Event NfcDetected: mode=%d, scan_state=%d", app->mode, app->scan_state);
            if(app->mode == HidReaderModeNfc) {
                // Single tag mode - output immediately
                FURI_LOG_D("HidReaderScene", "NFC single/any mode - stopping and outputting");
                hid_reader_scene_startscreen_stop_scanning(app);
                hid_reader_scene_startscreen_output_and_reset(app);
            } else if(app->mode == HidReaderModeNfcThenRfid) {
                // Combo mode - now wait for RFID
                hid_reader_nfc_stop(app->nfc);
                app->scan_state = HidReaderScanStateWaitingSecond;
                hid_reader_startscreen_set_status_text(app->hid_reader_startscreen, "Waiting for RFID...");
                hid_reader_startscreen_set_display_state(app->hid_reader_startscreen, HidReaderDisplayStateWaiting);

                // Start RFID scanning
                hid_reader_rfid_set_callback(app->rfid, hid_reader_scene_startscreen_rfid_callback, app);
                hid_reader_rfid_start(app->rfid);

                // TODO: Add timeout timer
            } else if(app->mode == HidReaderModeRfidThenNfc && app->scan_state == HidReaderScanStateWaitingSecond) {
                // Got the second tag in combo mode
                hid_reader_nfc_stop(app->nfc);
                hid_reader_scene_startscreen_output_and_reset(app);
            }
            consumed = true;
            break;

        case HidReaderCustomEventRfidDetected:
            // RFID tag detected
            FURI_LOG_I("HidReaderScene", "Event RfidDetected: mode=%d, scan_state=%d", app->mode, app->scan_state);
            if(app->mode == HidReaderModeRfid) {
                // Single tag mode - output immediately
                FURI_LOG_D("HidReaderScene", "RFID single/any mode - stopping and outputting");
                hid_reader_scene_startscreen_stop_scanning(app);
                hid_reader_scene_startscreen_output_and_reset(app);
            } else if(app->mode == HidReaderModeRfidThenNfc) {
                // Combo mode - now wait for NFC
                hid_reader_rfid_stop(app->rfid);
                app->scan_state = HidReaderScanStateWaitingSecond;
                hid_reader_startscreen_set_status_text(app->hid_reader_startscreen, "Waiting for NFC...");
                hid_reader_startscreen_set_display_state(app->hid_reader_startscreen, HidReaderDisplayStateWaiting);

                // Start NFC scanning
                hid_reader_nfc_set_callback(app->nfc, hid_reader_scene_startscreen_nfc_callback, app);
                hid_reader_nfc_start(app->nfc, app->ndef_enabled);

                // TODO: Add timeout timer
            } else if(app->mode == HidReaderModeNfcThenRfid && app->scan_state == HidReaderScanStateWaitingSecond) {
                // Got the second tag in combo mode
                hid_reader_rfid_stop(app->rfid);
                hid_reader_scene_startscreen_output_and_reset(app);
            }
            consumed = true;
            break;

        case HidReaderCustomEventStartscreenBack:
            hid_reader_scene_startscreen_stop_scanning(app);
            notification_message(app->notification, &sequence_reset_red);
            notification_message(app->notification, &sequence_reset_green);
            notification_message(app->notification, &sequence_reset_blue);
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, HidReaderSceneStartscreen)) {
                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->view_dispatcher);
            }
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Update HID connection status periodically
        hid_reader_scene_startscreen_update_status(app);

        // Process NFC state machine (handles scanner->poller transitions and callbacks)
        hid_reader_nfc_tick(app->nfc);

        // Check if we should start/stop scanning based on HID connection
        bool connected = hid_reader_hid_is_connected(app->hid);
        if(connected && app->scan_state == HidReaderScanStateIdle && app->mode != HidReaderModePairBluetooth) {
            hid_reader_scene_startscreen_start_scanning(app);
        } else if(!connected && app->scan_state != HidReaderScanStateIdle) {
            hid_reader_scene_startscreen_stop_scanning(app);
        }
    }

    return consumed;
}

void hid_reader_scene_startscreen_on_exit(void* context) {
    HidReader* app = context;
    hid_reader_scene_startscreen_stop_scanning(app);
}
