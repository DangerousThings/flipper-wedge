#pragma once

#include <furi.h>
#include <lfrfid/lfrfid_worker.h>

#define HID_DEVICE_RFID_UID_MAX_LEN 8

typedef struct HidDeviceRfid HidDeviceRfid;

typedef struct {
    uint8_t uid[HID_DEVICE_RFID_UID_MAX_LEN];
    uint8_t uid_len;
    char protocol_name[32];
} HidDeviceRfidData;

typedef void (*HidDeviceRfidCallback)(HidDeviceRfidData* data, void* context);

/** Allocate RFID reader
 *
 * @return HidDeviceRfid instance
 */
HidDeviceRfid* hid_device_rfid_alloc(void);

/** Free RFID reader
 *
 * @param instance HidDeviceRfid instance
 */
void hid_device_rfid_free(HidDeviceRfid* instance);

/** Set callback for tag detection
 *
 * @param instance HidDeviceRfid instance
 * @param callback Callback function
 * @param context Callback context
 */
void hid_device_rfid_set_callback(
    HidDeviceRfid* instance,
    HidDeviceRfidCallback callback,
    void* context);

/** Start RFID scanning
 *
 * @param instance HidDeviceRfid instance
 */
void hid_device_rfid_start(HidDeviceRfid* instance);

/** Stop RFID scanning
 *
 * @param instance HidDeviceRfid instance
 */
void hid_device_rfid_stop(HidDeviceRfid* instance);

/** Check if RFID is currently scanning
 *
 * @param instance HidDeviceRfid instance
 * @return true if scanning
 */
bool hid_device_rfid_is_scanning(HidDeviceRfid* instance);
