#include <stdio.h>
#include <string.h>
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "mqtt_client.h"

/* Project includes */
#include "init.h"
#include "events.h"
#include "wifi.h"
#include "ota.h"
#include "mqtt.h"
#include "display.h"
#include "dht_handler.h"
#include "scale.h"
#include "motor.h"
#include "feeder.h"
#include "scheduler.h"
#include "pins.h"
#include "tof.h"
#include "nfc.h"

static const char *MAIN_TAG = "main";
static const char *SENSOR_TAG = "sensor";

/* Sensor task: reads DHT11 + scale every 2 s, updates display when idle */
static void sensor_task(void *pvParameter)
{
    TickType_t last_wake = xTaskGetTickCount();

    dht.begin();
    nfc_init();
    vl53_init();
    scale_init();

    int16_t fill_pct = -1;

    /* NFC bowl-tracking state */
    uint8_t last_uid[7] = {0};
    uint8_t last_uid_len = 0;
    int     nfc_miss_count = 0;

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(2000));

        /* ── DHT11 (1-wire, no shared bus) ──────────────────────────── */
        float temp = dht.readTemperature();
        float hum  = dht.readHumidity();
        if (!isnan(temp) && !isnan(hum)) {
            temperatura = temp;
            humidade    = hum;
        }

        /* ── HX711 scale (GPIO bit-bang) ────────────────────────────── */
        xSemaphoreTake(i2c_mutex, portMAX_DELAY);
        float weight = scale.get_units();
        xSemaphoreGive(i2c_mutex);

        /* ── Publish temperature (MQTT API is thread-safe) ───────────── */
        if (mqtt_client != NULL) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f", temperatura);
            esp_mqtt_client_publish(mqtt_client, "feeder/temp", buf, 0, 0, 0);
        }

        /* ── VL53L1X ToF (I2C) ───────────────────────────────────────── */
        int16_t distance = -1;
        xSemaphoreTake(i2c_mutex, portMAX_DELAY);
        bool tof_ok = vl53_read(&distance);
        xSemaphoreGive(i2c_mutex);

        if (tof_ok) {
            ESP_LOGI(SENSOR_TAG, "Distance: %d mm", distance);
        }

        if ((distance != -1) && (xEventGroupGetBits(system_event_group) & RESERVOIR_UPDATE_BIT)) {
            xEventGroupClearBits(system_event_group, RESERVOIR_UPDATE_BIT);
            fill_pct = tof_fill_percent(distance);
        }
        else if (distance == -1) {
            fill_pct = -1; /* lid open */
        }

        /* ── Display update (queue send, no hardware) ────────────────── */
        display_post_status(weight, temperatura, humidade, fill_pct);

        /* ── NFC bowl detection (I2C + HX711 offset update) ─────────── */
        uint8_t uid[7];
        uint8_t uidLen = 0;

        xSemaphoreTake(i2c_mutex, portMAX_DELAY);
        bool nfc_seen = nfc_poll_uid(uid, &uidLen);

        if (nfc_seen) {
            nfc_miss_count = 0;

            /* New bowl? (different UID from last) */
            if (uidLen != last_uid_len || memcmp(uid, last_uid, uidLen) != 0) {
                float bowl_g = 0.0f;
                if (nfc_read_bowl_weight(&bowl_g)) {
                    long new_offset = g_raw_offset + (long)(bowl_g * scale.get_scale());
                    scale.set_offset(new_offset);
                    ESP_LOGI(SENSOR_TAG, "Bowl detected: %.1f g tare", bowl_g);
                } else if (g_bowl_g > 0) {
                    /* NFC tag unreadable – fall back to the configured bowl weight */
                    long new_offset = g_raw_offset + (long)(g_bowl_g * scale.get_scale());
                    scale.set_offset(new_offset);
                    ESP_LOGW(SENSOR_TAG, "Tag found but unreadable; using configured %.0f g bowl", (float)g_bowl_g);
                } else {
                    ESP_LOGW(SENSOR_TAG, "Tag found but failed to read page 6");
                }
                memcpy(last_uid, uid, uidLen);
                last_uid_len = uidLen;
            }
        } else {
            /* No tag – revert offset after 3 consecutive misses (~6 s) */
            if (last_uid_len != 0) {
                nfc_miss_count++;
                if (nfc_miss_count >= 3) {
                    scale.set_offset(g_raw_offset);
                    last_uid_len = 0;
                    memset(last_uid, 0, sizeof(last_uid));
                    ESP_LOGI(SENSOR_TAG, "Bowl removed, offset reverted");
                }
            }
        }
        xSemaphoreGive(i2c_mutex);
    }
}

static void configure_pins(void)
{
    gpio_config_t io_conf_output = {
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf_output);

    gpio_config_t io_conf_input = {
        .pin_bit_mask = GPIO_INPUT_PIN_SEL,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf_input);
}

extern "C" void app_main(void)
{
    /* 1. RTOS primitives (before anything else) */
    initialize_system();

    /* 2. Arduino framework + nvs_flash_init */
    initArduino();

    /* 3. Load NVS-persisted settings (broker URL, schedule, cal factor) */
    nvs_load_settings();

    /* 4. Network – captive portal or STA */
    initialize_wifi();

    /* 5. GPIO */
    configure_pins();

    /* 6. Peripherals */
    Serial.begin(115200);

    ESP_LOGI(MAIN_TAG, "Init done. Launching tasks...");

    /* 7. FreeRTOS tasks
     *   Priority: feeder (6) > display (5) > sensor (3) > scheduler (2) > ota (1)
     */
    xTaskCreate(feeder_task,    "feeder",    4096,  NULL, tskIDLE_PRIORITY + 6, NULL);
    xTaskCreate(display_task,   "display",   4096,  NULL, tskIDLE_PRIORITY + 5, NULL);
    xTaskCreate(sensor_task,    "sensor",    4096,  NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(scheduler_task, "scheduler", 4096,  NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(ota_task,       "ota",       12288, NULL, tskIDLE_PRIORITY + 1, NULL);

    /* 8. MQTT client (runs in its own internal task) */
    mqtt_app_start();

    ESP_LOGI(MAIN_TAG, "All tasks launched.");
    vTaskDelete(NULL); /* boot task no longer needed */
}