#pragma once

// --- Device Identity ---
// Unique name for this sensor node. Used as the MQTT client ID
// and included in the JSON payload so Grafana can distinguish devices.
#define DEVICE_ID "garden_sensor_01"

// --- WiFi ---
#define WIFI_SSID     "TP-Link_AP"
#define WIFI_PASSWORD "greenwater085"

// --- MQTT Broker ---
#define MQTT_BROKER   "192.168.1.12"   // IP of your Mosquitto container
#define MQTT_PORT     1883
#define MQTT_TOPIC    "sensors/garden/" DEVICE_ID

// Credentials - leave empty strings if your broker has no auth
#define MQTT_USER     ""
#define MQTT_PASSWORD ""

// --- Sleep / Sampling ---
// How long to sleep between wake cycles (seconds)
#define SLEEP_DURATION_SEC  10        // dev: 10 seconds
//#define SLEEP_DURATION_SEC  600     // production: 10 minutes

// How long to collect lux samples each wake cycle (milliseconds)
// Each BH1750 ONE_TIME_HIGH_RES_MODE reading takes ~150ms
// 10000ms window = ~66 samples averaged together
#define SAMPLE_WINDOW_MS    10000     // 10 seconds

// --- Timeouts ---
#define WIFI_TIMEOUT_MS     10000     // 10 seconds
#define NTP_TIMEOUT_MS       5000     // 5 seconds
#define MQTT_TIMEOUT_MS      5000     // 5 seconds

