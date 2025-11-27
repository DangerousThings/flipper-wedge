#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

#define HID_DEVICE_NFC_UID_MAX_LEN 10
#define HID_DEVICE_NDEF_MAX_LEN 1024  // Buffer size (max user setting is 1000 chars, +24 for safety)

typedef struct HidDeviceNfc HidDeviceNfc;

typedef enum {
    HidDeviceNfcErrorNone,            // Success
    HidDeviceNfcErrorNotForumCompliant, // Tag is not NFC Forum compliant (e.g., MIFARE Classic)
    HidDeviceNfcErrorUnsupportedType, // Tag detected but unsupported NFC Forum Type for NDEF
    HidDeviceNfcErrorNoTextRecord,    // Supported type but no NDEF text record found
} HidDeviceNfcError;

typedef struct {
    uint8_t uid[HID_DEVICE_NFC_UID_MAX_LEN];
    uint8_t uid_len;
    char ndef_text[HID_DEVICE_NDEF_MAX_LEN];
    bool has_ndef;
    HidDeviceNfcError error;
} HidDeviceNfcData;

typedef void (*HidDeviceNfcCallback)(HidDeviceNfcData* data, void* context);

/** Allocate NFC reader
 *
 * @return HidDeviceNfc instance
 */
HidDeviceNfc* hid_device_nfc_alloc(void);

/** Free NFC reader
 *
 * @param instance HidDeviceNfc instance
 */
void hid_device_nfc_free(HidDeviceNfc* instance);

/** Set callback for tag detection
 *
 * @param instance HidDeviceNfc instance
 * @param callback Callback function
 * @param context Callback context
 */
void hid_device_nfc_set_callback(
    HidDeviceNfc* instance,
    HidDeviceNfcCallback callback,
    void* context);

/** Start NFC scanning
 *
 * @param instance HidDeviceNfc instance
 * @param parse_ndef Whether to parse NDEF records
 */
void hid_device_nfc_start(HidDeviceNfc* instance, bool parse_ndef);

/** Stop NFC scanning
 *
 * @param instance HidDeviceNfc instance
 */
void hid_device_nfc_stop(HidDeviceNfc* instance);

/** Check if NFC is currently scanning
 *
 * @param instance HidDeviceNfc instance
 * @return true if scanning
 */
bool hid_device_nfc_is_scanning(HidDeviceNfc* instance);

/** Process NFC state machine from main thread
 * Call this in the tick event handler to safely process NFC events
 *
 * @param instance HidDeviceNfc instance
 * @return true if a tag was successfully read
 */
bool hid_device_nfc_tick(HidDeviceNfc* instance);
