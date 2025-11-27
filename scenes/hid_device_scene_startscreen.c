#include "../hid_device.h"
#include "../helpers/hid_device_custom_event.h"
#include "../views/hid_device_startscreen.h"
#include "../helpers/hid_device_haptic.h"
#include "../helpers/hid_device_led.h"

// Forward declaration of view model (defined in view's .c file)
typedef struct {
    bool usb_connected;
    bool bt_connected;
    uint8_t mode;
    HidDeviceDisplayState display_state;
    char status_text[32];
    char uid_text[64];
} HidDeviceStartscreenModel;

// Forward declarations
static void hid_device_scene_startscreen_start_scanning(HidDevice* app);
static void hid_device_scene_startscreen_stop_scanning(HidDevice* app);

// Display timer callback - used to clear messages and return to scanning
static void hid_device_scene_startscreen_display_timer_callback(void* context) {
    furi_assert(context);
    HidDevice* app = context;

    HidDeviceDisplayState current_state = HidDeviceDisplayStateIdle;
    with_view_model(
        hid_device_startscreen_get_view(app->hid_device_startscreen),
        HidDeviceStartscreenModel * model,
        { current_state = model->display_state; },
        false);

    FURI_LOG_D("HidDeviceScene", "Display timer fired - current display state: %d", current_state);

    // State machine for display sequence
    if(current_state == HidDeviceDisplayStateResult) {
        // First timer: showed result, check if this is success or error
        bool is_error = false;
        with_view_model(
            hid_device_startscreen_get_view(app->hid_device_startscreen),
            HidDeviceStartscreenModel * model,
            {
                // Error messages don't need "Sent" confirmation
                is_error = (strstr(model->status_text, "Not NFC Forum Compliant") != NULL) ||
                          (strstr(model->status_text, "Unsupported NFC Forum Type") != NULL) ||
                          (strstr(model->status_text, "NDEF Not Found") != NULL);
            },
            false);

        if(is_error) {
            // For errors, skip "Sent" and go directly to cooldown
            hid_device_led_reset(app);
            hid_device_startscreen_set_display_state(app->hid_device_startscreen, HidDeviceDisplayStateIdle);
            hid_device_startscreen_set_status_text(app->hid_device_startscreen, "");
            furi_timer_start(app->display_timer, furi_ms_to_ticks(300));
        } else {
            // For success, show "Sent" with vibration feedback
            hid_device_startscreen_set_display_state(app->hid_device_startscreen, HidDeviceDisplayStateSent);
            hid_device_startscreen_set_status_text(app->hid_device_startscreen, "Sent");
            hid_device_play_happy_bump(app);  // Vibrate when "Sent" is displayed
            furi_timer_start(app->display_timer, furi_ms_to_ticks(200));
        }
    } else if(current_state == HidDeviceDisplayStateSent) {
        // Second timer: showed "Sent", now cooldown and prepare to restart
        hid_device_led_reset(app);
        hid_device_startscreen_set_display_state(app->hid_device_startscreen, HidDeviceDisplayStateIdle);
        hid_device_startscreen_set_status_text(app->hid_device_startscreen, "");
        furi_timer_start(app->display_timer, furi_ms_to_ticks(300));
    } else {
        // Third timer: cooldown done, return to Idle state for scanning to restart
        hid_device_led_reset(app);
        hid_device_startscreen_set_display_state(app->hid_device_startscreen, HidDeviceDisplayStateIdle);
        hid_device_startscreen_set_status_text(app->hid_device_startscreen, "");
        app->scan_state = HidDeviceScanStateIdle;
        // Tick handler will restart scanning automatically
    }
}

void hid_device_scene_startscreen_callback(HidDeviceCustomEvent event, void* context) {
    furi_assert(context);
    HidDevice* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

// NFC callback - called when an NFC tag is detected
static void hid_device_scene_startscreen_nfc_callback(HidDeviceNfcData* data, void* context) {
    furi_assert(context);
    HidDevice* app = context;

    FURI_LOG_I("HidDeviceScene", "NFC callback: uid_len=%d, has_ndef=%d, error=%d", data->uid_len, data->has_ndef, data->error);

    // Store the NFC data
    app->nfc_uid_len = data->uid_len;
    memcpy(app->nfc_uid, data->uid, data->uid_len);
    app->nfc_error = data->error;

    // In NDEF mode, only store NDEF text; in other NFC modes, store UID
    if(data->has_ndef) {
        snprintf(app->ndef_text, sizeof(app->ndef_text), "%s", data->ndef_text);
    } else {
        app->ndef_text[0] = '\0';
    }

    // Send event to main thread
    FURI_LOG_D("HidDeviceScene", "NFC callback: sending custom event");
    view_dispatcher_send_custom_event(app->view_dispatcher, HidDeviceCustomEventNfcDetected);
}

// RFID callback - called when an RFID tag is detected
static void hid_device_scene_startscreen_rfid_callback(HidDeviceRfidData* data, void* context) {
    furi_assert(context);
    HidDevice* app = context;

    // Store the RFID data
    app->rfid_uid_len = data->uid_len;
    memcpy(app->rfid_uid, data->uid, data->uid_len);

    // Send event to main thread
    view_dispatcher_send_custom_event(app->view_dispatcher, HidDeviceCustomEventRfidDetected);
}

static void hid_device_scene_startscreen_update_status(HidDevice* app) {
    bool usb_connected = hid_device_hid_is_usb_connected(hid_device_get_hid(app));
    bool bt_connected = hid_device_hid_is_bt_connected(hid_device_get_hid(app));
    hid_device_startscreen_set_connected_status(
        app->hid_device_startscreen, usb_connected, bt_connected);
}

static void hid_device_scene_startscreen_output_and_reset(HidDevice* app) {
    FURI_LOG_I("HidDeviceScene", "output_and_reset: nfc_uid_len=%d, rfid_uid_len=%d", app->nfc_uid_len, app->rfid_uid_len);

    // Determine max NDEF length from settings
    size_t max_ndef_len = 0;
    switch(app->ndef_max_len) {
        case HidDeviceNdefMaxLen250:
            max_ndef_len = 250;
            break;
        case HidDeviceNdefMaxLen500:
            max_ndef_len = 500;
            break;
        case HidDeviceNdefMaxLen1000:
            max_ndef_len = 1000;
            break;
        default:
            max_ndef_len = 250;
            break;
    }

    // Sanitize NDEF text if present (remove non-printable chars, apply length limit)
    char sanitized_ndef[HID_DEVICE_NDEF_MAX_LEN];
    if(app->ndef_text[0] != '\0') {
        size_t original_len = strlen(app->ndef_text);
        size_t sanitized_len = hid_device_sanitize_text(
            app->ndef_text,
            sanitized_ndef,
            sizeof(sanitized_ndef),
            max_ndef_len);

        FURI_LOG_I("HidDeviceScene", "NDEF text: original=%zu, sanitized=%zu, limit=%zu",
                   original_len, sanitized_len, max_ndef_len);

        // Warn if text was truncated
        if(max_ndef_len > 0 && original_len > max_ndef_len) {
            FURI_LOG_W("HidDeviceScene", "NDEF text truncated from %zu to %zu chars",
                       original_len, sanitized_len);
        }
    } else {
        sanitized_ndef[0] = '\0';
    }

    // Format the output based on mode
    if(app->mode == HidDeviceModeNdef) {
        // NDEF mode: output only NDEF text (no UID)
        snprintf(app->output_buffer, sizeof(app->output_buffer), "%s", sanitized_ndef);
    } else {
        // Other modes: format UIDs (and NDEF if present)
        bool nfc_first = (app->mode == HidDeviceModeNfc ||
                          app->mode == HidDeviceModeNfcThenRfid);

        hid_device_format_output(
            app->nfc_uid_len > 0 ? app->nfc_uid : NULL,
            app->nfc_uid_len,
            app->rfid_uid_len > 0 ? app->rfid_uid : NULL,
            app->rfid_uid_len,
            sanitized_ndef,  // Use sanitized text
            app->delimiter,
            nfc_first,
            app->output_buffer,
            sizeof(app->output_buffer));
    }

    // Show the output briefly
    hid_device_startscreen_set_uid_text(app->hid_device_startscreen, app->output_buffer);
    hid_device_startscreen_set_display_state(app->hid_device_startscreen, HidDeviceDisplayStateResult);

    // Type the output via HID (with chunking for long text)
    if(hid_device_hid_is_connected(hid_device_get_hid(app))) {
        size_t text_len = strlen(app->output_buffer);

        // If text is long (>100 chars), show progress and type in chunks
        if(text_len > 100) {
            const size_t chunk_size = 100;
            size_t chunks = (text_len + chunk_size - 1) / chunk_size;

            for(size_t i = 0; i < chunks; i++) {
                // Update progress
                char progress_text[32];
                snprintf(progress_text, sizeof(progress_text), "Typing %zu/%zu...", i + 1, chunks);
                hid_device_startscreen_set_status_text(app->hid_device_startscreen, progress_text);

                // Type chunk
                size_t chunk_start = i * chunk_size;
                size_t chunk_len = (chunk_start + chunk_size > text_len) ?
                                   (text_len - chunk_start) : chunk_size;

                char chunk[101];  // 100 + null terminator
                memcpy(chunk, app->output_buffer + chunk_start, chunk_len);
                chunk[chunk_len] = '\0';

                hid_device_hid_type_string(hid_device_get_hid(app), chunk);

                // Small delay between chunks (let HID catch up)
                furi_delay_ms(50);
            }
        } else {
            // Short text, type normally
            hid_device_hid_type_string(hid_device_get_hid(app), app->output_buffer);
        }

        if(app->append_enter) {
            hid_device_hid_press_enter(hid_device_get_hid(app));
        }

        // Log to SD card if enabled
        if(app->log_to_sd) {
            hid_device_log_scan(app->output_buffer);
        }
    }

    // LED feedback (haptic happens later when "Sent" is displayed)
    hid_device_led_set_rgb(app, 0, 255, 0);  // Green flash

    // Start display timer to show result, then "Sent", then cooldown (non-blocking)
    if(app->display_timer) {
        furi_timer_stop(app->display_timer);
    } else {
        app->display_timer = furi_timer_alloc(
            hid_device_scene_startscreen_display_timer_callback,
            FuriTimerTypeOnce,
            app);
    }

    // Clear scanned data
    app->nfc_uid_len = 0;
    app->rfid_uid_len = 0;
    app->ndef_text[0] = '\0';

    // Set state to cooldown to prevent immediate re-scan
    app->scan_state = HidDeviceScanStateCooldown;

    // Timer will handle three stages: 200ms (show result) -> 200ms (show "Sent") -> 300ms (cooldown)
    furi_timer_start(app->display_timer, furi_ms_to_ticks(200));
}

static void hid_device_scene_startscreen_start_scanning(HidDevice* app) {
    // Don't scan if no HID connection
    if(!hid_device_hid_is_connected(hid_device_get_hid(app))) {
        FURI_LOG_D("HidDeviceScene", "start_scanning: no HID connection, skipping");
        return;
    }

    FURI_LOG_I("HidDeviceScene", "start_scanning: mode=%d, current scan_state=%d", app->mode, app->scan_state);

    // Clear previous scan state to ensure fresh start
    app->nfc_error = HidDeviceNfcErrorNone;
    app->nfc_uid_len = 0;
    app->ndef_text[0] = '\0';

    app->scan_state = HidDeviceScanStateScanning;
    // Keep display in Idle state to show mode selector while scanning

    // Start appropriate reader(s) based on mode
    switch(app->mode) {
    case HidDeviceModeNfc:
        // NFC mode: read UID only (no NDEF parsing)
        hid_device_nfc_set_callback(app->nfc, hid_device_scene_startscreen_nfc_callback, app);
        hid_device_nfc_start(app->nfc, false);
        break;
    case HidDeviceModeRfid:
        hid_device_rfid_set_callback(app->rfid, hid_device_scene_startscreen_rfid_callback, app);
        hid_device_rfid_start(app->rfid);
        break;
    case HidDeviceModeNdef:
        // NDEF mode: read and parse NDEF text records only
        hid_device_nfc_set_callback(app->nfc, hid_device_scene_startscreen_nfc_callback, app);
        hid_device_nfc_start(app->nfc, true);
        break;
    case HidDeviceModeNfcThenRfid:
        // Start with NFC (UID only for combo mode)
        hid_device_nfc_set_callback(app->nfc, hid_device_scene_startscreen_nfc_callback, app);
        hid_device_nfc_start(app->nfc, false);
        break;
    case HidDeviceModeRfidThenNfc:
        // Start with RFID
        hid_device_rfid_set_callback(app->rfid, hid_device_scene_startscreen_rfid_callback, app);
        hid_device_rfid_start(app->rfid);
        break;
    default:
        break;
    }
}

static void hid_device_scene_startscreen_stop_scanning(HidDevice* app) {
    FURI_LOG_I("HidDeviceScene", "stop_scanning: current scan_state=%d", app->scan_state);

    hid_device_nfc_stop(app->nfc);
    hid_device_rfid_stop(app->rfid);
    app->scan_state = HidDeviceScanStateIdle;
    hid_device_startscreen_set_display_state(app->hid_device_startscreen, HidDeviceDisplayStateIdle);
    FURI_LOG_D("HidDeviceScene", "stop_scanning: done, scan_state now Idle");
}

void hid_device_scene_startscreen_on_enter(void* context) {
    furi_assert(context);
    HidDevice* app = context;
    hid_device_startscreen_set_callback(
        app->hid_device_startscreen, hid_device_scene_startscreen_callback, app);

    // Set initial mode on the view
    hid_device_startscreen_set_mode(app->hid_device_startscreen, app->mode);

    // Update HID connection status
    hid_device_scene_startscreen_update_status(app);

    // Turn on backlight (but don't enforce - let system timeout work)
    // The app_alloc already turned it on initially, so this is just a wake-up
    notification_message(app->notification, &sequence_display_backlight_on);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidDeviceViewIdStartscreen);

    // Start scanning if HID is connected
    hid_device_scene_startscreen_start_scanning(app);
}

bool hid_device_scene_startscreen_on_event(void* context, SceneManagerEvent event) {
    HidDevice* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case HidDeviceCustomEventModeChange:
            // Stop current scanning and restart with new mode
            hid_device_scene_startscreen_stop_scanning(app);

            // Get the new mode from the view (the view already updated it)
            app->mode = hid_device_startscreen_get_mode(app->hid_device_startscreen);
            FURI_LOG_I("HidDeviceScene", "Mode changed to: %d", app->mode);

            // Save mode to persistent storage
            hid_device_save_settings(app);

            hid_device_scene_startscreen_start_scanning(app);
            consumed = true;
            break;

        case HidDeviceCustomEventNfcDetected:
            // NFC tag detected
            FURI_LOG_I("HidDeviceScene", "Event NfcDetected: mode=%d, scan_state=%d", app->mode, app->scan_state);
            if(app->mode == HidDeviceModeNfc) {
                // Single tag mode - output UID immediately
                FURI_LOG_D("HidDeviceScene", "NFC single mode - stopping and outputting");
                hid_device_scene_startscreen_stop_scanning(app);
                hid_device_scene_startscreen_output_and_reset(app);
            } else if(app->mode == HidDeviceModeNdef) {
                // NDEF mode - check the error status to distinguish between cases
                // We need to retrieve the error from the NFC data that was stored in the callback
                // Since we're in the custom event handler, we need to check what happened

                if(app->ndef_text[0] != '\0') {
                    // NDEF text found - output it
                    FURI_LOG_D("HidDeviceScene", "NDEF mode - NDEF text found, outputting");
                    hid_device_scene_startscreen_stop_scanning(app);
                    hid_device_scene_startscreen_output_and_reset(app);
                } else {
                    // No NDEF text - determine error message based on nfc_error field
                    const char* error_msg;
                    if(app->nfc_error == HidDeviceNfcErrorNotForumCompliant) {
                        error_msg = "Not NFC Forum Compliant";
                        FURI_LOG_D("HidDeviceScene", "NDEF mode - Not NFC Forum compliant (e.g., MIFARE Classic)");
                    } else if(app->nfc_error == HidDeviceNfcErrorUnsupportedType) {
                        error_msg = "Unsupported NFC Forum Type";
                        FURI_LOG_D("HidDeviceScene", "NDEF mode - Unsupported NFC Forum Type");
                    } else if(app->nfc_error == HidDeviceNfcErrorNoTextRecord) {
                        error_msg = "NDEF Not Found";
                        FURI_LOG_D("HidDeviceScene", "NDEF mode - NDEF not found");
                    } else {
                        // Fallback for any other case
                        error_msg = "NDEF Not Found";
                        FURI_LOG_D("HidDeviceScene", "NDEF mode - Unknown error");
                    }

                    // IMPORTANT: Stop the scanner before showing error to prevent conflicts
                    hid_device_scene_startscreen_stop_scanning(app);

                    hid_device_led_set_rgb(app, 255, 0, 0);  // Red flash

                    // Start display timer to show error for 500ms, then clear and continue scanning
                    if(app->display_timer) {
                        furi_timer_stop(app->display_timer);
                    } else {
                        app->display_timer = furi_timer_alloc(
                            hid_device_scene_startscreen_display_timer_callback,
                            FuriTimerTypeOnce,
                            app);
                    }

                    // Show error message
                    hid_device_startscreen_set_uid_text(app->hid_device_startscreen, "");
                    hid_device_startscreen_set_status_text(app->hid_device_startscreen, error_msg);
                    hid_device_startscreen_set_display_state(app->hid_device_startscreen, HidDeviceDisplayStateResult);

                    // Clear data
                    app->nfc_uid_len = 0;
                    app->ndef_text[0] = '\0';

                    // Set state to cooldown to prevent immediate re-scan
                    app->scan_state = HidDeviceScanStateCooldown;

                    // Timer will detect this is an error (via status_text) and skip "Sent" state
                    furi_timer_start(app->display_timer, furi_ms_to_ticks(500));
                }
            } else if(app->mode == HidDeviceModeNfcThenRfid) {
                // Combo mode - now wait for RFID
                hid_device_nfc_stop(app->nfc);
                app->scan_state = HidDeviceScanStateWaitingSecond;
                hid_device_startscreen_set_status_text(app->hid_device_startscreen, "Waiting for RFID...");
                hid_device_startscreen_set_display_state(app->hid_device_startscreen, HidDeviceDisplayStateWaiting);

                // Start RFID scanning
                hid_device_rfid_set_callback(app->rfid, hid_device_scene_startscreen_rfid_callback, app);
                hid_device_rfid_start(app->rfid);

                // TODO: Add timeout timer
            } else if(app->mode == HidDeviceModeRfidThenNfc && app->scan_state == HidDeviceScanStateWaitingSecond) {
                // Got the second tag in combo mode
                hid_device_nfc_stop(app->nfc);
                hid_device_scene_startscreen_output_and_reset(app);
            }
            consumed = true;
            break;

        case HidDeviceCustomEventRfidDetected:
            // RFID tag detected
            FURI_LOG_I("HidDeviceScene", "Event RfidDetected: mode=%d, scan_state=%d", app->mode, app->scan_state);
            if(app->mode == HidDeviceModeRfid) {
                // Single tag mode - output immediately
                FURI_LOG_D("HidDeviceScene", "RFID single/any mode - stopping and outputting");
                hid_device_scene_startscreen_stop_scanning(app);
                hid_device_scene_startscreen_output_and_reset(app);
            } else if(app->mode == HidDeviceModeRfidThenNfc) {
                // Combo mode - now wait for NFC
                hid_device_rfid_stop(app->rfid);
                app->scan_state = HidDeviceScanStateWaitingSecond;
                hid_device_startscreen_set_status_text(app->hid_device_startscreen, "Waiting for NFC...");
                hid_device_startscreen_set_display_state(app->hid_device_startscreen, HidDeviceDisplayStateWaiting);

                // Start NFC scanning (UID only for combo mode)
                hid_device_nfc_set_callback(app->nfc, hid_device_scene_startscreen_nfc_callback, app);
                hid_device_nfc_start(app->nfc, false);

                // TODO: Add timeout timer
            } else if(app->mode == HidDeviceModeNfcThenRfid && app->scan_state == HidDeviceScanStateWaitingSecond) {
                // Got the second tag in combo mode
                hid_device_rfid_stop(app->rfid);
                hid_device_scene_startscreen_output_and_reset(app);
            }
            consumed = true;
            break;

        case HidDeviceCustomEventStartscreenBack:
            hid_device_scene_startscreen_stop_scanning(app);
            notification_message(app->notification, &sequence_reset_red);
            notification_message(app->notification, &sequence_reset_green);
            notification_message(app->notification, &sequence_reset_blue);
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, HidDeviceSceneStartscreen)) {
                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->view_dispatcher);
            }
            consumed = true;
            break;

        case HidDeviceCustomEventOpenSettings:
            // Stop scanning and open Settings
            hid_device_scene_startscreen_stop_scanning(app);
            scene_manager_next_scene(app->scene_manager, HidDeviceSceneSettings);
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Update HID connection status periodically
        hid_device_scene_startscreen_update_status(app);

        // Process NFC state machine (handles scanner->poller transitions and callbacks)
        hid_device_nfc_tick(app->nfc);

        // Check if we should start/stop scanning based on HID connection
        bool connected = hid_device_hid_is_connected(hid_device_get_hid(app));
        if(connected && app->scan_state == HidDeviceScanStateIdle) {
            hid_device_scene_startscreen_start_scanning(app);
        } else if(!connected && app->scan_state != HidDeviceScanStateIdle) {
            hid_device_scene_startscreen_stop_scanning(app);
        }
    }

    return consumed;
}

void hid_device_scene_startscreen_on_exit(void* context) {
    HidDevice* app = context;
    hid_device_scene_startscreen_stop_scanning(app);

    // Stop display timer if running
    if(app->display_timer) {
        furi_timer_stop(app->display_timer);
    }

    // Return backlight to auto mode
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
}
