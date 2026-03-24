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
SemaphoreHandle_t  i2c_mutex          = NULL;

/* Persisted settings – defaults come from Kconfig */
sched_entry_t g_sched[CONFIG_SCHED_MAX] = {
    { -1, 0, 100 },
    { -1, 0, 100 },
    { -1, 0, 100 },
    { -1, 0, 100 },
};
int16_t  g_cal_factor   = CONFIG_HX711_CALIBRATION_FACTOR;
int32_t  g_raw_offset   = CONFIG_HX711_RAW_OFFSET;
int16_t  g_bowl_g       = CONFIG_BOWL_WEIGHT_GRAMS;
char g_mqtt_broker[128] = CONFIG_MQTT_BROKER_URL;

void nvs_load_settings(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "No saved settings, using defaults");
        return;
    }
    int32_t val;
    char key[16];
    for (int i = 0; i < CONFIG_SCHED_MAX; i++) {
        snprintf(key, sizeof(key), "sch%d_h", i);
        if (nvs_get_i32(nvs, key, &val) == ESP_OK) g_sched[i].hour   = (int16_t)val;
        snprintf(key, sizeof(key), "sch%d_m", i);
        if (nvs_get_i32(nvs, key, &val) == ESP_OK) g_sched[i].minute = (int16_t)val;
        snprintf(key, sizeof(key), "sch%d_g", i);
        if (nvs_get_i32(nvs, key, &val) == ESP_OK) g_sched[i].grams  = (int16_t)val;
    }
    if (nvs_get_i32(nvs, "cal_factor",   &val) == ESP_OK) g_cal_factor   = (int16_t)val;
    if (nvs_get_i32(nvs, "raw_offset",   &val) == ESP_OK) g_raw_offset   = (int32_t)val;
    if (nvs_get_i32(nvs, "bowl_g",       &val) == ESP_OK) g_bowl_g       = (int16_t)val;
    size_t len = sizeof(g_mqtt_broker);
    nvs_get_str(nvs, "mqtt_broker", g_mqtt_broker, &len);
    nvs_close(nvs);
    ESP_LOGI(TAG, "Settings loaded from NVS");
}

void nvs_save_settings(void)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READWRITE, &nvs));
    char key[16];
    bool ok = true;
    for (int i = 0; i < CONFIG_SCHED_MAX; i++) {
        snprintf(key, sizeof(key), "sch%d_h", i);
        ok = ok && (nvs_set_i32(nvs, key, (int32_t)g_sched[i].hour)   == ESP_OK);
        snprintf(key, sizeof(key), "sch%d_m", i);
        ok = ok && (nvs_set_i32(nvs, key, (int32_t)g_sched[i].minute) == ESP_OK);
        snprintf(key, sizeof(key), "sch%d_g", i);
        ok = ok && (nvs_set_i32(nvs, key, (int32_t)g_sched[i].grams)  == ESP_OK);
    }
    ok = ok && (nvs_set_i32(nvs, "cal_factor", (int32_t)g_cal_factor) == ESP_OK);
    ok = ok && (nvs_set_i32(nvs, "raw_offset", (int32_t)g_raw_offset) == ESP_OK);
    ok = ok && (nvs_set_i32(nvs, "bowl_g",     (int32_t)g_bowl_g)     == ESP_OK);
    if (!ok) {
        ESP_LOGE(TAG, "One or more NVS writes failed");
    }
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    ESP_LOGI(TAG, "Settings saved to NVS");
}

void nvs_save_broker(const char *broker_url)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READWRITE, &nvs));
    esp_err_t err = nvs_set_str(nvs, "mqtt_broker", broker_url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set mqtt_broker: %s", esp_err_to_name(err));
    } else {
        ESP_ERROR_CHECK(nvs_commit(nvs));
        strlcpy(g_mqtt_broker, broker_url, sizeof(g_mqtt_broker));
        ESP_LOGI(TAG, "MQTT broker saved: %s", broker_url);
    }
    nvs_close(nvs);
}

void initialize_system(void)
{
    logic_queue        = xQueueCreate(10, sizeof(logic_queue_item_t));
    display_queue      = xQueueCreate(8,  sizeof(display_msg_t));
    system_event_group = xEventGroupCreate();
    i2c_mutex          = xSemaphoreCreateMutex();

    if (!logic_queue || !display_queue || !system_event_group || !i2c_mutex) {
        ESP_LOGE(TAG, "Failed to create RTOS primitives!");
        abort();
    }
    ESP_LOGI(TAG, "RTOS primitives created");
}