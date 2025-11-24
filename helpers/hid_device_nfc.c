#include "hid_device_nfc.h"
#include <furi_hal.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

#define TAG "HidDeviceNfc"

typedef enum {
    HidDeviceNfcStateIdle,
    HidDeviceNfcStateScanning,
    HidDeviceNfcStateTagDetected,  // Scanner detected tag, need to switch to poller
    HidDeviceNfcStatePolling,
    HidDeviceNfcStateSuccess,
} HidDeviceNfcState;

struct HidDeviceNfc {
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;

    HidDeviceNfcState state;
    bool parse_ndef;
    NfcProtocol detected_protocol;

    HidDeviceNfcCallback callback;
    void* callback_context;

    HidDeviceNfcData last_data;

    // Thread-safe signaling
    FuriThreadId owner_thread;
};

// Simple NDEF text record parser
// Returns number of bytes written to output, 0 if no text records found
static size_t hid_device_nfc_parse_ndef_text(const uint8_t* data, size_t data_len, char* output, size_t output_max) {
    if(!data || !output || data_len < 4 || output_max == 0) {
        return 0;
    }

    size_t output_pos = 0;
    size_t pos = 0;

    // Look for NDEF Message TLV (Type=0x03)
    while(pos < data_len - 1) {
        uint8_t tlv_type = data[pos++];

        // Skip padding
        if(tlv_type == 0x00) continue;

        // Terminator found
        if(tlv_type == 0xFE) break;

        // Get length
        if(pos >= data_len) break;
        uint32_t tlv_len = data[pos++];
        if(tlv_len == 0xFF) {
            // 3-byte length
            if(pos + 2 > data_len) break;
            tlv_len = (data[pos] << 8) | data[pos + 1];
            pos += 2;
        }

        // Check if this is an NDEF message
        if(tlv_type == 0x03 && tlv_len > 0 && pos + tlv_len <= data_len) {
            // Parse NDEF message records
            size_t msg_end = pos + tlv_len;

            while(pos < msg_end && pos < data_len) {
                // Read record header
                if(pos + 1 > data_len) break;
                uint8_t flags_tnf = data[pos++];

                uint8_t tnf = flags_tnf & 0x07;
                bool short_record = (flags_tnf & 0x10) != 0;
                bool id_length_present = (flags_tnf & 0x08) != 0;

                // Read type length
                if(pos >= data_len) break;
                uint8_t type_len = data[pos++];

                // Read payload length
                if(pos >= data_len) break;
                uint32_t payload_len;
                if(short_record) {
                    payload_len = data[pos++];
                } else {
                    if(pos + 4 > data_len) break;
                    payload_len = (data[pos] << 24) | (data[pos + 1] << 16) |
                                 (data[pos + 2] << 8) | data[pos + 3];
                    pos += 4;
                }

                // Read ID length if present
                uint8_t id_len = 0;
                if(id_length_present) {
                    if(pos >= data_len) break;
                    id_len = data[pos++];
                }

                // Read type
                if(pos + type_len > data_len) break;
                const uint8_t* type = &data[pos];
                pos += type_len;

                // Skip ID
                if(pos + id_len > data_len) break;
                pos += id_len;

                // Read payload
                if(pos + payload_len > data_len) break;
                const uint8_t* payload = &data[pos];
                pos += payload_len;

                // Check if this is a text record (TNF=0x01, Type='T')
                if(tnf == 0x01 && type_len == 1 && type[0] == 'T' && payload_len > 1) {
                    // Text record format: [status byte][language code][text]
                    uint8_t status = payload[0];
                    uint8_t lang_len = status & 0x3F;

                    if((uint32_t)(lang_len + 1) <= payload_len) {
                        // Skip language code, extract text
                        const uint8_t* text = &payload[1 + lang_len];
                        size_t text_len = payload_len - 1 - lang_len;

                        // Copy text to output
                        size_t copy_len = text_len;
                        if(output_pos + copy_len >= output_max) {
                            copy_len = output_max - output_pos - 1;
                        }
                        if(copy_len > 0) {
                            memcpy(&output[output_pos], text, copy_len);
                            output_pos += copy_len;
                        }
                    }
                }
            }

            // Found and processed NDEF message, stop searching
            break;
        } else {
            // Skip this TLV
            pos += tlv_len;
        }
    }

    // Null-terminate
    if(output_pos < output_max) {
        output[output_pos] = '\0';
    } else if(output_max > 0) {
        output[output_max - 1] = '\0';
    }

    return output_pos;
}

static NfcCommand hid_device_nfc_poller_callback_iso14443_3a(NfcGenericEvent event, void* context) {
    furi_assert(context);
    HidDeviceNfc* instance = context;

    FURI_LOG_D(TAG, "3A callback: protocol=%d", event.protocol);

    if(event.protocol == NfcProtocolIso14443_3a) {
        const Iso14443_3aPollerEvent* iso3a_event = event.event_data;
        FURI_LOG_D(TAG, "3A event type: %d", iso3a_event->type);

        if(iso3a_event->type == Iso14443_3aPollerEventTypeReady) {
            const Iso14443_3aData* iso3a_data = nfc_poller_get_data(instance->poller);
            if(iso3a_data) {
                // Validate UID length first
                uint8_t uid_len = iso3a_data->uid_len;
                if(uid_len > HID_DEVICE_NFC_UID_MAX_LEN) {
                    uid_len = HID_DEVICE_NFC_UID_MAX_LEN;
                }
                if(uid_len > 0) {
                    instance->last_data.uid_len = uid_len;
                    memcpy(instance->last_data.uid, iso3a_data->uid, uid_len);
                    instance->last_data.has_ndef = false;
                    instance->last_data.ndef_text[0] = '\0';

                    FURI_LOG_I(TAG, "Got ISO14443-3A UID, len: %d", instance->last_data.uid_len);
                    instance->state = HidDeviceNfcStateSuccess;

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

static NfcCommand hid_device_nfc_poller_callback_iso14443_4a(NfcGenericEvent event, void* context) {
    furi_assert(context);
    HidDeviceNfc* instance = context;

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

                    if(uid_len > HID_DEVICE_NFC_UID_MAX_LEN) {
                        uid_len = HID_DEVICE_NFC_UID_MAX_LEN;
                    }
                    if(uid_len > 0) {
                        instance->last_data.uid_len = uid_len;
                        memcpy(instance->last_data.uid, iso3a_data->uid, uid_len);
                        instance->last_data.has_ndef = false;
                        instance->last_data.ndef_text[0] = '\0';

                        FURI_LOG_I(TAG, "Got ISO14443-4A UID, len: %d", instance->last_data.uid_len);
                        instance->state = HidDeviceNfcStateSuccess;
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

static NfcCommand hid_device_nfc_poller_callback_mf_ultralight(NfcGenericEvent event, void* context) {
    furi_assert(context);
    HidDeviceNfc* instance = context;

    FURI_LOG_D(TAG, "MF Ultralight callback: protocol=%d", event.protocol);

    if(event.protocol == NfcProtocolMfUltralight) {
        const MfUltralightPollerEvent* mfu_event = event.event_data;
        FURI_LOG_D(TAG, "MFU event type: %d", mfu_event->type);

        if(mfu_event->type == MfUltralightPollerEventTypeReadSuccess) {
            // Successfully read the tag
            const MfUltralightData* mfu_data = nfc_poller_get_data(instance->poller);
            if(mfu_data) {
                // Get UID from ISO14443-3A base data
                const Iso14443_3aData* iso3a_data = mfu_data->iso14443_3a_data;
                if(iso3a_data) {
                    uint8_t uid_len = iso3a_data->uid_len;
                    if(uid_len > HID_DEVICE_NFC_UID_MAX_LEN) {
                        uid_len = HID_DEVICE_NFC_UID_MAX_LEN;
                    }
                    if(uid_len > 0) {
                        instance->last_data.uid_len = uid_len;
                        memcpy(instance->last_data.uid, iso3a_data->uid, uid_len);
                        instance->last_data.has_ndef = false;
                        instance->last_data.ndef_text[0] = '\0';

                        FURI_LOG_I(TAG, "Got MF Ultralight UID, len: %d", instance->last_data.uid_len);

                        // Parse NDEF if requested
                        if(instance->parse_ndef && mfu_data->pages_read > 4) {
                            // NDEF data typically starts at page 4 (byte offset 16)
                            // Pages 0-3 are reserved for UID and lock bytes
                            // We'll read up to 64 pages (256 bytes) for NDEF
                            size_t ndef_data_len = (mfu_data->pages_read - 4) * 4;
                            if(ndef_data_len > 240) ndef_data_len = 240; // Limit to reasonable size

                            const uint8_t* ndef_data = &mfu_data->page[4].data[0];

                            FURI_LOG_D(TAG, "Attempting NDEF parse, data_len=%zu, pages_read=%d",
                                      ndef_data_len, mfu_data->pages_read);

                            size_t text_len = hid_device_nfc_parse_ndef_text(
                                ndef_data,
                                ndef_data_len,
                                instance->last_data.ndef_text,
                                HID_DEVICE_NDEF_MAX_LEN);

                            if(text_len > 0) {
                                instance->last_data.has_ndef = true;
                                FURI_LOG_I(TAG, "Found NDEF text: %s", instance->last_data.ndef_text);
                            } else {
                                FURI_LOG_D(TAG, "No NDEF text records found");
                            }
                        }

                        instance->state = HidDeviceNfcStateSuccess;
                    }
                }
            }
            return NfcCommandStop;
        } else if(mfu_event->type == MfUltralightPollerEventTypeReadFailed) {
            FURI_LOG_E(TAG, "MFU poller read failed");
            return NfcCommandStop;
        } else if(mfu_event->type == MfUltralightPollerEventTypeRequestMode) {
            // Set read mode
            mfu_event->data->poller_mode = MfUltralightPollerModeRead;
            FURI_LOG_D(TAG, "MFU poller set to read mode");
            return NfcCommandContinue;
        }
    }
    return NfcCommandContinue;
}

static NfcCommand hid_device_nfc_poller_callback_iso15693(NfcGenericEvent event, void* context) {
    furi_assert(context);
    HidDeviceNfc* instance = context;

    FURI_LOG_D(TAG, "ISO15693 callback: protocol=%d", event.protocol);

    if(event.protocol == NfcProtocolIso15693_3) {
        const Iso15693_3PollerEvent* iso15_event = event.event_data;
        FURI_LOG_D(TAG, "ISO15693 event type: %d", iso15_event->type);

        if(iso15_event->type == Iso15693_3PollerEventTypeReady) {
            const Iso15693_3Data* iso15_data = nfc_poller_get_data(instance->poller);
            if(iso15_data) {
                // ISO15693 UID is 8 bytes
                uint8_t uid_len = 8;
                if(uid_len > HID_DEVICE_NFC_UID_MAX_LEN) {
                    uid_len = HID_DEVICE_NFC_UID_MAX_LEN;
                }

                instance->last_data.uid_len = uid_len;
                memcpy(instance->last_data.uid, iso15_data->uid, uid_len);
                instance->last_data.has_ndef = false;
                instance->last_data.ndef_text[0] = '\0';

                FURI_LOG_I(TAG, "Got ISO15693 UID, len: %d", instance->last_data.uid_len);

                // TODO: Add Type 5 NDEF parsing if parse_ndef is enabled
                // For now, we just read the UID

                instance->state = HidDeviceNfcStateSuccess;
            }
            return NfcCommandStop;
        } else if(iso15_event->type == Iso15693_3PollerEventTypeError) {
            FURI_LOG_E(TAG, "ISO15693 poller error");
            return NfcCommandStop;
        }
    }
    return NfcCommandContinue;
}

static void hid_device_nfc_scanner_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    HidDeviceNfc* instance = context;

    if(event.type == NfcScannerEventTypeDetected) {
        FURI_LOG_I(TAG, "NFC tag detected, protocols: %zu", event.data.protocol_num);

        // Select best protocol in priority order (NDEF capability is handled in callbacks)
        // Priority: MfUltralight > ISO14443-4A > ISO15693 > ISO14443-3A
        NfcProtocol protocol_to_use = NfcProtocolInvalid;

        // Log all detected protocols
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            FURI_LOG_I(TAG, "  Protocol %zu: %d", i, event.data.protocols[i]);
        }

        // Check for protocols in priority order
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            NfcProtocol p = event.data.protocols[i];

            // Highest priority: MfUltralight (supports Type 2 NDEF)
            if(p == NfcProtocolMfUltralight) {
                protocol_to_use = p;
                FURI_LOG_I(TAG, "Using MF Ultralight protocol");
                break;
            }
            // Next: ISO14443-4A (supports Type 4 NDEF)
            if(p == NfcProtocolIso14443_4a && protocol_to_use == NfcProtocolInvalid) {
                protocol_to_use = p;
            }
            // Next: ISO15693 (supports Type 5 NDEF, UID always available)
            if(p == NfcProtocolIso15693_3 && protocol_to_use == NfcProtocolInvalid) {
                protocol_to_use = p;
            }
            // Last: ISO14443-3A (UID only)
            if(p == NfcProtocolIso14443_3a && protocol_to_use == NfcProtocolInvalid) {
                protocol_to_use = p;
            }
        }

        // If no direct match, try parent protocols
        if(protocol_to_use == NfcProtocolInvalid && event.data.protocol_num > 0) {
            for(size_t i = 0; i < event.data.protocol_num; i++) {
                NfcProtocol p = event.data.protocols[i];
                NfcProtocol parent = nfc_protocol_get_parent(p);
                FURI_LOG_I(TAG, "  Protocol %d has parent: %d", p, parent);

                // Check parents in same priority order
                if(parent == NfcProtocolMfUltralight) {
                    protocol_to_use = parent;
                    FURI_LOG_I(TAG, "Using parent MF Ultralight");
                    break;
                }
                if(parent == NfcProtocolIso14443_4a && protocol_to_use == NfcProtocolInvalid) {
                    protocol_to_use = parent;
                }
                if(parent == NfcProtocolIso15693_3 && protocol_to_use == NfcProtocolInvalid) {
                    protocol_to_use = parent;
                }
                if(parent == NfcProtocolIso14443_3a && protocol_to_use == NfcProtocolInvalid) {
                    protocol_to_use = parent;
                }
            }
        }

        if(protocol_to_use != NfcProtocolInvalid) {
            instance->detected_protocol = protocol_to_use;
            instance->state = HidDeviceNfcStateTagDetected;
            FURI_LOG_I(TAG, "Selected protocol %d, waiting for poller start", protocol_to_use);
        } else {
            FURI_LOG_W(TAG, "No supported protocol found");
        }
    }
}

// Internal function to switch from scanner to poller
static void hid_device_nfc_start_poller(HidDeviceNfc* instance) {
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
        instance->state = HidDeviceNfcStatePolling;
        if(instance->detected_protocol == NfcProtocolMfUltralight) {
            nfc_poller_start(instance->poller, hid_device_nfc_poller_callback_mf_ultralight, instance);
        } else if(instance->detected_protocol == NfcProtocolIso14443_3a) {
            nfc_poller_start(instance->poller, hid_device_nfc_poller_callback_iso14443_3a, instance);
        } else if(instance->detected_protocol == NfcProtocolIso14443_4a) {
            nfc_poller_start(instance->poller, hid_device_nfc_poller_callback_iso14443_4a, instance);
        } else if(instance->detected_protocol == NfcProtocolIso15693_3) {
            nfc_poller_start(instance->poller, hid_device_nfc_poller_callback_iso15693, instance);
        }
        FURI_LOG_I(TAG, "Started poller for protocol %d", instance->detected_protocol);
    } else {
        FURI_LOG_E(TAG, "Failed to allocate poller");
        instance->state = HidDeviceNfcStateIdle;
    }
}

HidDeviceNfc* hid_device_nfc_alloc(void) {
    HidDeviceNfc* instance = malloc(sizeof(HidDeviceNfc));

    instance->nfc = nfc_alloc();
    instance->scanner = NULL;
    instance->poller = NULL;
    instance->state = HidDeviceNfcStateIdle;
    instance->parse_ndef = false;
    instance->detected_protocol = NfcProtocolInvalid;
    instance->callback = NULL;
    instance->callback_context = NULL;
    instance->owner_thread = furi_thread_get_current_id();

    memset(&instance->last_data, 0, sizeof(HidDeviceNfcData));

    FURI_LOG_I(TAG, "NFC reader allocated");

    return instance;
}

void hid_device_nfc_free(HidDeviceNfc* instance) {
    furi_assert(instance);

    hid_device_nfc_stop(instance);

    if(instance->nfc) {
        nfc_free(instance->nfc);
        instance->nfc = NULL;
    }

    free(instance);
    FURI_LOG_I(TAG, "NFC reader freed");
}

void hid_device_nfc_set_callback(
    HidDeviceNfc* instance,
    HidDeviceNfcCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

void hid_device_nfc_start(HidDeviceNfc* instance, bool parse_ndef) {
    furi_assert(instance);

    FURI_LOG_I(TAG, "NFC start called, current state=%d, scanner=%p, poller=%p",
               instance->state, (void*)instance->scanner, (void*)instance->poller);

    if(instance->state != HidDeviceNfcStateIdle) {
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
    memset(&instance->last_data, 0, sizeof(HidDeviceNfcData));

    // Create and start scanner
    instance->scanner = nfc_scanner_alloc(instance->nfc);
    if(!instance->scanner) {
        FURI_LOG_E(TAG, "Failed to allocate NFC scanner!");
        return;
    }

    nfc_scanner_start(instance->scanner, hid_device_nfc_scanner_callback, instance);

    instance->state = HidDeviceNfcStateScanning;
    FURI_LOG_I(TAG, "NFC scanning started (NDEF: %s), scanner=%p", parse_ndef ? "ON" : "OFF", (void*)instance->scanner);
}

void hid_device_nfc_stop(HidDeviceNfc* instance) {
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
    instance->state = HidDeviceNfcStateIdle;
    instance->detected_protocol = NfcProtocolInvalid;

    FURI_LOG_I(TAG, "NFC scanning stopped, state now Idle");
}

bool hid_device_nfc_is_scanning(HidDeviceNfc* instance) {
    furi_assert(instance);
    return instance->state == HidDeviceNfcStateScanning ||
           instance->state == HidDeviceNfcStateTagDetected ||
           instance->state == HidDeviceNfcStatePolling;
}

// Call this from the main thread's tick handler to process NFC events
// Returns true if a tag was successfully read (data available in last_data)
bool hid_device_nfc_tick(HidDeviceNfc* instance) {
    furi_assert(instance);

    if(instance->state == HidDeviceNfcStateTagDetected) {
        // Scanner detected a tag, switch to poller (safe to do from main thread)
        FURI_LOG_I(TAG, "Tick: starting poller for detected tag, protocol=%d", instance->detected_protocol);
        hid_device_nfc_start_poller(instance);
        return false;
    }

    if(instance->state == HidDeviceNfcStateSuccess) {
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
        instance->state = HidDeviceNfcStateIdle;
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
