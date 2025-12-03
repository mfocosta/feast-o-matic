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
#include "esp_netif.h"
#include "string.h"
/* Include certificate bundle to verify GitHub HTTPS server */
#include "esp_crt_bundle.h"

#include <sys/socket.h>
#include <inttypes.h>

static const char *TAG = "ota_handler";
SemaphoreHandle_t ota_semaphore = NULL;

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
    // Create semaphore if not already created
    if (ota_semaphore == NULL) {
        ota_semaphore = xSemaphoreCreateBinary();
        if (ota_semaphore == NULL) {
            ESP_LOGE(TAG, "Failed to create OTA semaphore");
            vTaskDelete(NULL);
            return;
        }
        xSemaphoreGive(ota_semaphore);
    }

    while (1) {
        // Wait a bit before checking for updates (5 minutes)
        vTaskDelay(300000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Starting OTA task");

        // Check if we have an IP address (indicates WiFi is connected)
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        
        if (netif == NULL || esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || 
            ip_info.ip.addr == 0) {
            ESP_LOGW(TAG, "WiFi not connected, skipping OTA check");
            continue;
        }
        
        ESP_LOGI(TAG, "WiFi is connected, checking for updates");
        
        // Signal main loop that OTA is starting
        xSemaphoreTake(ota_semaphore, portMAX_DELAY);

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

        esp_https_ota_handle_t https_ota_handle = NULL;
        esp_err_t ret = esp_https_ota_begin(&ota_config, &https_ota_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(ret));
            xSemaphoreGive(ota_semaphore);
            vTaskDelay(300000 / portTICK_PERIOD_MS);
            continue;
        }

        // Give semaphore back to main loop during download
        xSemaphoreGive(ota_semaphore);

        // Get the image description to check version before downloading
        esp_app_desc_t new_app_info = {};
        ret = esp_https_ota_get_img_desc(https_ota_handle, &new_app_info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed: %s", esp_err_to_name(ret));
            esp_https_ota_abort(https_ota_handle);
            xSemaphoreGive(ota_semaphore);
            vTaskDelay(300000 / portTICK_PERIOD_MS);
            continue;
        }

        // Get running app info for version comparison
        const esp_partition_t *running_partition = esp_ota_get_running_partition();
        esp_app_desc_t running_app_info = {};
        ret = esp_ota_get_partition_description(running_partition, &running_app_info);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read running app info, proceeding with OTA");
        } else {
            ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
            ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

            // Compare versions - skip update if same version
            if (memcmp(new_app_info.version, running_app_info.version, 
                       sizeof(new_app_info.version)) == 0) {
                ESP_LOGI(TAG, "Firmware version is the same, skipping OTA update");
                esp_https_ota_abort(https_ota_handle);
                xSemaphoreGive(ota_semaphore);
                vTaskDelay(300000 / portTICK_PERIOD_MS);
                continue;
            }
            
            // Before performing OTA, take semaphore to pause main loop
            xSemaphoreTake(ota_semaphore, portMAX_DELAY);
        }

        // Perform the OTA download and write
        while (1) {
            ret = esp_https_ota_perform(https_ota_handle);
            if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                break;
            }
        }

        if (ret == ESP_OK) {
            ret = esp_https_ota_finish(https_ota_handle);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "OTA Succeeded, Rebooting...");
                xSemaphoreGive(ota_semaphore);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
            } else {
                ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(ret));
                xSemaphoreGive(ota_semaphore);
            }
        } else {
            ESP_LOGE(TAG, "Firmware upgrade failed: %s", esp_err_to_name(ret));
            esp_https_ota_abort(https_ota_handle);
            xSemaphoreGive(ota_semaphore);
        }

        vTaskDelay(300000 / portTICK_PERIOD_MS);
    }
}