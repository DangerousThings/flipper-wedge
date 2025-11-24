#include "hid_reader_nfc.h"
#include <furi_hal.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#define TAG "HidReaderNfc"

typedef enum {
    HidReaderNfcStateIdle,
    HidReaderNfcStateScanning,
    HidReaderNfcStateTagDetected,  // Scanner detected tag, need to switch to poller
    HidReaderNfcStatePolling,
    HidReaderNfcStateSuccess,
} HidReaderNfcState;

struct HidReaderNfc {
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;

    HidReaderNfcState state;
    bool parse_ndef;
    NfcProtocol detected_protocol;

    HidReaderNfcCallback callback;
    void* callback_context;

    HidReaderNfcData last_data;

    // Thread-safe signaling
    FuriThreadId owner_thread;
};

static NfcCommand hid_reader_nfc_poller_callback_iso14443_3a(NfcGenericEvent event, void* context) {
    furi_assert(context);
    HidReaderNfc* instance = context;

    FURI_LOG_D(TAG, "3A callback: protocol=%d", event.protocol);

    if(event.protocol == NfcProtocolIso14443_3a) {
        const Iso14443_3aPollerEvent* iso3a_event = event.event_data;
        FURI_LOG_D(TAG, "3A event type: %d", iso3a_event->type);

        if(iso3a_event->type == Iso14443_3aPollerEventTypeReady) {
            const Iso14443_3aData* iso3a_data = nfc_poller_get_data(instance->poller);
            if(iso3a_data) {
                // Validate UID length first
                uint8_t uid_len = iso3a_data->uid_len;
                if(uid_len > HID_READER_NFC_UID_MAX_LEN) {
                    uid_len = HID_READER_NFC_UID_MAX_LEN;
                }
                if(uid_len > 0) {
                    instance->last_data.uid_len = uid_len;
                    memcpy(instance->last_data.uid, iso3a_data->uid, uid_len);
                    instance->last_data.has_ndef = false;
                    instance->last_data.ndef_text[0] = '\0';

                    FURI_LOG_I(TAG, "Got ISO14443-3A UID, len: %d", instance->last_data.uid_len);
                    instance->state = HidReaderNfcStateSuccess;

                    // Signal the owner thread that we have data
                    // The callback will be invoked from the main thread via polling
                }
            }
            return NfcCommandStop;
        } else if(iso3a_event->type == Iso14443_3aPollerEventTypeError) {
            FURI_LOG_E(TAG, "3A poller error");
            return NfcCommandStop;
        }
    }
    return NfcCommandContinue;
}

static NfcCommand hid_reader_nfc_poller_callback_iso14443_4a(NfcGenericEvent event, void* context) {
    furi_assert(context);
    HidReaderNfc* instance = context;

    FURI_LOG_D(TAG, "4A callback: protocol=%d", event.protocol);

    // ISO14443-4A pollers receive both 3A and 4A events
    // We only care about the 4A Ready event which means the full handshake is done
    if(event.protocol == NfcProtocolIso14443_4a) {
        const Iso14443_4aPollerEvent* iso4a_event = event.event_data;
        FURI_LOG_D(TAG, "4A event type: %d", iso4a_event->type);

        if(iso4a_event->type == Iso14443_4aPollerEventTypeReady) {
            const Iso14443_4aData* iso4a_data = nfc_poller_get_data(instance->poller);
            FURI_LOG_D(TAG, "4A data ptr: %p", (void*)iso4a_data);

            if(iso4a_data) {
                // Get the 3a base data which contains the UID
                const Iso14443_3aData* iso3a_data = iso4a_data->iso14443_3a_data;
                FURI_LOG_D(TAG, "3A data ptr: %p", (void*)iso3a_data);

                if(iso3a_data) {
                    // Validate UID length first
                    uint8_t uid_len = iso3a_data->uid_len;
                    FURI_LOG_D(TAG, "UID len: %d", uid_len);

                    if(uid_len > HID_READER_NFC_UID_MAX_LEN) {
                        uid_len = HID_READER_NFC_UID_MAX_LEN;
                    }
                    if(uid_len > 0) {
                        instance->last_data.uid_len = uid_len;
                        memcpy(instance->last_data.uid, iso3a_data->uid, uid_len);
                        instance->last_data.has_ndef = false;
                        instance->last_data.ndef_text[0] = '\0';

                        FURI_LOG_I(TAG, "Got ISO14443-4A UID, len: %d", instance->last_data.uid_len);
                        instance->state = HidReaderNfcStateSuccess;
                    }
                } else {
                    FURI_LOG_E(TAG, "4A data has NULL 3A pointer");
                }
            } else {
                FURI_LOG_E(TAG, "4A poller returned NULL data");
            }
            return NfcCommandStop;
        } else if(iso4a_event->type == Iso14443_4aPollerEventTypeError) {
            FURI_LOG_E(TAG, "4A poller error");
            return NfcCommandStop;
        }
    } else if(event.protocol == NfcProtocolIso14443_3a) {
        // Ignore 3A events from the 4A poller - just continue
        FURI_LOG_D(TAG, "4A poller got 3A event, continuing...");
    }
    return NfcCommandContinue;
}

static void hid_reader_nfc_scanner_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    HidReaderNfc* instance = context;

    if(event.type == NfcScannerEventTypeDetected) {
        FURI_LOG_I(TAG, "NFC tag detected, protocols: %zu", event.data.protocol_num);

        // Find the best protocol to use
        // For UID reading, we need ISO14443-3A or ISO14443-4A
        // MIFARE tags (Ultralight, Classic, etc.) are built on ISO14443-3A but may not expose it
        NfcProtocol protocol_to_use = NfcProtocolInvalid;

        // First check for base protocols we can poll directly
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            NfcProtocol p = event.data.protocols[i];
            FURI_LOG_I(TAG, "  Protocol %zu: %d", i, p);

            // Prefer ISO14443-3A for simple UID reading
            if(p == NfcProtocolIso14443_3a) {
                protocol_to_use = p;
                break;
            }
            // Fall back to ISO14443-4A if available
            if(p == NfcProtocolIso14443_4a && protocol_to_use == NfcProtocolInvalid) {
                protocol_to_use = p;
            }
        }

        // If we didn't find a base protocol, try to get the parent protocol of higher-level protocols
        if(protocol_to_use == NfcProtocolInvalid && event.data.protocol_num > 0) {
            // Check each detected protocol for a parent we can use
            for(size_t i = 0; i < event.data.protocol_num; i++) {
                NfcProtocol p = event.data.protocols[i];
                NfcProtocol parent = nfc_protocol_get_parent(p);
                FURI_LOG_I(TAG, "  Protocol %d has parent: %d", p, parent);

                if(parent == NfcProtocolIso14443_3a) {
                    protocol_to_use = parent;
                    FURI_LOG_I(TAG, "Using parent protocol ISO14443-3A for UID reading");
                    break;
                } else if(parent == NfcProtocolIso14443_4a && protocol_to_use == NfcProtocolInvalid) {
                    protocol_to_use = parent;
                    FURI_LOG_I(TAG, "Using parent protocol ISO14443-4A for UID reading");
                }
            }
        }

        if(protocol_to_use != NfcProtocolInvalid) {
            // Just mark that we detected a tag and store the protocol
            // The main thread will handle the scanner->poller transition
            instance->detected_protocol = protocol_to_use;
            instance->state = HidReaderNfcStateTagDetected;
            FURI_LOG_I(TAG, "Tag detected, protocol %d, waiting for main thread", protocol_to_use);
        } else {
            FURI_LOG_W(TAG, "No supported protocol found - cannot read UID");
        }
    }
}

// Internal function to switch from scanner to poller
static void hid_reader_nfc_start_poller(HidReaderNfc* instance) {
    furi_assert(instance);

    // Stop and free scanner
    if(instance->scanner) {
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    // Start poller for the detected protocol
    instance->poller = nfc_poller_alloc(instance->nfc, instance->detected_protocol);
    if(instance->poller) {
        instance->state = HidReaderNfcStatePolling;
        if(instance->detected_protocol == NfcProtocolIso14443_3a) {
            nfc_poller_start(instance->poller, hid_reader_nfc_poller_callback_iso14443_3a, instance);
        } else if(instance->detected_protocol == NfcProtocolIso14443_4a) {
            nfc_poller_start(instance->poller, hid_reader_nfc_poller_callback_iso14443_4a, instance);
        }
        FURI_LOG_I(TAG, "Started poller for protocol %d", instance->detected_protocol);
    } else {
        FURI_LOG_E(TAG, "Failed to allocate poller");
        instance->state = HidReaderNfcStateIdle;
    }
}

HidReaderNfc* hid_reader_nfc_alloc(void) {
    HidReaderNfc* instance = malloc(sizeof(HidReaderNfc));

    instance->nfc = nfc_alloc();
    instance->scanner = NULL;
    instance->poller = NULL;
    instance->state = HidReaderNfcStateIdle;
    instance->parse_ndef = false;
    instance->detected_protocol = NfcProtocolInvalid;
    instance->callback = NULL;
    instance->callback_context = NULL;
    instance->owner_thread = furi_thread_get_current_id();

    memset(&instance->last_data, 0, sizeof(HidReaderNfcData));

    FURI_LOG_I(TAG, "NFC reader allocated");

    return instance;
}

void hid_reader_nfc_free(HidReaderNfc* instance) {
    furi_assert(instance);

    hid_reader_nfc_stop(instance);

    if(instance->nfc) {
        nfc_free(instance->nfc);
        instance->nfc = NULL;
    }

    free(instance);
    FURI_LOG_I(TAG, "NFC reader freed");
}

void hid_reader_nfc_set_callback(
    HidReaderNfc* instance,
    HidReaderNfcCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

void hid_reader_nfc_start(HidReaderNfc* instance, bool parse_ndef) {
    furi_assert(instance);

    FURI_LOG_I(TAG, "NFC start called, current state=%d, scanner=%p, poller=%p",
               instance->state, (void*)instance->scanner, (void*)instance->poller);

    if(instance->state != HidReaderNfcStateIdle) {
        FURI_LOG_W(TAG, "Already scanning, state=%d", instance->state);
        return;
    }

    // Defensive cleanup - ensure no stale scanner/poller
    if(instance->poller) {
        FURI_LOG_W(TAG, "Stale poller found, cleaning up");
        nfc_poller_stop(instance->poller);
        nfc_poller_free(instance->poller);
        instance->poller = NULL;
    }
    if(instance->scanner) {
        FURI_LOG_W(TAG, "Stale scanner found, cleaning up");
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    instance->parse_ndef = parse_ndef;
    instance->detected_protocol = NfcProtocolInvalid;
    memset(&instance->last_data, 0, sizeof(HidReaderNfcData));

    // Create and start scanner
    instance->scanner = nfc_scanner_alloc(instance->nfc);
    if(!instance->scanner) {
        FURI_LOG_E(TAG, "Failed to allocate NFC scanner!");
        return;
    }

    nfc_scanner_start(instance->scanner, hid_reader_nfc_scanner_callback, instance);

    instance->state = HidReaderNfcStateScanning;
    FURI_LOG_I(TAG, "NFC scanning started (NDEF: %s), scanner=%p", parse_ndef ? "ON" : "OFF", (void*)instance->scanner);
}

void hid_reader_nfc_stop(HidReaderNfc* instance) {
    furi_assert(instance);

    FURI_LOG_I(TAG, "NFC stop called, state=%d", instance->state);

    if(instance->poller) {
        FURI_LOG_D(TAG, "Stopping poller");
        nfc_poller_stop(instance->poller);
        nfc_poller_free(instance->poller);
        instance->poller = NULL;
    }

    if(instance->scanner) {
        FURI_LOG_D(TAG, "Stopping scanner");
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    // Reset all state
    instance->state = HidReaderNfcStateIdle;
    instance->detected_protocol = NfcProtocolInvalid;

    FURI_LOG_I(TAG, "NFC scanning stopped, state now Idle");
}

bool hid_reader_nfc_is_scanning(HidReaderNfc* instance) {
    furi_assert(instance);
    return instance->state == HidReaderNfcStateScanning ||
           instance->state == HidReaderNfcStateTagDetected ||
           instance->state == HidReaderNfcStatePolling;
}

// Call this from the main thread's tick handler to process NFC events
// Returns true if a tag was successfully read (data available in last_data)
bool hid_reader_nfc_tick(HidReaderNfc* instance) {
    furi_assert(instance);

    if(instance->state == HidReaderNfcStateTagDetected) {
        // Scanner detected a tag, switch to poller (safe to do from main thread)
        FURI_LOG_I(TAG, "Tick: starting poller for detected tag, protocol=%d", instance->detected_protocol);
        hid_reader_nfc_start_poller(instance);
        return false;
    }

    if(instance->state == HidReaderNfcStateSuccess) {
        // Poller got the UID, invoke callback
        FURI_LOG_I(TAG, "Tick: tag read success, UID len=%d, invoking callback", instance->last_data.uid_len);

        // Stop the poller first
        if(instance->poller) {
            FURI_LOG_D(TAG, "Tick: stopping poller");
            nfc_poller_stop(instance->poller);
            nfc_poller_free(instance->poller);
            instance->poller = NULL;
        }

        // Reset state to Idle BEFORE calling callback
        // This ensures the NFC module is ready for restart
        instance->state = HidReaderNfcStateIdle;
        instance->detected_protocol = NfcProtocolInvalid;
        FURI_LOG_D(TAG, "Tick: state reset to Idle");

        // Call the callback from main thread (safe!)
        if(instance->callback) {
            FURI_LOG_D(TAG, "Tick: calling callback");
            instance->callback(&instance->last_data, instance->callback_context);
            FURI_LOG_D(TAG, "Tick: callback returned");
        }
        return true;
    }

    return false;
}
