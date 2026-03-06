#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"
#include "events.h"
#include "init.h"

static const char *TAG = "system_init";
static const char *NVS_NS = "feeder";

/* Shared RTOS handles */
QueueHandle_t      logic_queue        = NULL;
QueueHandle_t      display_queue      = NULL;
EventGroupHandle_t system_event_group = NULL;

/* Persisted settings – defaults come from Kconfig */
int  g_sched_hour   = -1;
int  g_sched_minute = 0;
int  g_sched_grams  = 100;
int  g_cal_factor   = CONFIG_HX711_CALIBRATION_FACTOR;
char g_mqtt_broker[128] = CONFIG_MQTT_BROKER_URL;

void nvs_load_settings(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "No saved settings, using defaults");
        return;
    }
    int32_t val;
    if (nvs_get_i32(nvs, "sched_hour",   &val) == ESP_OK) g_sched_hour   = (int)val;
    if (nvs_get_i32(nvs, "sched_minute", &val) == ESP_OK) g_sched_minute = (int)val;
    if (nvs_get_i32(nvs, "sched_grams",  &val) == ESP_OK) g_sched_grams  = (int)val;
    if (nvs_get_i32(nvs, "cal_factor",   &val) == ESP_OK) g_cal_factor   = (int)val;
    size_t len = sizeof(g_mqtt_broker);
    nvs_get_str(nvs, "mqtt_broker", g_mqtt_broker, &len);
    nvs_close(nvs);
    ESP_LOGI(TAG, "Settings loaded from NVS");
}

void nvs_save_settings(void)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READWRITE, &nvs));
    nvs_set_i32(nvs, "sched_hour",   (int32_t)g_sched_hour);
    nvs_set_i32(nvs, "sched_minute", (int32_t)g_sched_minute);
    nvs_set_i32(nvs, "sched_grams",  (int32_t)g_sched_grams);
    nvs_set_i32(nvs, "cal_factor",   (int32_t)g_cal_factor);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "Settings saved to NVS");
}

void nvs_save_broker(const char *broker_url)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READWRITE, &nvs));
    nvs_set_str(nvs, "mqtt_broker", broker_url);
    nvs_commit(nvs);
    nvs_close(nvs);
    strlcpy(g_mqtt_broker, broker_url, sizeof(g_mqtt_broker));
    ESP_LOGI(TAG, "MQTT broker saved: %s", broker_url);
}

void initialize_system(void)
{
    logic_queue        = xQueueCreate(10, sizeof(logic_queue_item_t));
    display_queue      = xQueueCreate(8,  sizeof(display_msg_t));
    system_event_group = xEventGroupCreate();

    if (!logic_queue || !display_queue || !system_event_group) {
        ESP_LOGE(TAG, "Failed to create RTOS primitives!");
        abort();
    }
    ESP_LOGI(TAG, "RTOS primitives created");
}