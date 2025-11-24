# CLAUDE.md - Contactless HID Reader Project Guide

This file provides guidance for developing the Contactless HID Reader Flipper Zero application. Follow these guidelines to ensure a well-tested, polished final product.

---

## Project Overview

**Goal**: Transform the Flipper Zero into a contactless tag-to-keyboard device that reads RFID/NFC tags and types their UIDs (and optional NDEF data) as HID keyboard input.

**Base Template**: leedave/flipper-zero-fap-boilerplate
**Target Firmware**: Official Flipper Zero firmware (latest stable)
**Build System**: Docker-based via `./fbt`

### Core Features
1. Read RFID (125 kHz) UIDs - EM4100, HID Prox, Indala
2. Read NFC (13.56 MHz) UIDs - ISO14443A/B, MIFARE, NTAG
3. Parse NDEF text records from NFC tags
4. Output via USB HID keyboard
5. Output via Bluetooth HID keyboard
6. 4 scanning modes + Bluetooth pairing
7. Configurable delimiter, Enter key append, NDEF toggle

### Specification Document
Full requirements are in `contactless_hid_reader.md` - reference it for all behavioral details.

---

## Build Environment Setup

### Initial Setup (Run Once)
```bash
# Clone official Flipper Zero firmware (outside this app directory)
cd /home/work
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware

# Create symlink for our app
ln -s "/home/work/contactless hid reader" applications_user/contactless_hid_reader

# First build will download Docker image and toolchain
./fbt fap_contactless_hid_reader
```

### Build Commands
```bash
# Build the app
./fbt fap_contactless_hid_reader

# Build and deploy to connected Flipper via USB
./fbt launch APPSRC=applications_user/contactless_hid_reader

# Clean build
./fbt -c fap_contactless_hid_reader
```

### Build Verification Checkpoints
- [ ] Boilerplate builds successfully before any changes
- [ ] Build succeeds after renaming
- [ ] Build succeeds after each new module added
- [ ] No warnings in final build

---

## Flipper Zero UX Guidelines

### Display Constraints
- **Screen**: 128x64 pixels, monochrome
- **Font sizes**: Use Flipper's built-in fonts (Primary, Secondary, Keyboard)
- **Keep text concise**: Max ~21 characters per line with Primary font
- **Status bar**: 8 pixels at top (if used)

### Input Model
- **5 buttons**: Up, Down, Left, Right, OK (center), Back (physical)
- **Short press vs Long press**: Support both where appropriate
- **Back button**: ALWAYS returns to previous screen (never override)
- **OK button**: Confirms selection or primary action

### Navigation Patterns
- **Main menu**: Use Submenu widget for mode selection
- **Settings**: Use VariableItemList for toggles and selections
- **Text input**: Use built-in TextInput widget
- **Confirmation**: Show status briefly (200ms), then auto-return

### Feedback Best Practices
- **Haptic**: Short vibration on successful scan, longer on error
- **LED**: Brief flash on scan (green=success, red=error)
- **Sound**: Optional beep (respect user setting)
- **Visual**: Always show what happened ("Sent", "Tag Not Present", etc.)

### Status Messaging
- Keep messages under 20 characters
- Use clear, actionable language:
  - "Scanning..." (during scan)
  - "FE:90:1A:B3" (show UID)
  - "Sent" (after HID output)
  - "Tag Not Present" (timeout in combo mode)
  - "Connect USB or BT HID" (no connection)

---

## Code Architecture

### File Naming Convention
All files use `hid_reader_` prefix after renaming:
```
hid_reader.c              # Main app entry, allocation, lifecycle
hid_reader.h              # Main app struct and types
helpers/
  hid_reader_hid.c/h      # USB + Bluetooth HID output
  hid_reader_nfc.c/h      # NFC reading and NDEF parsing
  hid_reader_rfid.c/h     # RFID reading
  hid_reader_scan.c/h     # Scan state machine and modes
  hid_reader_format.c/h   # UID formatting with delimiter
  hid_reader_storage.c/h  # Settings persistence
scenes/
  hid_reader_scene.c/h    # Scene manager
  hid_reader_scene_home.c # Main scanning screen
  hid_reader_scene_advanced.c # Settings screen
  hid_reader_scene_ndef.c # NDEF inspector
views/
  hid_reader_home_view.c/h # Custom home screen view
```

### Main App Struct (hid_reader.h)
```c
typedef struct {
    // Core Flipper components
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notification;

    // Views and widgets
    Submenu* submenu;
    VariableItemList* variable_item_list;
    HidReaderHomeView* home_view;
    TextInput* text_input;

    // Reader workers
    NfcWorker* nfc_worker;
    LfRfidWorker* rfid_worker;

    // HID connections
    FuriHalUsbHidConfig* usb_hid;
    bool bt_hid_connected;
    bool usb_hid_connected;

    // Scan state
    HidReaderMode mode;           // Current mode
    HidReaderScanState scan_state; // State machine
    uint8_t nfc_uid[10];
    uint8_t nfc_uid_len;
    uint8_t rfid_uid[8];
    uint8_t rfid_uid_len;
    char ndef_payload[256];
    FuriTimer* timeout_timer;
    FuriTimer* display_timer;

    // Settings
    char delimiter[8];
    bool append_enter;
    bool ndef_enabled;

    // UI state
    char status_text[32];
    char uid_display[64];
} HidReader;

typedef enum {
    HidReaderModeNfc,
    HidReaderModeRfid,
    HidReaderModeNfcThenRfid,
    HidReaderModeRfidThenNfc,
} HidReaderMode;

typedef enum {
    HidReaderScanStateIdle,
    HidReaderScanStateScanning,
    HidReaderScanStateWaitingSecond,
    HidReaderScanStateDisplaying,
    HidReaderScanStateCooldown,
} HidReaderScanState;
```

### application.fam Updates Required
```python
App(
    appid="contactless_hid_reader",
    name="Contactless HID Reader",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="hid_reader_app",
    requires=[
        "gui",
        "storage",
        "nfc",          # Add for NFC reading
        "lfrfid",       # Add for RFID reading
        "bt",           # Add for Bluetooth HID
    ],
    stack_size=4 * 1024,  # Increase for NFC/RFID workers
    fap_icon="icons/hid_reader_10px.png",
    fap_category="NFC",
    fap_version="1.0",
)
```

---

## Implementation Order

Follow this exact order to ensure incremental, testable progress:

### Phase 1: Build Environment (No Flipper needed)
1. Clone firmware repo
2. Symlink app
3. Verify boilerplate builds

### Phase 2: Rename (No Flipper needed)
1. Rename all files
2. Update application.fam
3. Search/replace all "boilerplate" references
4. Remove demo scenes (scene_1 through scene_6)
5. Verify build

### Phase 3: HID Module (Flipper needed for testing)
1. Implement USB HID connection detection
2. Implement USB HID character typing
3. Implement BT HID connection detection
4. Implement BT HID character typing
5. Create simple test: type "HELLO" on button press
6. **TEST CHECKPOINT**: Verify typing works on USB and BT

### Phase 4: NFC Module (Flipper + NFC tags needed)
1. Initialize NFC worker
2. Implement tag detection callback
3. Extract UID from detected tag
4. Implement NDEF text record parsing
5. **TEST CHECKPOINT**: Show detected UID on screen

### Phase 5: RFID Module (Flipper + RFID tags needed)
1. Initialize LFRFID worker
2. Implement tag detection callback
3. Extract UID from detected tag
4. **TEST CHECKPOINT**: Show detected RFID UID on screen

### Phase 6: UID Formatting
1. Implement hex byte to string conversion
2. Apply delimiter between bytes
3. Concatenate NDEF payload
4. Add Enter key if enabled
5. **TEST CHECKPOINT**: Formatted output displays correctly

### Phase 7: Scan Modes
1. Implement single-tag modes (NFC only, RFID only)
2. Implement combo mode state machine
3. Implement timeout handling
4. **TEST CHECKPOINT**: All 4 modes work correctly

### Phase 8: UI Polish
1. Implement home screen with mode selector
2. Implement HID status line
3. Implement Advanced Settings screen
4. Implement NDEF Inspector
5. Add haptic/LED/sound feedback
6. **TEST CHECKPOINT**: Full UX flow works smoothly

### Phase 9: Final Testing
1. Test all modes with various tags
2. Test edge cases (tag left on, rapid scans, no HID)
3. Test settings persistence
4. Test BT pairing flow
5. **FINAL CHECKPOINT**: App is complete and polished

---

## Testing Protocol

### Required Test Equipment
- Flipper Zero device
- USB cable for deployment
- Computer with USB for USB HID testing
- Phone/computer for Bluetooth HID testing
- NFC tags: NTAG215, MIFARE Classic, any ISO14443A
- RFID tags: EM4100, HID Prox (if available)
- NFC tag with NDEF text record (can write one using phone)

### Test Checklist

#### HID Output Tests
- [ ] USB HID types characters correctly
- [ ] USB HID types Enter key when enabled
- [ ] BT HID pairs successfully
- [ ] BT HID types characters correctly
- [ ] Both USB and BT receive output when both connected
- [ ] "Connect USB or BT HID" shown when neither connected
- [ ] Scanning disabled when no HID connection

#### NFC Tests
- [ ] ISO14443A tag UID read correctly
- [ ] 4-byte UID formatted correctly
- [ ] 7-byte UID formatted correctly
- [ ] NDEF text record parsed and appended
- [ ] Non-NDEF tag works (UID only)
- [ ] Tag removal detected

#### RFID Tests
- [ ] EM4100 tag UID read correctly
- [ ] HID Prox tag UID read (if available)
- [ ] Tag removal detected

#### Mode Tests
- [ ] NFC mode: scan → output → cooldown
- [ ] RFID mode: scan → output → cooldown
- [ ] NFC→RFID: both scanned → combined output
- [ ] NFC→RFID: timeout if second tag missing
- [ ] RFID→NFC: both scanned → combined output
- [ ] RFID→NFC: timeout if second tag missing

#### UI Tests
- [ ] Mode selector navigates correctly
- [ ] Back button returns to previous screen
- [ ] Settings toggles work
- [ ] Delimiter can be changed
- [ ] NDEF Inspector lists records
- [ ] Status messages display correctly
- [ ] Haptic feedback fires on scan

#### Edge Case Tests
- [ ] Tag left on reader: ignored until removed
- [ ] Rapid consecutive scans: cooldown prevents spam
- [ ] Tag with no NDEF: works (UID only)
- [ ] Very long NDEF payload: truncated gracefully
- [ ] Settings persist across app restart

---

## Common Pitfalls to Avoid

### Memory Management
- Always free allocated resources in `_free()` functions
- Use Flipper's memory allocation (`malloc`/`free` are wrapped)
- Keep stack usage low (use heap for large buffers)
- NFC/RFID workers must be stopped before freeing

### Thread Safety
- NFC/RFID callbacks run in separate threads
- Use `furi_message_queue` to communicate with main thread
- Don't access GUI from worker callbacks
- Use `with_view_model()` for thread-safe view model access

### HID Gotchas
- USB HID requires specific initialization sequence
- BT HID requires pairing before use
- Some characters need Shift modifier (handle in typing function)
- Send key release after key press
- Small delay between keystrokes (1-5ms)

### NFC/RFID API
- NFC and RFID cannot poll simultaneously (hardware limitation)
- Must stop one worker before starting the other
- Tag presence is detected by successful read, absence by failed read

### Scene Manager
- Always call `scene_manager_next_scene()` to push new scene
- Use `scene_manager_previous_scene()` for back navigation
- Clean up scene state in `on_exit()` handler
- Don't block in `on_enter()` - use timers/callbacks instead

---

## API Quick Reference

### USB HID
```c
#include <furi_hal_usb_hid.h>

// Initialize
furi_hal_usb_set_config(&usb_hid_config, NULL);

// Check connection
bool connected = furi_hal_usb_is_connected();

// Type character
furi_hal_hid_kb_press(HID_KEYBOARD_A);
furi_hal_hid_kb_release(HID_KEYBOARD_A);
```

### Bluetooth HID
```c
#include <bt/bt_service/bt.h>
#include <furi_hal_bt_hid.h>

// Start BT HID
bt_set_status_changed_callback(bt, callback, context);
furi_hal_bt_start_advertising();

// Type character
furi_hal_bt_hid_kb_press(HID_KEYBOARD_A);
furi_hal_bt_hid_kb_release(HID_KEYBOARD_A);
```

### NFC
```c
#include <lib/nfc/nfc.h>
#include <lib/nfc/nfc_worker.h>

// Start NFC worker
nfc_worker_start(worker, NfcWorkerStateDetect, callback, context);

// In callback, extract UID
uint8_t* uid = nfc_data->uid;
uint8_t uid_len = nfc_data->uid_len;
```

### LFRFID
```c
#include <lib/lfrfid/lfrfid_worker.h>

// Start RFID worker
lfrfid_worker_start_thread(worker);
lfrfid_worker_read_start(worker, LFRFIDWorkerReadTypeAuto, callback, context);

// In callback, get protocol data
// UID extraction depends on protocol
```

### Timers
```c
// Create timer
FuriTimer* timer = furi_timer_alloc(callback, FuriTimerTypeOnce, context);

// Start timer (200ms)
furi_timer_start(timer, furi_ms_to_ticks(200));

// Stop timer
furi_timer_stop(timer);

// Free timer
furi_timer_free(timer);
```

---

## Quality Gates

Before marking any phase complete:

1. **Build Gate**: Code compiles without warnings
2. **Function Gate**: Feature works as specified
3. **UX Gate**: Interaction feels responsive and intuitive
4. **Edge Case Gate**: Handles error conditions gracefully
5. **Integration Gate**: Works with previously completed features

Before final delivery:

1. All test checklist items pass
2. No crashes or freezes during normal use
3. Memory usage is stable (no leaks)
4. App feels polished and professional
5. User has confirmed app meets their needs

---

## Communication Protocol

### When to Ask User to Connect Flipper
- After Phase 1: "Ready to test builds. Please connect Flipper via USB."
- Phase 3: "HID module ready for testing. Connect Flipper."
- Phase 4+: Keep Flipper connected for iterative testing.

### What to Report
- Build success/failure with any errors
- Test results for each checkpoint
- Any deviations from spec or design decisions made
- Blockers or questions that need user input

### How to Handle Issues
- If build fails: Show error, attempt fix, retry
- If feature doesn't work: Debug, show findings, propose solution
- If spec is unclear: Ask user for clarification before proceeding
- If hardware limitation found: Document and propose alternative

---

## Final Deliverables

1. **Working FAP**: Builds and runs on official firmware
2. **All features**: Per specification document
3. **Clean code**: Well-organized, properly named
4. **Tested**: All checklist items verified
5. **Documentation**: Updated README.md if needed

---

*This document should be referenced throughout development. Update it if new patterns or issues are discovered.*
