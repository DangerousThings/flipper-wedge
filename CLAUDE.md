# CLAUDE.md - Contactless HID Device Maintenance Guide

This file provides guidance for **maintaining and stabilizing** the Contactless HID Device Flipper Zero application across multiple firmware versions. The focus is on **preserving functionality, ensuring cross-firmware compatibility, and preventing regressions** as new firmware versions are released.

---

## Project Status

**Current Version**: 1.0 (Feature-Complete)
**Status**: Active Maintenance
**Target**: Stability and multi-firmware compatibility

### Core Features (Complete)
✅ RFID (125 kHz) reading - EM4100, HID Prox, Indala
✅ NFC (13.56 MHz) reading - ISO14443A/B, MIFARE, NTAG
✅ NDEF parsing - Type 2, Type 4, Type 5 text records
✅ USB HID keyboard output
✅ Bluetooth HID keyboard output
✅ 5 scanning modes (NFC, RFID, NDEF, NFC+RFID, RFID+NFC)
✅ Dynamic USB/BLE switching (no app restart required)
✅ Configurable settings (delimiter, Enter key, output mode, vibration)
✅ Settings persistence
✅ Scan logging to SD card
✅ Haptic/LED/sound feedback
✅ Mode startup behavior (remember last mode or default)

### Supported Firmwares
- **Official** (Primary) - flipperzero-firmware
- **Unleashed** - unleashed-firmware
- **Momentum** (includes Xtreme) - Momentum-Firmware
- **RogueMaster** (Secondary) - roguemaster-firmware

---

## Maintenance Philosophy

### Primary Goals
1. **Stability First**: No new features without explicit user request
2. **Cross-Firmware Compatibility**: Test on all 4 firmwares before release
3. **Regression Prevention**: Validate all existing features after any change
4. **API Compatibility**: Monitor firmware API changes and adapt proactively
5. **User Experience**: Maintain consistent behavior across firmware versions

### What NOT to Do
- ❌ Add new features unless explicitly requested
- ❌ Refactor working code without a clear maintenance benefit
- ❌ Optimize code that isn't causing problems
- ❌ Change UX patterns without user feedback
- ❌ Update firmware dependencies without thorough testing

### When to Make Changes
- ✅ Firmware API breaks compatibility (required adaptation)
- ✅ Critical bugs affecting core functionality
- ✅ Security vulnerabilities
- ✅ User-reported issues with evidence
- ✅ Explicitly requested enhancements from maintainers

---

## Build & Deploy Workflows

### Build Scripts Overview

The project includes three build scripts for multi-firmware support:

#### 1. `build.sh` - Build for specific firmware
```bash
# Usage
./build.sh [firmware] [--branch BRANCH] [--tag TAG]

# Examples
./build.sh official              # Official firmware (latest stable tag)
./build.sh unleashed             # Unleashed firmware (release branch)
./build.sh momentum              # Momentum firmware (release branch)
./build.sh official --tag 1.3.4  # Specific version
./build.sh official --branch dev # Development branch

# Aliases
./build.sh ofw   # Official
./build.sh ul    # Unleashed
./build.sh mntm  # Momentum
./build.sh rm    # RogueMaster
```

**Output**: `dist/<firmware>/<version>/contactless_hid_device.fap`

**Features**:
- Auto-detects latest stable tag for Official firmware
- Uses release branch for custom firmwares by default
- Warns when firmware version changes
- Updates submodules automatically
- Creates organized dist/ directory structure

#### 2. `deploy.sh` - Build and deploy to connected Flipper
```bash
# Usage
./deploy.sh [firmware] [--branch BRANCH] [--tag TAG]

# Examples
./deploy.sh official   # Build and deploy Official firmware
./deploy.sh momentum   # Build and deploy Momentum firmware
```

**Features**:
- Verifies Flipper is connected (/dev/ttyACM0)
- Warns about firmware version mismatches
- Uploads and launches app automatically
- Uses fbt's built-in launch command

#### 3. `build-all-firmwares.sh` - Build for all firmwares
```bash
# Usage
./build-all-firmwares.sh [--branch BRANCH] [--tag TAG]

# Example
./build-all-firmwares.sh  # Build for all available firmwares
```

**Output**: `dist/<firmware>/<version>/contactless_hid_device.fap` for each firmware

### Firmware Update Detection

The build scripts **automatically detect firmware version changes** and display warnings:

```
⚠️ FIRMWARE VERSION CHANGED: release → tag 1.3.5 (a1b2c3d)
```

When you see this warning:
1. **Read the firmware changelog** for API changes
2. **Run regression tests** after building
3. **Check for deprecation warnings** during build
4. **Test on actual hardware** before distribution

---

## Testing & Validation Protocols

### Pre-Release Testing Checklist

Before releasing any update, **ALL** tests must pass on **ALL** supported firmwares.

#### Build Validation
- [ ] Official firmware: builds without warnings
- [ ] Unleashed firmware: builds without warnings
- [ ] Momentum firmware: builds without warnings
- [ ] RogueMaster firmware: builds without warnings (or document known issues)

#### Core Functionality Tests

**NFC Reading** (test on each firmware)
- [ ] ISO14443A tag UID read correctly
- [ ] 4-byte UID formatted correctly
- [ ] 7-byte UID formatted correctly
- [ ] Tag removal detected
- [ ] Multiple consecutive scans work

**RFID Reading** (test on each firmware)
- [ ] EM4100 tag UID read correctly
- [ ] HID Prox tag works (if available)
- [ ] Tag removal detected
- [ ] Multiple consecutive scans work

**NDEF Parsing** (test on each firmware)
- [ ] Type 2 NDEF text record parsed (NTAG)
- [ ] Type 4 NDEF text record parsed (if available)
- [ ] Type 5 NDEF text record parsed (if available)
- [ ] Non-NDEF tag shows error in NDEF mode
- [ ] Non-NDEF tag outputs UID in NFC/combo modes

**HID Output** (test on each firmware)
- [ ] USB HID connection detected
- [ ] USB HID types characters correctly
- [ ] USB HID Enter key works when enabled
- [ ] BT HID pairing works
- [ ] BT HID types characters correctly
- [ ] Dual output (USB + BT) works simultaneously
- [ ] "Connect USB or BT HID" shown when disconnected

**Scan Modes** (test on each firmware)
- [ ] NFC Only: scan → output UID → cooldown
- [ ] RFID Only: scan → output UID → cooldown
- [ ] NDEF Mode: scan NDEF tag → output text → cooldown
- [ ] NDEF Mode: scan non-NDEF → red LED → no output
- [ ] NFC+RFID: both scanned → combined output
- [ ] NFC+RFID: timeout after 5s if second tag missing
- [ ] RFID+NFC: both scanned → combined output
- [ ] RFID+NFC: timeout after 5s if second tag missing

**Settings & Persistence** (test on each firmware)
- [ ] Delimiter changes apply to output
- [ ] Append Enter toggle works
- [ ] Output mode (USB/BLE) switches dynamically without restart
- [ ] Vibration level changes work
- [ ] Scan logging to SD works when enabled
- [ ] Mode startup behavior persists across restarts
- [ ] Settings persist across app restarts

**UI/UX** (test on each firmware)
- [ ] Mode selector navigates correctly
- [ ] Back button returns to previous screen
- [ ] Status messages display clearly
- [ ] Haptic feedback fires on scan
- [ ] LED feedback works (green=success, red=error)
- [ ] BT pairing instructions display correctly

#### Edge Case Tests
- [ ] Tag left on reader: ignored until removed
- [ ] Rapid consecutive scans: cooldown prevents spam
- [ ] Very long NDEF payload: truncated gracefully
- [ ] App survives USB disconnect/reconnect
- [ ] App survives BT disconnect/reconnect
- [ ] No crashes during normal operation
- [ ] No memory leaks (run app for extended period)

### Regression Test Protocol

After **any** code change (bug fix, firmware adaptation, etc.):

1. **Build on all firmwares** - Ensure no new warnings
2. **Run core functionality tests** (minimum)
3. **Test the specific area changed** (thorough)
4. **Check for unintended side effects** in related code
5. **Verify settings persistence** still works
6. **Document any behavioral changes**

---

## Firmware Update Procedures

### When a New Firmware Version is Released

Follow this procedure **for each supported firmware** when a new version is released:

#### Step 1: Update Local Firmware Clone
```bash
cd /home/work/flipperzero-firmware  # or unleashed/momentum/roguemaster
git fetch --all --tags
git checkout <new-version>  # or release branch
git submodule update --init --recursive
```

#### Step 2: Review Firmware Changelog
- Read the firmware's CHANGELOG.md or release notes
- Look for **API changes** affecting:
  - NFC/RFID workers
  - HID keyboard (USB/BT)
  - GUI components (ViewDispatcher, SceneManager, etc.)
  - Storage/settings APIs
  - Notification/feedback APIs

#### Step 3: Build and Check for Issues
```bash
cd "/home/work/contactless hid device"
./build.sh <firmware>
```

Watch for:
- **Compilation errors** → API breaking change
- **New warnings** → Deprecation or API change
- **Missing symbols** → Removed API functions
- **Link errors** → Library changes

#### Step 4: Address API Changes

If APIs have changed, follow this pattern:

##### Pattern 1: Deprecated API
```c
// Old API (deprecated)
nfc_worker_start(worker, callback);

// New API
nfc_worker_start_ex(worker, callback, context);

// Solution: Update calls, test thoroughly
```

##### Pattern 2: Removed API
```c
// Old API (removed)
furi_hal_usb_hid_kb_press(key);

// New API (different module)
usb_hid_keyboard_press(hid, key);

// Solution: Find replacement in firmware docs, update code
```

##### Pattern 3: Changed Behavior
```c
// Example: NFC poller now requires explicit restart after error
// Old: Auto-restarts on error
// New: Must call nfc_poller_restart() manually

// Solution: Update error handling logic
```

**Important**: Always check the firmware's migration guide or API documentation.

#### Step 5: Run Full Regression Tests
- [ ] Build succeeds on updated firmware
- [ ] Run complete testing checklist (see above)
- [ ] Test on actual hardware with the new firmware flashed
- [ ] Verify no behavioral changes in existing features

#### Step 6: Update Other Firmwares
- Repeat Steps 1-5 for **all** supported firmwares
- Document any firmware-specific quirks or workarounds

#### Step 7: Update Build Scripts (if needed)
- If default branches/tags change, update build.sh defaults
- Update BUILD_MULTI_FIRMWARE.md with new version info

#### Step 8: Document Changes
Update [docs/changelog.md](docs/changelog.md) with:
- Firmware versions tested
- Any code changes made for compatibility
- Known issues or workarounds

---

## Code Architecture Reference

### File Structure
```
contactless_hid_device/
├── application.fam              # App manifest (dependencies, version)
├── hid_device.c/h               # Main app entry, lifecycle, state
├── helpers/
│   ├── hid_device_hid.c/h       # HID interface (USB/BT abstraction)
│   ├── hid_device_hid_worker.c/h # HID worker thread (non-blocking)
│   ├── hid_device_nfc.c/h       # NFC worker, NDEF parsing
│   ├── hid_device_rfid.c/h      # RFID worker
│   ├── hid_device_format.c/h    # UID formatting, delimiter logic
│   ├── hid_device_storage.c/h   # Settings persistence (storage API)
│   ├── hid_device_log.c/h       # SD card scan logging
│   ├── hid_device_haptic.c/h    # Vibration feedback
│   ├── hid_device_led.c/h       # LED feedback
│   ├── hid_device_speaker.c/h   # Audio feedback
│   └── hid_device_debug.c/h     # Debug logging utilities
├── scenes/
│   ├── hid_device_scene.c/h     # Scene manager definitions
│   ├── hid_device_scene_menu.c  # Main menu scene
│   ├── hid_device_scene_startscreen.c # Scanning screen scene
│   ├── hid_device_scene_settings.c # Settings screen scene
│   └── hid_device_scene_bt_pair.c # BT pairing scene
├── views/
│   └── hid_device_startscreen.c/h # Custom scanning view
├── icons/                        # PNG assets
├── docs/                         # Documentation
├── build.sh                      # Single-firmware build script
├── deploy.sh                     # Build and deploy script
└── build-all-firmwares.sh        # Multi-firmware build script
```

### Key Architecture Patterns

#### 1. HID Worker Thread Pattern
The HID module runs in a **separate worker thread** to avoid blocking the UI thread during Bluetooth operations (which can take 100ms+).

**Location**: [helpers/hid_device_hid_worker.c](helpers/hid_device_hid_worker.c)

**Key Functions**:
- `hid_device_hid_worker_alloc()` - Creates worker thread
- `hid_device_hid_worker_start()` - Starts HID in background
- `hid_device_hid_worker_type_string()` - Queues typing (non-blocking)
- `hid_device_hid_worker_get_hid()` - Gets HID instance

**Thread Safety**: Uses FuriMessageQueue for UI ↔ worker communication.

#### 2. Dynamic Output Mode Switching
USB ↔ BLE switching happens **without app restart**, similar to Bad USB.

**Location**: [hid_device.c:195](hid_device.c#L195) - `hid_device_switch_output_mode()`

**Pattern**:
1. Stop NFC/RFID workers
2. Stop HID worker
3. Deinit old HID profile
4. Switch mode in settings
5. Init new HID profile
6. Restart HID worker
7. Restart NFC/RFID workers

**Important**: Always stop workers before HID operations to avoid crashes.

#### 3. NFC Error Recovery
NFC poller can fail and requires restart. The app **automatically recovers**.

**Location**: [helpers/hid_device_nfc.c](helpers/hid_device_nfc.c) - NFC worker callback

**Pattern**:
```c
case NfcPollerEventTypeStopped:
    // Poller stopped (error or intentional)
    if (nfc->running) {
        // Auto-restart if we're supposed to be running
        nfc_poller_start(nfc->poller, callback, context);
    }
    break;
```

**Critical**: Do NOT remove this auto-restart logic or NFC will stop working after first error.

#### 4. Scan State Machine
Manages scanning lifecycle across single and combo modes.

**Location**: [hid_device.h:66](hid_device.h#L66) - `HidDeviceScanState` enum

**States**:
- `Idle` - Not scanning
- `Scanning` - Waiting for first tag
- `WaitingSecond` - Combo mode, waiting for second tag
- `Displaying` - Showing result briefly
- `Cooldown` - Preventing re-scan of same tag

**Timers**:
- `timeout_timer` - 5s timeout for second tag in combo mode
- `display_timer` - Brief display before returning to scanning

**Location**: Scene startscreen manages state transitions.

#### 5. Settings Persistence

**CRITICAL**: All user-configurable settings **MUST** persist across app restarts. This is a core requirement.

Settings are saved to `/ext/apps_data/hid_device/hid_device.conf` using Flipper's FlipperFormat API.

**Location**: [helpers/hid_device_storage.c](helpers/hid_device_storage.c)

**Current Settings** (File Version 5):
- `delimiter` (string) - Delimiter between fields
- `append_enter` (bool) - Append Enter key after output
- `mode` (uint32) - Last used scan mode
- `mode_startup_behavior` (uint32) - Mode startup behavior enum
- `output_mode` (uint32) - USB/BLE output mode
- `vibration_level` (uint32) - Vibration intensity enum
- `ndef_max_len` (uint32) - NDEF text length limit enum (250/500/1000)
- `log_to_sd` (bool) - Scan logging to SD card

**Adding a New Setting - Required Steps**:

When adding ANY new user-facing setting, you **MUST** follow this pattern:

1. **Define the setting in app struct** ([hid_device.h](hid_device.h)):
   ```c
   typedef struct {
       // ... existing fields ...
       YourSettingType your_new_setting;
   } HidDevice;
   ```

2. **Add storage key constant** ([helpers/hid_device_storage.h](helpers/hid_device_storage.h)):
   ```c
   #define HID_DEVICE_SETTINGS_KEY_YOUR_SETTING "YourSetting"
   ```

3. **Initialize default value** ([hid_device.c](hid_device.c) in `hid_device_alloc()`):
   ```c
   app->your_new_setting = default_value;
   ```

4. **Save the setting** ([helpers/hid_device_storage.c](helpers/hid_device_storage.c) in `hid_device_save_settings()`):
   ```c
   // For uint32/enum:
   uint32_t your_setting = app->your_new_setting;
   flipper_format_write_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_YOUR_SETTING, &your_setting, 1);

   // For bool:
   flipper_format_write_bool(fff_file, HID_DEVICE_SETTINGS_KEY_YOUR_SETTING, &app->your_new_setting, 1);

   // For string:
   flipper_format_write_string_cstr(fff_file, HID_DEVICE_SETTINGS_KEY_YOUR_SETTING, app->your_new_setting);
   ```

5. **Load the setting** ([helpers/hid_device_storage.c](helpers/hid_device_storage.c) in `hid_device_read_settings()`):
   ```c
   // For uint32/enum:
   uint32_t your_setting = default_value;
   if(flipper_format_read_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_YOUR_SETTING, &your_setting, 1)) {
       // Validate range if it's an enum
       if(your_setting < YourSettingEnumCount) {
           app->your_new_setting = (YourSettingEnum)your_setting;
       }
   }

   // For bool:
   flipper_format_read_bool(fff_file, HID_DEVICE_SETTINGS_KEY_YOUR_SETTING, &app->your_new_setting, 1);
   ```

6. **Save immediately in callback** ([scenes/hid_device_scene_settings.c](scenes/hid_device_scene_settings.c)):
   ```c
   static void your_setting_callback(VariableItem* item) {
       HidDevice* app = variable_item_get_context(item);
       uint8_t index = variable_item_get_current_value_index(item);

       variable_item_set_current_value_text(item, your_text_array[index]);
       app->your_new_setting = (YourSettingEnum)index;
       hid_device_save_settings(app);  // CRITICAL: Save immediately
   }
   ```

   **CRITICAL**: Always call `hid_device_save_settings(app)` immediately in the callback.
   - **DO NOT** rely on Back button save alone (line 390) - it's a fallback only
   - **Reason**: If app exits abnormally (crash, home button, etc.), settings would be lost
   - **Pattern**: All settings callbacks MUST save immediately after updating the value

**Example: NDEF Max Length Setting**

See [helpers/hid_device_storage.c:65-66](helpers/hid_device_storage.c#L65-L66) (save) and [helpers/hid_device_storage.c:200-207](helpers/hid_device_storage.c#L200-L207) (load) for a complete example.

**File Versioning**:

- Current version: **5** ([helpers/hid_device_storage.h:9](helpers/hid_device_storage.h#L9))
- Increment version if you change the format or add required fields
- Old config files (version < 5) are rejected and defaults are used
- After first save, new version file is created

**Important Notes**:

- Settings are saved in **FlipperFormat** (not plain text key=value)
- **Settings MUST save immediately in callback** - never rely only on Back button save
- Settings are loaded in `hid_device_alloc()` after defaults are set
- **Always provide a sensible default** for backward compatibility
- **Always validate loaded values** (range checks for enums)
- Don't skip the validation step - prevents crashes from corrupted files
- **CRITICAL**: Always check return values from `flipper_format_write_*` functions!
  - They return `bool` and can fail silently
  - If unchecked, some settings save while others fail randomly
  - Use proper error handling with goto cleanup pattern (see [storage.c:50-110](helpers/hid_device_storage.c#L50-L110))
- **Lessons learned**:
  - Not saving immediately caused NDEF max length to always reset to 250
  - Not checking write return values caused random settings to not persist

---

## Monitoring Firmware API Changes

### Critical APIs to Watch

Monitor these firmware areas for breaking changes:

#### 1. NFC APIs
**Files to watch**: `lib/nfc/`, `furi_hal_nfc.h`

**Current usage**:
- `nfc_poller_alloc()` - Create NFC poller
- `nfc_poller_start()` - Start polling
- `nfc_poller_stop()` - Stop polling
- `nfc_poller_free()` - Cleanup
- `NfcPollerEvent` callbacks - Tag detection

**Type 4 NDEF**:
- `iso14443_4a_poller_send_apdu()` - APDU commands
- SELECT NDEF app, READ commands

**Type 5 NDEF**:
- `iso15693_poller_read_single_block()` - Block reads

**Location**: [helpers/hid_device_nfc.c](helpers/hid_device_nfc.c)

#### 2. RFID APIs
**Files to watch**: `lib/lfrfid/`, `furi_hal_rfid.h`

**Current usage**:
- `lfrfid_worker_alloc()` - Create RFID worker
- `lfrfid_worker_start_read()` - Start reading
- `lfrfid_worker_stop()` - Stop reading
- `lfrfid_worker_free()` - Cleanup
- Protocol-specific UID extraction

**Location**: [helpers/hid_device_rfid.c](helpers/hid_device_rfid.c)

#### 3. HID APIs
**Files to watch**: `furi_hal_usb_hid.h`, `furi_hal_bt_hid.h`

**USB HID current usage**:
- `furi_hal_usb_set_config(&usb_hid, NULL)` - Enable USB HID
- `furi_hal_usb_is_connected()` - Check connection
- `furi_hal_hid_kb_press()` / `release()` - Type keys

**BT HID current usage**:
- `bt_set_profile(bt, BtProfileHidKeyboard)` - Enable BT HID
- `furi_hal_bt_is_active()` - Check connection
- `furi_hal_bt_hid_kb_press()` / `release()` - Type keys

**Location**: [helpers/hid_device_hid.c](helpers/hid_device_hid.c)

#### 4. GUI/Scene APIs
**Files to watch**: `gui/`, `scene_manager.h`

**Current usage**:
- `ViewDispatcher`, `SceneManager` - Navigation
- `Submenu`, `VariableItemList` - Standard widgets
- Custom view: `HidDeviceStartscreen`

**Location**: [hid_device.c](hid_device.c), [scenes/](scenes/)

### How to Monitor

1. **Subscribe to firmware release notifications** on GitHub
2. **Read changelogs** before updating
3. **Search for "BREAKING"** in release notes
4. **Check API header files** for deprecation warnings
5. **Build regularly** against latest firmware to catch issues early

---

## Troubleshooting & Common Issues

### Build Issues

#### "Application not found" or Symlink Errors
**Symptom**: `fbt` can't find the app

**Solution**:
```bash
# Ensure symlink exists
ln -s "/home/work/contactless hid device" /home/work/flipperzero-firmware/applications_user/contactless_hid_device
```

**Note**: Build scripts create symlinks automatically, but manual builds may need this.

#### Build Warnings
**Symptom**: Compiler warnings during build

**Action**:
- **Do NOT ignore warnings** - they often indicate deprecated API usage
- Investigate each warning and fix the root cause
- Warnings today become errors in future firmware versions

#### "Undefined reference" Errors
**Symptom**: Linker errors about missing symbols

**Cause**: Firmware API changed or removed functions

**Solution**:
1. Search firmware source for new function name
2. Check firmware migration guide
3. Update code to use new API
4. Test thoroughly

### Runtime Issues

#### App Crashes on Launch
**Possible causes**:
- Firmware version mismatch (FAP built for different firmware)
- API incompatibility
- Memory allocation failure

**Diagnosis**:
1. Check Flipper firmware version matches build target
2. Flash matching firmware: `cd /path/to/firmware && ./fbt flash_usb`
3. Rebuild FAP for correct firmware
4. Check logs on Flipper (if serial console available)

#### NFC/RFID Not Detecting Tags
**Possible causes**:
- Worker not started
- Poller failure without recovery
- Hardware issue

**Diagnosis**:
1. Test with official NFC/RFID apps (verify hardware works)
2. Check debug logs (if enabled)
3. Verify worker callbacks are being called
4. Check auto-restart logic in NFC worker

#### HID Output Not Working
**Possible causes**:
- USB/BT not connected
- HID profile not started
- Character mapping issue

**Diagnosis**:
1. Check status bar shows "USB" or "BT"
2. Verify connection on host device
3. Test with simple string (e.g., "TEST")
4. Check HID worker thread is running

#### Settings Not Persisting
**Possible causes**:
- SD card not mounted
- Storage API failure
- Permissions issue

**Diagnosis**:
1. Check `/ext/apps_data/hid_device/` exists
2. Verify SD card is mounted and writable
3. Check storage API return codes
4. Test with minimal settings (just one value)

---

## Quality Gates for Updates

### Before Committing Any Code Change

- [ ] Code compiles on **all** supported firmwares without warnings
- [ ] Change is **necessary** (bug fix, firmware adaptation, or requested feature)
- [ ] Change does **not** break existing functionality (regression test)
- [ ] Change follows **existing code style** and patterns
- [ ] No over-engineering or unnecessary refactoring

### Before Tagging a Release

- [ ] **Full regression test** passes on all firmwares
- [ ] All changes documented in [docs/changelog.md](docs/changelog.md)
- [ ] README.md updated if user-facing changes
- [ ] Build scripts tested and working
- [ ] FAP files built for all firmwares
- [ ] Version number incremented in `application.fam`
- [ ] Git tag created with version number

### Release Testing Matrix

Test **every combination**:

| Firmware | NFC | RFID | NDEF | USB HID | BT HID | Settings |
|----------|-----|------|------|---------|--------|----------|
| Official | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Unleashed | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Momentum | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| RogueMaster | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ |

**Legend**:
- ✅ Fully tested and working
- ⚠️ Tested with known issues documented
- ❌ Not working (blocking issue)

**Goal**: All checkmarks green before release (RogueMaster warnings acceptable if documented).

---

## Communication Protocol

### When Firmware Updates Break Compatibility

If a firmware update introduces breaking API changes:

1. **Document the issue** in a new GitHub issue
2. **Identify the breaking change** (API function, behavior change, etc.)
3. **Find the new API** in firmware source or docs
4. **Implement fix** following existing patterns
5. **Test thoroughly** on affected firmware
6. **Verify other firmwares** still work
7. **Update changelog** with firmware-specific notes

### When to Ask for Help

Ask the user for guidance when:
- Multiple equally valid solutions exist for an API change
- Firmware behavior changed without clear migration path
- Test results are ambiguous or inconsistent
- Major architectural decision needed

Provide:
- Clear description of the problem
- Firmware versions affected
- Potential solutions with trade-offs
- Recommendation based on project goals

---

## Reference: UX Guidelines

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
  - "No NDEF Found" (NDEF mode with non-NDEF tag, show with red LED)
  - "Connect USB or BT HID" (no connection)

---

## Reference: Common Pitfalls

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

## Final Notes

### Stability is the Goal

This project is **feature-complete**. The goal is to **maintain stability** as firmwares evolve, not to add new features.

**Resist the temptation to**:
- Refactor code that works
- Add "nice to have" features
- Optimize without profiling
- Change UX without user feedback

**Focus energy on**:
- Keeping up with firmware API changes
- Fixing reported bugs
- Maintaining cross-firmware compatibility
- Improving test coverage
- Documenting quirks and workarounds

### Cross-Firmware Compatibility is Critical

Users rely on **all four firmwares**. A release that breaks one firmware is unacceptable.

**Always**:
- Test on all firmwares before releasing
- Document firmware-specific issues
- Provide workarounds when possible
- Keep build scripts up to date

### When in Doubt, Test More

If uncertain about a change:
- Run more tests
- Test on more firmwares
- Test on actual hardware
- Ask for user validation

**Better to be slow and stable than fast and broken.**

---

*This document should be the primary reference for maintaining the Contactless HID Device app. Update it when new patterns or firmware-specific quirks are discovered.*
