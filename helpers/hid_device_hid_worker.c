#include "hid_device_hid_worker.h"
#include "hid_device_debug.h"

#define TAG "HidDeviceHidWorker"

typedef enum {
    HidDeviceHidWorkerEventStop = (1 << 0),
} HidDeviceHidWorkerEvent;

struct HidDeviceHidWorker {
    HidDeviceHid* hid;
    FuriThread* thread;
    HidDeviceHidWorkerMode mode;
};

static int32_t hid_device_hid_worker_thread(void* context) {
    HidDeviceHidWorker* worker = context;

    FURI_LOG_I(TAG, "Worker thread started, mode=%d", worker->mode);
    hid_device_debug_log(TAG, "Worker thread starting HID init (mode=%d)", worker->mode);

    // Initialize HID interface in worker thread context
    if(worker->mode == HidDeviceHidWorkerModeUsb) {
        hid_device_hid_init_usb(worker->hid);
    } else {
        hid_device_hid_init_ble(worker->hid);
    }

    FURI_LOG_I(TAG, "Worker thread HID initialized, waiting for stop signal");
    hid_device_debug_log(TAG, "Worker thread HID init complete, entering wait loop");

    // Wait for stop signal
    while(true) {
        uint32_t events = furi_thread_flags_wait(
            HidDeviceHidWorkerEventStop,
            FuriFlagWaitAny,
            FuriWaitForever);

        if(events & HidDeviceHidWorkerEventStop) {
            FURI_LOG_I(TAG, "Worker thread received stop signal");
            hid_device_debug_log(TAG, "Worker thread stopping, deiniting HID");
            break;
        }
    }

    // Deinitialize HID interface in worker thread context
    if(worker->mode == HidDeviceHidWorkerModeUsb) {
        hid_device_hid_deinit_usb(worker->hid);
    } else {
        hid_device_hid_deinit_ble(worker->hid);
    }

    FURI_LOG_I(TAG, "Worker thread exiting");
    hid_device_debug_log(TAG, "Worker thread HID deinit complete, exiting");

    return 0;
}

HidDeviceHidWorker* hid_device_hid_worker_alloc(void) {
    HidDeviceHidWorker* worker = malloc(sizeof(HidDeviceHidWorker));

    worker->hid = hid_device_hid_alloc();
    worker->thread = NULL;
    worker->mode = HidDeviceHidWorkerModeUsb;

    return worker;
}

void hid_device_hid_worker_free(HidDeviceHidWorker* worker) {
    furi_assert(worker);

    // Stop worker if running
    if(worker->thread) {
        hid_device_hid_worker_stop(worker);
    }

    hid_device_hid_free(worker->hid);
    free(worker);
}

void hid_device_hid_worker_start(HidDeviceHidWorker* worker, HidDeviceHidWorkerMode mode) {
    furi_assert(worker);
    furi_assert(!worker->thread);  // Don't start if already running

    FURI_LOG_I(TAG, "Starting worker thread with mode=%d", mode);
    hid_device_debug_log(TAG, "Starting worker thread (mode=%d)", mode);

    worker->mode = mode;
    worker->thread = furi_thread_alloc_ex(
        "HidDeviceHidWorker",
        2048,  // Stack size
        hid_device_hid_worker_thread,
        worker);

    furi_thread_start(worker->thread);

    // Small delay to let worker initialize
    furi_delay_ms(100);

    FURI_LOG_I(TAG, "Worker thread started");
}

void hid_device_hid_worker_stop(HidDeviceHidWorker* worker) {
    furi_assert(worker);

    if(!worker->thread) {
        FURI_LOG_W(TAG, "Worker thread not running");
        return;
    }

    FURI_LOG_I(TAG, "Stopping worker thread");
    hid_device_debug_log(TAG, "Signaling worker thread to stop");

    // Signal thread to stop
    furi_thread_flags_set(furi_thread_get_id(worker->thread), HidDeviceHidWorkerEventStop);

    // Wait for thread to exit
    FURI_LOG_D(TAG, "Waiting for worker thread to exit");
    furi_thread_join(worker->thread);

    // Free thread
    furi_thread_free(worker->thread);
    worker->thread = NULL;

    FURI_LOG_I(TAG, "Worker thread stopped");
    hid_device_debug_log(TAG, "Worker thread stopped and cleaned up");
}

HidDeviceHid* hid_device_hid_worker_get_hid(HidDeviceHidWorker* worker) {
    furi_assert(worker);
    return worker->hid;
}

bool hid_device_hid_worker_is_running(HidDeviceHidWorker* worker) {
    furi_assert(worker);
    return (worker->thread != NULL);
}
