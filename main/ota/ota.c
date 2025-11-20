/* OTA handler

    This file implements the OTA functionality using ESP-IDF libraries.
    It connects to a specified URL, downloads the firmware in chunks,
    and updates the device firmware.

*/
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "string.h"
/* Include certificate bundle to verify GitHub HTTPS server */
#include "esp_crt_bundle.h"

#include <sys/socket.h>

#define HASH_LEN 32

static const char *TAG = "ota_handler";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[]   asm("_binary_ca_cert_pem_end");

#define OTA_URL_SIZE 256

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

void ota_task(void *pvParameter)
{
    while (1) {

        ESP_LOGI(TAG, "Starting OTA task");

        esp_http_client_config_t config = {
            .buffer_size       = 2048,
            .buffer_size_tx    = 1024,
            .timeout_ms        = 600000, /* max 10 minutes for firmware download */
            .url               = CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .event_handler     = _http_event_handler,
            .keep_alive_enable = true,
        };

        esp_https_ota_config_t ota_config = {
            .http_config           = &config,
            .http_client_init_cb   = NULL,
            .partial_http_download = true,  // Enable streaming
            .max_http_request_size = 2048,  // Chunk size (2 KB)
        };
        ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
        esp_err_t ret = esp_https_ota(&ota_config);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA Succeeded, Rebooting...");
            esp_restart();
        } else {
            ESP_LOGE(TAG, "Firmware upgrade failed");
        }

        vTaskDelay(300000 / portTICK_PERIOD_MS);
    }
}