# Changelog

All notable changes to the Flipper Wedge project will be documented in this file.

## v1.0 - 2024-11-24

**Multi-mode NFC/RFID scanning**
- NFC Only mode (ISO14443A/B, MIFARE, NTAG)
- RFID Only mode (EM4100, HID Prox, Indala)
- NDEF Mode (dedicated NDEF text record parsing)
- NFC + RFID combo mode (scan both, output combined UIDs)
- RFID + NFC combo mode (reverse order)
- Mode persistence - remembers last selected scan mode

**NDEF Support**
- Type 2 NDEF text records (MIFARE Ultralight, NTAG series)
- Type 4 NDEF support with APDU commands (ISO14443-4A)
- Type 5 NDEF support (ISO15693 tags)
- Automatic NDEF parsing and text extraction
- NDEF error type distinction for unsupported NFC Forum Types
- Retry logic for robust NDEF reading

**HID Output**
- USB HID keyboard emulation
- Bluetooth HID keyboard emulation
- Dual output support (USB + BT simultaneously)
- Connection status detection and display
- Automatic typing with configurable delimiter

**User Interface**
- Main scanning screen with real-time status
- Mode selection menu
- Advanced Settings screen
  - Custom delimiter selection (space, colon, dash, none)
  - Enter key append toggle
  - Bluetooth enable/disable toggle
  - USB debug mode toggle
- Bluetooth pairing screen
- Visual feedback (LED: green/red for success/error)
- Haptic feedback on scans
- Optional audio feedback

**Reliability Features**
- Automatic error recovery for NFC poller failures
- Timeout handling for combo modes (5 seconds)
- Tag removal detection
- Cooldown period to prevent accidental re-scans
- Settings persistence across app restarts

**Technical Details**
- Built on official Flipper Zero firmware (0.105.0+)
- Compatible with Unleashed, Xtreme, and RogueMaster firmwares
- Modular architecture with separate helpers for NFC, RFID, HID, and formatting
- Comprehensive error handling and user feedback
- Clean separation of concerns (scenes, views, helpers)
