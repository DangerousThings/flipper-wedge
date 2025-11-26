#pragma once

#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format_i.h>
#include "../hid_device.h"

#define HID_DEVICE_SETTINGS_FILE_VERSION 5
#define CONFIG_FILE_DIRECTORY_PATH EXT_PATH("apps_data/hid_device")
#define HID_DEVICE_SETTINGS_SAVE_PATH CONFIG_FILE_DIRECTORY_PATH "/hid_device.conf"
#define HID_DEVICE_SETTINGS_SAVE_PATH_TMP HID_DEVICE_SETTINGS_SAVE_PATH ".tmp"
#define HID_DEVICE_SETTINGS_HEADER "HidDevice Config File"
#define HID_DEVICE_SETTINGS_KEY_DELIMITER "Delimiter"
#define HID_DEVICE_SETTINGS_KEY_APPEND_ENTER "AppendEnter"
#define HID_DEVICE_SETTINGS_KEY_MODE "Mode"
#define HID_DEVICE_SETTINGS_KEY_MODE_STARTUP "ModeStartup"
#define HID_DEVICE_SETTINGS_KEY_OUTPUT_MODE "OutputMode"
#define HID_DEVICE_SETTINGS_KEY_USB_DEBUG "UsbDebug"
#define HID_DEVICE_SETTINGS_KEY_VIBRATION "Vibration"
#define HID_DEVICE_SETTINGS_KEY_LOG_TO_SD "LogToSd"

void hid_device_save_settings(void* context);
void hid_device_read_settings(void* context);