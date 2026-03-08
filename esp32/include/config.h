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

// --- Sampling ---
// How often to take a reading and publish (milliseconds)

//#define SAMPLE_INTERVAL_MS 60000   // 1 minute
#define SAMPLE_INTERVAL_MS 5000   // 5 seconds
