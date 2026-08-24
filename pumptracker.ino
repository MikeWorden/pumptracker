#include "pumptracker.h"

// --- Global State & Buffers ---
static PumpState currentState = PUMP_IDLE;
static uint32_t activeCounter = 0;
static uint32_t idleCounter   = 0;
static unsigned long pumpStartTimeMs = 0;
static time_t pumpStartEpoch = 0;

static int32_t raw_samples[BUFFER_SAMPLES];
static i2s_chan_handle_t rx_handle = NULL;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setupI2S() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 2;
    chan_cfg.dma_frame_num = BUFFER_SAMPLES;
    
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode      = I2S_SLOT_MODE_STEREO,
            .slot_mask      = I2S_STD_SLOT_BOTH,
            .ws_width       = 32,
            .ws_pol         = false,
            .bit_shift      = false,
            .left_align     = true,
            .big_endian     = false,
            .bit_order_lsb  = false
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_MIC_SCK_PIN,
            .ws   = I2S_MIC_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_MIC_SD_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = true,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

String getTimestamp(time_t rawTime) {
    struct tm timeinfo;
    localtime_r(&rawTime, &timeinfo);
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    return String(buf);
}

void ensureMQTTConnected() {
    if (mqttClient.connected()) return;

    static unsigned long lastReconnectAttempt = 0;
    unsigned long now = millis();

    // Non-blocking reconnect attempt every 5 seconds
    if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        Serial.print("[MQTT] Connecting to broker...");
        
        bool connected = false;
        if (strlen(MQTT_USER) > 0) {
            connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, TOPIC_STATE, 0, true, "offline");
        } else {
            connected = mqttClient.connect(MQTT_CLIENT_ID, TOPIC_STATE, 0, true, "offline");
        }

        if (connected) {
            Serial.println(" connected.");
            mqttClient.publish(TOPIC_STATE, "online", true);
        } else {
            Serial.printf(" failed, rc=%d. Will retry.\n", mqttClient.state());
        }
    }
}

void publishStartEvent(time_t startTime, float rmsVal) {
    char payload[160];
    snprintf(payload, sizeof(payload), 
             "{\"event\":\"start\",\"timestamp\":\"%s\",\"rms\":%.1f}", 
             getTimestamp(startTime).c_str(), rmsVal);

    mqttClient.publish(TOPIC_EVENTS, payload);
    Serial.printf("[MQTT PUB] %s -> %s\n", TOPIC_EVENTS, payload);
}

void publishCompleteEvent(time_t startTime, float durationSec) {
    char payload[192];
    snprintf(payload, sizeof(payload), 
             "{\"event\":\"complete\",\"start_time\":\"%s\",\"duration_sec\":%.2f}", 
             getTimestamp(startTime).c_str(), durationSec);

    mqttClient.publish(TOPIC_EVENTS, payload);
    Serial.printf("[MQTT PUB] %s -> %s\n", TOPIC_EVENTS, payload);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n[INIT] Starting pumptracker...");

    // Wi-Fi Setup
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WIFI] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WIFI] Connected! IP: %s (Channel %d)\n", 
                  WiFi.localIP().toString().c_str(), WiFi.channel());

    // NTP Setup
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
        Serial.printf("[NTP] Time synchronized: %s", asctime(&timeinfo));
    } else {
        Serial.println("[NTP] Warning: Sync timed out, continuing background sync.");
    }

    // MQTT Setup
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setBufferSize(256);

    setupI2S();
    Serial.println("[pumptracker] Acoustic monitoring and MQTT active.\n");
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        ensureMQTTConnected();
        mqttClient.loop();
    }

    if (!rx_handle) return;

    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(
        rx_handle, 
        raw_samples, 
        sizeof(raw_samples), 
        &bytes_read, 
        pdMS_TO_TICKS(100)
    );

    if (ret == ESP_OK && bytes_read > 0) {
        size_t total_words = bytes_read / sizeof(int32_t);
        double sum = 0.0;
        size_t count = 0;

        for (size_t i = 0; i < total_words; i += 2) {
            int32_t sample = raw_samples[i] >> 14;
            sum += (double)sample * (double)sample;
            count++;
        }

        float rms = (count > 0) ? (float)sqrt(sum / count) : 0.0f;

        if (rms >= RMS_THRESHOLD) {
            activeCounter++;
            idleCounter = 0;
        } else {
            idleCounter++;
            activeCounter = 0;
        }

        // Transition: PUMP_IDLE -> PUMP_RUNNING
        if (currentState == PUMP_IDLE && activeCounter >= DEBOUNCE_ACTIVE_COUNT) {
            currentState = PUMP_RUNNING;
            pumpStartTimeMs = millis();
            time(&pumpStartEpoch);

            Serial.println("==================================================");
            Serial.printf("[EVENT START] Pump Activated at %s (RMS: %.1f)\n", 
                          getTimestamp(pumpStartEpoch).c_str(), rms);
            Serial.println("==================================================");

            publishStartEvent(pumpStartEpoch, rms);
        }

        // Transition: PUMP_RUNNING -> PUMP_IDLE
        if (currentState == PUMP_RUNNING && idleCounter >= DEBOUNCE_IDLE_COUNT) {
            currentState = PUMP_IDLE;
            float durationSec = (millis() - pumpStartTimeMs) / 1000.0f;

            Serial.println("--------------------------------------------------");
            Serial.printf("[EVENT COMPLETE] Start: %s | Duration: %.2f s\n", 
                          getTimestamp(pumpStartEpoch).c_str(), durationSec);
            Serial.println("--------------------------------------------------");

            publishCompleteEvent(pumpStartEpoch, durationSec);
        }
    }
}