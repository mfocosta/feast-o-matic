/* MQTT handler
 *
 * Subscribes to "feeder/command" and publishes status on "feeder/status".
 *
 * Command protocol (plain text, no JSON needed):
 *   feed:<GRAMS>                     e.g. "feed:100"
 *   schedule:<HOUR>:<MINUTE>:<GRAMS> e.g. "schedule:8:0:100"
 *   bowl:<WEIGHT>                    e.g. "bowl:150.0" (write bowl tare to NFC tag)
 *   ota                              triggers an immediate OTA check
 *
 * The broker URL is read from g_mqtt_broker (init.h), which is loaded from
 * NVS on boot (default from CONFIG_MQTT_BROKER_URL).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "events.h"
#include "init.h"
#include "ota.h"

static const char *TAG = "mqtt";

#define TOPIC_CMD    "feeder/command"
#define TOPIC_STATUS "feeder/status"

esp_mqtt_client_handle_t mqtt_client = NULL;

static void handle_command(const char *data, int data_len)
{
    char buf[64];
    int len = data_len < (int)sizeof(buf) - 1 ? data_len : (int)sizeof(buf) - 1;
    memcpy(buf, data, len);
    buf[len] = '\0';

    logic_queue_item_t item = {0};
    int grams, hour, minute;
    float bowl_w;

    if (sscanf(buf, "feed:%d", &grams) == 1) {
        item.type       = CMD_FEED_NOW;
        item.data.grams = grams;
        ESP_LOGI(TAG, "CMD_FEED_NOW: %d g", grams);
        xQueueSend(logic_queue, &item, pdMS_TO_TICKS(500));

    } else if (sscanf(buf, "schedule:%d:%d:%d", &hour, &minute, &grams) == 3) {
        item.type                  = CMD_UPDATE_SCHEDULE;
        item.data.schedule.hour   = hour;
        item.data.schedule.minute = minute;
        item.data.schedule.grams  = grams;
        ESP_LOGI(TAG, "CMD_UPDATE_SCHEDULE: %02d:%02d, %d g", hour, minute, grams);
        xQueueSend(logic_queue, &item, pdMS_TO_TICKS(500));

    } else if (sscanf(buf, "bowl:%f", &bowl_w) == 1) {
        item.type             = CMD_WRITE_BOWL_TAG;
        item.data.bowl_weight = bowl_w;
        ESP_LOGI(TAG, "CMD_WRITE_BOWL_TAG: %.1f g", bowl_w);
        xQueueSend(logic_queue, &item, pdMS_TO_TICKS(500));

    } else if (strcmp(buf, "ota") == 0) {
        ESP_LOGI(TAG, "CMD_OTA_CHECK");
        ota_trigger_check();

    } else {
        ESP_LOGW(TAG, "Unknown command: %s", buf);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t  event  = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        xEventGroupSetBits(system_event_group, MQTT_CONNECTED_BIT);
        esp_mqtt_client_subscribe(client, TOPIC_CMD, 1);
        esp_mqtt_client_publish(client, TOPIC_STATUS, "online", 0, 0, 1 /* retain */);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected from broker");
        xEventGroupClearBits(system_event_group, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_DATA:
        handle_command(event->data, event->data_len);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error type: %d", event->error_handle->error_type);
        break;

    default:
        break;
    }
}

void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t cfg = {
        .broker = {
            .address.uri = g_mqtt_broker,
        },
    };
    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "MQTT client started, broker: %s", g_mqtt_broker);
}