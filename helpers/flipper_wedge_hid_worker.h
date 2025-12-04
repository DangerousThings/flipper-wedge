#pragma once

#include <furi.h>
#include "hid_device_hid.h"

typedef struct HidDeviceHidWorker HidDeviceHidWorker;

typedef enum {
    HidDeviceHidWorkerModeUsb,
    HidDeviceHidWorkerModeBle,
} HidDeviceHidWorkerMode;

/** Allocate HID worker
 * Worker thread owns the HID interface lifecycle
 *
 * @return HidDeviceHidWorker instance
 */
HidDeviceHidWorker* hid_device_hid_worker_alloc(void);

/** Free HID worker
 * Stops worker thread and cleans up HID interface
 *
 * @param worker HidDeviceHidWorker instance
 */
void hid_device_hid_worker_free(HidDeviceHidWorker* worker);

/** Start HID worker with specified mode
 * Creates worker thread that initializes HID interface
 *
 * @param worker HidDeviceHidWorker instance
 * @param mode USB or BLE mode
 */
void hid_device_hid_worker_start(HidDeviceHidWorker* worker, HidDeviceHidWorkerMode mode);

/** Stop HID worker
 * Signals worker thread to exit and deinit HID interface
 * Blocks until worker thread exits
 *
 * @param worker HidDeviceHidWorker instance
 */
void hid_device_hid_worker_stop(HidDeviceHidWorker* worker);

/** Get HID instance from worker
 * Returns the HID interface managed by the worker
 *
 * @param worker HidDeviceHidWorker instance
 * @return HidDeviceHid instance
 */
HidDeviceHid* hid_device_hid_worker_get_hid(HidDeviceHidWorker* worker);

/** Check if worker is running
 *
 * @param worker HidDeviceHidWorker instance
 * @return true if worker thread is active
 */
bool hid_device_hid_worker_is_running(HidDeviceHidWorker* worker);
