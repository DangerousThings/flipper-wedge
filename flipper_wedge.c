#include "hid_device.h"
#include "helpers/hid_device_debug.h"

bool hid_device_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    HidDevice* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

void hid_device_tick_event_callback(void* context) {
    furi_assert(context);
    HidDevice* app = context;

    // Handle pending output mode switch (async to avoid blocking UI thread)
    if(app->output_switch_pending) {
        FURI_LOG_I(TAG, "Tick: Processing pending output mode switch");
        hid_device_debug_log(TAG, "Tick callback executing deferred mode switch");

        hid_device_switch_output_mode(app, app->output_switch_target);
        app->output_switch_pending = false;

        hid_device_debug_log(TAG, "Deferred mode switch complete");
    }

    scene_manager_handle_tick_event(app->scene_manager);
}

//leave app if back button pressed
bool hid_device_navigation_event_callback(void* context) {
    furi_assert(context);
    HidDevice* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

HidDevice* hid_device_app_alloc() {
    HidDevice* app = malloc(sizeof(HidDevice));

    // Initialize debug logging to SD card
    hid_device_debug_init();
    hid_device_debug_log("App", "=== APP STARTING ===");

    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    //Turn backlight on, believe me this makes testing your app easier
    notification_message(app->notification, &sequence_display_backlight_on);

    //Scene additions
    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&hid_device_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, hid_device_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, hid_device_tick_event_callback, 100);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, hid_device_custom_event_callback);
    app->submenu = submenu_alloc();

    // Set defaults
    app->output_mode = HidDeviceOutputUsb;  // Default: USB HID
    app->usb_debug_mode = false;  // Deprecated: kept for backward compatibility

    // Scanning defaults
    app->mode = HidDeviceModeNfc;  // Default: NFC only
    app->mode_startup_behavior = HidDeviceModeStartupRemember;  // Default: Remember last mode
    app->scan_state = HidDeviceScanStateIdle;
    app->delimiter[0] = '\0';  // Empty delimiter by default
    app->append_enter = true;
    app->vibration_level = HidDeviceVibrationMedium;  // Default: Medium vibration
    app->ndef_max_len = HidDeviceNdefMaxLen250;  // Default: 250 char limit (fast typing)
    app->log_to_sd = false;  // Default: Logging disabled for privacy/performance
    app->restart_pending = false;  // Deprecated field, no longer used
    app->output_switch_pending = false;
    app->output_switch_target = HidDeviceOutputUsb;

    // Clear scanned data
    app->nfc_uid_len = 0;
    app->rfid_uid_len = 0;
    app->ndef_text[0] = '\0';
    app->output_buffer[0] = '\0';

    // Used for File Browser
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->file_path = furi_string_alloc();

    // Load configs BEFORE initializing HID (so we respect output_mode setting)
    hid_device_read_settings(app);

    // Allocate HID worker (manages HID interface in separate thread)
    app->hid_worker = hid_device_hid_worker_alloc();

    // Start HID worker with loaded output mode (like Bad USB pattern)
    hid_device_debug_log("App", "Starting HID worker in %s mode",
                        app->output_mode == HidDeviceOutputUsb ? "USB" : "BLE");
    HidDeviceHidWorkerMode worker_mode = (app->output_mode == HidDeviceOutputUsb) ?
        HidDeviceHidWorkerModeUsb : HidDeviceHidWorkerModeBle;
    hid_device_hid_worker_start(app->hid_worker, worker_mode);

    // Allocate NFC module
    app->nfc = hid_device_nfc_alloc();

    // Allocate RFID module
    app->rfid = hid_device_rfid_alloc();

    // Timers will be created as needed
    app->timeout_timer = NULL;
    app->display_timer = NULL;

    view_dispatcher_add_view(
        app->view_dispatcher, HidDeviceViewIdMenu, submenu_get_view(app->submenu));
    app->hid_device_startscreen = hid_device_startscreen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidDeviceViewIdStartscreen,
        hid_device_startscreen_get_view(app->hid_device_startscreen));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidDeviceViewIdSettings,
        variable_item_list_get_view(app->variable_item_list));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidDeviceViewIdTextInput,
        text_input_get_view(app->text_input));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidDeviceViewIdNumberInput,
        number_input_get_view(app->number_input));

    //End Scene Additions

    return app;
}

void hid_device_switch_output_mode(HidDevice* app, HidDeviceOutput new_mode) {
    furi_assert(app);

    // Note: app->output_mode may already equal new_mode (set by settings callback)
    // but we still need to restart the HID worker with the new profile
    FURI_LOG_I(TAG, "Switching output mode to: %d", new_mode);
    hid_device_debug_log(TAG, "=== OUTPUT MODE SWITCH: %d -> %d ===",
                        app->output_mode, new_mode);

    // STEP 1: Stop all workers (NFC/RFID)
    hid_device_debug_log(TAG, "Step 1: Stopping NFC/RFID workers");
    bool nfc_was_scanning = hid_device_nfc_is_scanning(app->nfc);
    bool rfid_was_scanning = hid_device_rfid_is_scanning(app->rfid);
    bool parse_ndef = (app->mode == HidDeviceModeNdef);

    if(nfc_was_scanning) {
        hid_device_nfc_stop(app->nfc);
        hid_device_debug_log(TAG, "NFC stopped");
    }
    if(rfid_was_scanning) {
        hid_device_rfid_stop(app->rfid);
        hid_device_debug_log(TAG, "RFID stopped");
    }

    // STEP 2: Stop HID worker (deinits HID in worker thread, waits for exit)
    hid_device_debug_log(TAG, "Step 2: Stopping HID worker (old mode=%s)",
                        app->output_mode == HidDeviceOutputUsb ? "USB" : "BLE");
    hid_device_hid_worker_stop(app->hid_worker);
    hid_device_debug_log(TAG, "HID worker stopped");

    // STEP 3: Small delay between modes (safety buffer)
    hid_device_debug_log(TAG, "Step 3: Waiting 300ms before starting new mode");
    furi_delay_ms(300);

    // STEP 4: Switch mode
    hid_device_debug_log(TAG, "Step 4: Switching mode");
    app->output_mode = new_mode;

    // STEP 5: Start HID worker with new mode (inits HID in worker thread)
    hid_device_debug_log(TAG, "Step 5: Starting HID worker (new mode=%s)",
                        new_mode == HidDeviceOutputUsb ? "USB" : "BLE");
    HidDeviceHidWorkerMode worker_mode = (new_mode == HidDeviceOutputUsb) ?
        HidDeviceHidWorkerModeUsb : HidDeviceHidWorkerModeBle;
    hid_device_hid_worker_start(app->hid_worker, worker_mode);
    hid_device_debug_log(TAG, "HID worker started");

    // STEP 6: Restart NFC/RFID workers if they were running
    hid_device_debug_log(TAG, "Step 6: Restarting NFC/RFID workers (NFC=%d, RFID=%d)",
                        nfc_was_scanning, rfid_was_scanning);
    if(nfc_was_scanning) {
        hid_device_nfc_start(app->nfc, parse_ndef);
        hid_device_debug_log(TAG, "NFC restarted");
    }
    if(rfid_was_scanning) {
        hid_device_rfid_start(app->rfid);
        hid_device_debug_log(TAG, "RFID restarted");
    }

    // STEP 7: Save settings to persist the change
    hid_device_debug_log(TAG, "Step 7: Saving settings");
    hid_device_save_settings(app);

    FURI_LOG_I(TAG, "Output mode switch complete");
    hid_device_debug_log(TAG, "=== OUTPUT MODE SWITCH COMPLETE ===");
}

void hid_device_app_free(HidDevice* app) {
    furi_assert(app);

    // Free timers
    if(app->timeout_timer) {
        furi_timer_free(app->timeout_timer);
        app->timeout_timer = NULL;
    }
    if(app->display_timer) {
        furi_timer_free(app->display_timer);
        app->display_timer = NULL;
    }

    // Free RFID module
    if(app->rfid) {
        hid_device_rfid_free(app->rfid);
        app->rfid = NULL;
    }

    // Free NFC module
    if(app->nfc) {
        hid_device_nfc_free(app->nfc);
        app->nfc = NULL;
    }

    // Free HID worker (stops thread and cleans up HID)
    hid_device_hid_worker_free(app->hid_worker);

    // Scene manager
    scene_manager_free(app->scene_manager);

    // View Dispatcher
    view_dispatcher_remove_view(app->view_dispatcher, HidDeviceViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, HidDeviceViewIdSettings);
    view_dispatcher_remove_view(app->view_dispatcher, HidDeviceViewIdStartscreen);
    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    hid_device_startscreen_free(app->hid_device_startscreen);

    view_dispatcher_remove_view(app->view_dispatcher, HidDeviceViewIdNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, HidDeviceViewIdTextInput);
    text_input_free(app->text_input);

    view_dispatcher_free(app->view_dispatcher);

    // Restore backlight to auto mode before closing notification service
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    app->gui = NULL;
    app->notification = NULL;

    // Close File Browser
    furi_record_close(RECORD_DIALOGS);
    furi_string_free(app->file_path);

    // Close debug logging
    hid_device_debug_log("App", "=== APP EXITING ===");
    hid_device_debug_close();

    //Remove whatever is left
    free(app);
}

int32_t hid_device_app(void* p) {
    UNUSED(p);
    HidDevice* app = hid_device_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(
        app->scene_manager, HidDeviceSceneStartscreen); //Start with start screen

    furi_hal_power_suppress_charge_enter();

    view_dispatcher_run(app->view_dispatcher);

    hid_device_save_settings(app);

    furi_hal_power_suppress_charge_exit();
    hid_device_app_free(app);

    return 0;
}
