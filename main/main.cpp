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

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(2000));

        if (is_feeding) continue; /* feeder owns display and scale */

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

        if (xSemaphoreTake(display_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            display_show_status(current_weight, temperatura, humidade);
            xSemaphoreGive(display_mutex);
        }
    }
}

static void configure_pins(void)
{
    gpio_config_t io_conf = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
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
    dht.begin();

    display_init();
    display_show_feast_logo();
    display_show_startup();

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(g_cal_factor);
    scale.tare();

    stepper.setSpeed(10);

    ESP_LOGI(TAG, "Hardware ready. Launching tasks...");

    /* 7. FreeRTOS tasks
     *   Priority: feeder (5) > sensor (3) > scheduler (2) > ota (1)
     */
    xTaskCreate(feeder_task,    "feeder",    4096,  NULL, 5, NULL);
    xTaskCreate(sensor_task,    "sensor",    4096,  NULL, 3, NULL);
    xTaskCreate(scheduler_task, "scheduler", 4096,  NULL, 2, NULL);
    xTaskCreate(ota_task,       "ota",       12288, NULL, 1, NULL);

    /* 8. MQTT client (runs in its own internal task) */
    mqtt_app_start();

    ESP_LOGI(TAG, "All tasks launched.");
    vTaskDelete(NULL); /* boot task no longer needed */
}