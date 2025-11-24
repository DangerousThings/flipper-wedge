#include "hid_reader.h"

bool hid_reader_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    HidReader* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

void hid_reader_tick_event_callback(void* context) {
    furi_assert(context);
    HidReader* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

//leave app if back button pressed
bool hid_reader_navigation_event_callback(void* context) {
    furi_assert(context);
    HidReader* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

HidReader* hid_reader_app_alloc() {
    HidReader* app = malloc(sizeof(HidReader));
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    //Turn backlight on, believe me this makes testing your app easier
    notification_message(app->notification, &sequence_display_backlight_on);

    //Scene additions
    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&hid_reader_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, hid_reader_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, hid_reader_tick_event_callback, 100);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, hid_reader_custom_event_callback);
    app->submenu = submenu_alloc();

    // Set defaults, in case no config loaded
    app->haptic = 1;
    app->speaker = 1;
    app->led = 1;
    app->save_settings = 1;
    app->bt_enabled = true;

    // Scanning defaults
    app->mode = HidReaderModeAny;  // Default: Any tag (NFC or RFID)
    app->scan_state = HidReaderScanStateIdle;
    app->delimiter[0] = '\0';  // Empty delimiter by default
    app->append_enter = true;
    app->ndef_enabled = true;

    // Clear scanned data
    app->nfc_uid_len = 0;
    app->rfid_uid_len = 0;
    app->ndef_text[0] = '\0';
    app->output_buffer[0] = '\0';

    // Allocate and start HID module
    bool usb_hid_enabled = false;  // Disabled for serial debugging
    app->hid = hid_reader_hid_alloc();
    hid_reader_hid_start(app->hid, usb_hid_enabled, app->bt_enabled);

    // Allocate NFC module
    app->nfc = hid_reader_nfc_alloc();

    // Allocate RFID module
    app->rfid = hid_reader_rfid_alloc();

    // Timers will be created as needed
    app->timeout_timer = NULL;
    app->display_timer = NULL;

    // Used for File Browser
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->file_path = furi_string_alloc();

    // Load configs
    hid_reader_read_settings(app);

    view_dispatcher_add_view(
        app->view_dispatcher, HidReaderViewIdMenu, submenu_get_view(app->submenu));
    app->hid_reader_startscreen = hid_reader_startscreen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidReaderViewIdStartscreen,
        hid_reader_startscreen_get_view(app->hid_reader_startscreen));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidReaderViewIdSettings,
        variable_item_list_get_view(app->variable_item_list));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidReaderViewIdTextInput,
        text_input_get_view(app->text_input));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidReaderViewIdNumberInput,
        number_input_get_view(app->number_input));

    //End Scene Additions

    return app;
}

void hid_reader_app_free(HidReader* app) {
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
        hid_reader_rfid_free(app->rfid);
        app->rfid = NULL;
    }

    // Free NFC module
    if(app->nfc) {
        hid_reader_nfc_free(app->nfc);
        app->nfc = NULL;
    }

    // Free HID module
    hid_reader_hid_free(app->hid);

    // Scene manager
    scene_manager_free(app->scene_manager);

    // View Dispatcher
    view_dispatcher_remove_view(app->view_dispatcher, HidReaderViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, HidReaderViewIdSettings);
    view_dispatcher_remove_view(app->view_dispatcher, HidReaderViewIdStartscreen);
    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    hid_reader_startscreen_free(app->hid_reader_startscreen);

    view_dispatcher_remove_view(app->view_dispatcher, HidReaderViewIdNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, HidReaderViewIdTextInput);
    text_input_free(app->text_input);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    app->gui = NULL;
    app->notification = NULL;

    // Close File Browser
    furi_record_close(RECORD_DIALOGS);
    furi_string_free(app->file_path);

    //Remove whatever is left
    free(app);
}

int32_t hid_reader_app(void* p) {
    UNUSED(p);
    HidReader* app = hid_reader_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(
        app->scene_manager, HidReaderSceneStartscreen); //Start with start screen

    furi_hal_power_suppress_charge_enter();

    view_dispatcher_run(app->view_dispatcher);

    hid_reader_save_settings(app);

    furi_hal_power_suppress_charge_exit();
    hid_reader_app_free(app);

    return 0;
}
