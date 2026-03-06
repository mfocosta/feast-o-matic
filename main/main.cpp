#include <stdio.h>
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

static const char *TAG = "main";

/* Sensor task: reads DHT11 + scale every 2 s, updates display when idle */
static void sensor_task(void *pvParameter)
{
    TickType_t last_wake = xTaskGetTickCount();

    dht.begin();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(2000));

        float temp = dht.readTemperature();
        float hum  = dht.readHumidity();
        float w    = scale.get_units();

        if (!isnan(temp) && !isnan(hum)) {
            temperatura = temp;
            humidade    = hum;
        }
        current_weight = w;

        /* Publish temperature */
        if (mqtt_client != NULL) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f", temperatura);
            esp_mqtt_client_publish(mqtt_client, "feeder/temp", buf, 0, 0, 0);
        }

        /* Update display */
        display_post_status(current_weight, temperatura, humidade);
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
        .intr_type    = GPIO_INTR_ENABLE,
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

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(g_cal_factor);
    scale.tare();

    stepper.setSpeed(10);

    ESP_LOGI(TAG, "Hardware ready. Launching tasks...");

    /* 7. FreeRTOS tasks
     *   Priority: feeder (6) > display (5) > sensor (3) > scheduler (2) > ota (1)
     */
    xTaskCreate(display_task,   "display",   4096,  NULL, tskIDLE_PRIORITY + 5, NULL);
    xTaskCreate(feeder_task,    "feeder",    4096,  NULL, tskIDLE_PRIORITY + 6, NULL);
    xTaskCreate(sensor_task,    "sensor",    4096,  NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(scheduler_task, "scheduler", 4096,  NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(ota_task,       "ota",       12288, NULL, tskIDLE_PRIORITY + 1, NULL);

    /* 8. MQTT client (runs in its own internal task) */
    mqtt_app_start();

    ESP_LOGI(TAG, "All tasks launched.");
    vTaskDelete(NULL); /* boot task no longer needed */
}