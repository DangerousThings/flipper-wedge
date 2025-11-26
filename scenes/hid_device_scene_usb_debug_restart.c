#include "../hid_device.h"
#include <gui/modules/dialog_ex.h>

#define RESTART_SCENE_EXIT_EVENT (0xDEADBEEF)

static void hid_device_scene_usb_debug_restart_callback(DialogExResult result, void* context) {
    HidDevice* app = context;

    if(result == DialogExPressCenter) {
        // Exit button pressed - send custom event
        view_dispatcher_send_custom_event(app->view_dispatcher, RESTART_SCENE_EXIT_EVENT);
    }
}

void hid_device_scene_usb_debug_restart_on_enter(void* context) {
    HidDevice* app = context;
    DialogEx* dialog = dialog_ex_alloc();

    dialog_ex_set_header(dialog, "Restart Required", 64, 32, AlignCenter, AlignCenter);

    dialog_ex_set_center_button_text(dialog, "Exit");
    dialog_ex_set_result_callback(dialog, hid_device_scene_usb_debug_restart_callback);
    dialog_ex_set_context(dialog, app);
    dialog_ex_enable_extended_events(dialog);

    view_dispatcher_add_view(
        app->view_dispatcher, HidDeviceViewIdOutputRestart, dialog_ex_get_view(dialog));
    view_dispatcher_switch_to_view(app->view_dispatcher, HidDeviceViewIdOutputRestart);

    // Store dialog in scene state for cleanup
    scene_manager_set_scene_state(
        app->scene_manager, HidDeviceSceneUsbDebugRestart, (uint32_t)dialog);
}

bool hid_device_scene_usb_debug_restart_on_event(void* context, SceneManagerEvent event) {
    HidDevice* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == RESTART_SCENE_EXIT_EVENT) {
            // Exit button pressed - stop both scene manager and view dispatcher
            // This is the official pattern from power_settings_scene_power_off
            scene_manager_stop(app->scene_manager);
            view_dispatcher_stop(app->view_dispatcher);
            consumed = true;
        }
    }

    return consumed;
}

void hid_device_scene_usb_debug_restart_on_exit(void* context) {
    HidDevice* app = context;

    // Retrieve dialog from scene state
    DialogEx* dialog = (DialogEx*)scene_manager_get_scene_state(
        app->scene_manager, HidDeviceSceneUsbDebugRestart);

    if(dialog) {
        // Remove view first, then free dialog (matches bt_pair pattern)
        view_dispatcher_remove_view(app->view_dispatcher, HidDeviceViewIdOutputRestart);
        dialog_ex_free(dialog);
        scene_manager_set_scene_state(app->scene_manager, HidDeviceSceneUsbDebugRestart, 0);
    }
}
