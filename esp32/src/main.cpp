#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BH1750.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config.h"

// --- NTP ---
#define NTP_SERVER     "pool.ntp.org"
#define NTP_UTC_OFFSET 0
#define NTP_DST_OFFSET 0

// --- RTC memory (survives deep sleep) ---
// Stores WiFi channel + BSSID for fast reconnect, and boot count.
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR uint8_t rtcBssid[6];
RTC_DATA_ATTR uint8_t rtcChannel;
RTC_DATA_ATTR bool    rtcWifiValid = false;

BH1750     lightMeter;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ----------------------------------------------------------------
// WiFi - fast reconnect on subsequent boots using RTC-cached info
// ----------------------------------------------------------------
bool connectWifi() {
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);

    if (rtcWifiValid) {
        // Use cached channel + BSSID to skip full scan
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD, rtcChannel, rtcBssid, true);
    } else {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println("\nWiFi timeout.");
            // Invalidate cached info so next boot does a full scan
            rtcWifiValid = false;
            return false;
        }
        delay(100);
        Serial.print(".");
    }

    // Cache connection info for next wake
    rtcChannel  = WiFi.channel();
    memcpy(rtcBssid, WiFi.BSSID(), 6);
    rtcWifiValid = true;

    Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// ----------------------------------------------------------------
// NTP sync
// ----------------------------------------------------------------
bool syncTime() {
    configTime(NTP_UTC_OFFSET, NTP_DST_OFFSET, NTP_SERVER);
    Serial.print("Waiting for NTP sync");
    time_t now = 0;
    unsigned long start = millis();
    while (now < 1000000000L) {
        if (millis() - start > NTP_TIMEOUT_MS) {
            Serial.println("\nNTP sync timeout.");
            return false;
        }
        delay(200);
        Serial.print(".");
        time(&now);
    }
    Serial.println("\nNTP synced.");
    return true;
}

// ----------------------------------------------------------------
// MQTT connect
// ----------------------------------------------------------------
bool connectMqtt() {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    Serial.printf("Connecting to MQTT: %s:%d\n", MQTT_BROKER, MQTT_PORT);

    unsigned long start = millis();
    while (!mqttClient.connected()) {
        if (millis() - start > MQTT_TIMEOUT_MS) {
            Serial.println("MQTT timeout.");
            return false;
        }

        bool ok = (strlen(MQTT_USER) > 0)
            ? mqttClient.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD)
            : mqttClient.connect(DEVICE_ID);

        if (!ok) {
            Serial.printf("MQTT failed rc=%d, retrying...\n", mqttClient.state());
            delay(500);
        }
    }
    Serial.println("MQTT connected.");
    return true;
}

// ----------------------------------------------------------------
// Sample lux for SAMPLE_WINDOW_MS, return average
// ----------------------------------------------------------------
float sampleAverageLux() {
    // ONE_TIME_HIGH_RES_MODE: sensor powers down after each read (power efficient)
    // Each measurement takes ~120ms

    float   sum     = 0;
    int     count   = 0;
    unsigned long windowStart = millis();

    Serial.printf("Sampling lux for %d ms...\n", SAMPLE_WINDOW_MS);

    while (millis() - windowStart < SAMPLE_WINDOW_MS) {
        lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
        delay(150); // wait for measurement to complete (~120ms + margin)

        float lux = lightMeter.readLightLevel();
        if (lux >= 0) {
            sum += lux;
            count++;
            Serial.printf("  Sample %d: %.2f lux\n", count, lux);
        } else {
            Serial.println("  BH1750 read error, skipping sample.");
        }
    }

    if (count == 0) {
        Serial.println("No valid samples collected.");
        return -1.0f;
    }

    float avg = sum / count;
    Serial.printf("Average lux over %d samples: %.2f\n", count, avg);
    return avg;
}

// ----------------------------------------------------------------
// Publish reading
// ----------------------------------------------------------------
bool publishReading(float lux) {
    time_t now;
    time(&now);

    JsonDocument doc;
    doc["device_id"]  = DEVICE_ID;
    doc["lux"]        = lux;
    doc["timestamp"]  = (long)now;
    doc["boot_count"] = bootCount;

    char payload[128];
    serializeJson(doc, payload, sizeof(payload));

    Serial.printf("Publishing to %s: %s\n", MQTT_TOPIC, payload);

    if (!mqttClient.publish(MQTT_TOPIC, payload)) {
        Serial.println("Publish failed.");
        return false;
    }
    return true;
}

// ----------------------------------------------------------------
// Go to deep sleep
// ----------------------------------------------------------------
void goToSleep() {
    Serial.printf("Going to sleep for %d seconds.\n", SLEEP_DURATION_SEC);
    Serial.flush();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_DURATION_SEC * 1000000ULL);
    esp_deep_sleep_start();
}

// ----------------------------------------------------------------
// Setup - runs once per wake cycle, then sleeps
// ----------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(200);

    bootCount++;
    Serial.printf("\n--- Garden Light Sensor (boot #%d) ---\n", bootCount);
    Serial.printf("Device ID: %s\n", DEVICE_ID);

    // Init I2C and BH1750
    Wire.begin();
    if (!lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
        Serial.println("ERROR: BH1750 not found. Check wiring. Going to sleep.");
        goToSleep();
    }
    Serial.println("BH1750 initialised.");

    // WiFi
    if (!connectWifi()) {
        Serial.println("WiFi failed. Going to sleep.");
        goToSleep();
    }

    // NTP
    if (!syncTime()) {
        Serial.println("NTP failed. Going to sleep.");
        goToSleep();
    }

    // Sample lux
    float avgLux = sampleAverageLux();
    if (avgLux < 0) {
        Serial.println("Sampling failed. Going to sleep.");
        goToSleep();
    }

    // MQTT
    if (!connectMqtt()) {
        Serial.println("MQTT failed. Going to sleep.");
        goToSleep();
    }

    // Publish
    publishReading(avgLux);

    // Allow MQTT to flush
    mqttClient.loop();
    delay(200);

    goToSleep();
}

// ----------------------------------------------------------------
// Loop - unused in deep sleep architecture
// ----------------------------------------------------------------
void loop() {
    // intentionally empty
}
