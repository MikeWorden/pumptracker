#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <math.h>
#include "driver/i2s_std.h"

// ==========================================
// Network & Time Synchronization Settings
// ==========================================
const char* const WIFI_SSID       = "YOUR_WIFI_SSID";
const char* const WIFI_PASS       = "YOUR_WIFI_PASS";
const char* const NTP_SERVER      = "pool.ntp.org";
const long        GMT_OFFSET_SEC  = -25200;           // UTC-7 (Mountain Time)
const int         DAYLIGHT_OFFSET = 3600;

// ==========================================
// MQTT Broker Configuration
// ==========================================
const char* const MQTT_BROKER       = "192.168.1.100";  // Broker IP or hostname
const int         MQTT_PORT         = 1883;
const char* const MQTT_USER         = "";               // Optional (leave empty if none)
const char* const MQTT_PASS         = "";               // Optional
const char* const MQTT_CLIENT_ID    = "pumptracker-c5";

// MQTT Topics
const char* const TOPIC_STATE       = "pumptracker/state";
const char* const TOPIC_EVENTS      = "pumptracker/events";

// ==========================================
// Hardware Pinout (ESP32-C5)
// ==========================================
#define I2S_MIC_SCK_PIN   GPIO_NUM_3                  // SCK / BCLK
#define I2S_MIC_WS_PIN    GPIO_NUM_2                  // WS / LRCK
#define I2S_MIC_SD_PIN    GPIO_NUM_1                  // SD / DOUT

// ==========================================
// Audio & Detection Parameters
// ==========================================
#define SAMPLE_RATE       16000
#define BUFFER_SAMPLES    128

const float    RMS_THRESHOLD         = 800.0f;
const uint32_t DEBOUNCE_ACTIVE_COUNT = 3;             // ~300ms sustained noise
const uint32_t DEBOUNCE_IDLE_COUNT   = 10;            // ~1.0s silence

// ==========================================
// State Machine Types
// ==========================================
enum PumpState {
    PUMP_IDLE,
    PUMP_RUNNING
};