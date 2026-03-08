#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BH1750.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config.h"

// --- NTP ---
// Used to get a real timestamp for each reading.
// Adjust timezone offset (seconds) if you want local time,
// or leave at 0 to store UTC (recommended for InfluxDB/Grafana).
#define NTP_SERVER    "pool.ntp.org"
#define NTP_UTC_OFFSET 0
#define NTP_DST_OFFSET 0

BH1750 lightMeter;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastSampleTime = 0;

// ----------------------------------------------------------------
// WiFi
// ----------------------------------------------------------------
void connectWifi() {
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
}

// ----------------------------------------------------------------
// MQTT
// ----------------------------------------------------------------
void connectMqtt() {
    while (!mqttClient.connected()) {
        Serial.printf("Connecting to MQTT broker: %s:%d\n", MQTT_BROKER, MQTT_PORT);

        bool connected = (strlen(MQTT_USER) > 0)
            ? mqttClient.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD)
            : mqttClient.connect(DEVICE_ID);

        if (connected) {
            Serial.println("MQTT connected.");
        } else {
            Serial.printf("MQTT connect failed, rc=%d. Retrying in 5s\n", mqttClient.state());
            delay(5000);
        }
    }
}

// ----------------------------------------------------------------
// NTP sync - call once after WiFi connects
// ----------------------------------------------------------------
void syncTime() {
    configTime(NTP_UTC_OFFSET, NTP_DST_OFFSET, NTP_SERVER);
    Serial.print("Waiting for NTP sync");
    time_t now = 0;
    int retries = 0;
    while (now < 1000000000L && retries < 20) {
        delay(500);
        Serial.print(".");
        time(&now);
        retries++;
    }
    Serial.println(now > 0 ? "\nNTP synced." : "\nNTP sync failed - timestamps will be 0.");
}

// ----------------------------------------------------------------
// Publish a reading
// ----------------------------------------------------------------
void publishReading(float lux) {
    time_t now;
    time(&now);

    // Build JSON payload
    // Example: {"device_id":"garden_sensor_01","lux":4521.5,"timestamp":1712000000}
    JsonDocument doc;
    doc["device_id"]  = DEVICE_ID;
    doc["lux"]        = lux;
    doc["timestamp"]  = (long)now;

    char payload[128];
    serializeJson(doc, payload, sizeof(payload));

    Serial.printf("Publishing to %s: %s\n", MQTT_TOPIC, payload);

    if (!mqttClient.publish(MQTT_TOPIC, payload)) {
        Serial.println("Publish failed.");
    }
}

// ----------------------------------------------------------------
// Setup
// ----------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- Garden Light Sensor ---");
    Serial.printf("Device ID: %s\n", DEVICE_ID);

    // Init I2C and BH1750
    Wire.begin();
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
        Serial.println("ERROR: BH1750 not found. Check wiring.");
        while (true) { delay(1000); }
    }
    Serial.println("BH1750 initialised.");

    connectWifi();
    syncTime();

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

    // Take an immediate reading on boot rather than waiting for first interval
    lastSampleTime = millis() - SAMPLE_INTERVAL_MS;
}

// ----------------------------------------------------------------
// Loop
// ----------------------------------------------------------------
void loop() {
    // Maintain MQTT connection
    if (!mqttClient.connected()) {
        connectMqtt();
    }
    mqttClient.loop();

    // Sample on interval
    unsigned long now = millis();
    if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime = now;

        float lux = lightMeter.readLightLevel();

        if (lux < 0) {
            Serial.println("BH1750 read error.");
        } else {
            Serial.printf("Lux: %.1f\n", lux);
            publishReading(lux);
        }
    }
}
