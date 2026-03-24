/* scheduler_task
 *
 * 1. Waits for WIFI_CONNECTED_BIT in system_event_group.
 * 2. Syncs time via SNTP (pool.ntp.org).
 * 3. Wakes every 30 s and checks whether the current wall-clock minute
 *    matches the configured schedule.  If it does, enqueues CMD_FEED_NOW.
 *
 * The schedule is stored in g_sched_hour / g_sched_minute / g_sched_grams
 * (init.h).  Set g_sched_hour to -1 to disable scheduling.
 */

#include <time.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif_sntp.h"

#include "events.h"
#include "init.h"
#include "scheduler.h"

static const char *TAG = "scheduler";

static void sntp_sync(void)
{
    ESP_LOGI(TAG, "Initialising SNTP...");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.smooth_sync = true;
    esp_netif_sntp_init(&config);

    /* Wait up to 30 s for the first sync */
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    time(&now);

    char strftime_buf[64];

    setenv("TZ", "WET0WEST,M3.5.0/1,M10.5.0", 1);
    tzset();

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Lisbon is: %s", strftime_buf);

    ESP_LOGI(TAG, "Time synced: %04d-%02d-%02d %02d:%02d",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min);
}

void scheduler_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Waiting for WiFi...");
    xEventGroupWaitBits(system_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    sntp_sync();

    /* Track the last encoded minute that triggered a feed, per slot */
    int last_triggered[CONFIG_SCHED_MAX];
    for (int i = 0; i < CONFIG_SCHED_MAX; i++) last_triggered[i] = -1;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000)); /* check every 30 s */

        /* Verify time is valid (NTP synced ≈ year > 2020) */
        time_t now;
        struct tm t;
        time(&now);
        localtime_r(&now, &t);
        if (t.tm_year + 1900 < 2020) {
            /* Clock not synced yet – try again */
            sntp_sync();
            continue;
        }

        int current = t.tm_hour * 60 + t.tm_min;

        for (int i = 0; i < CONFIG_SCHED_MAX; i++) {
            if (g_sched[i].hour < 0) continue; /* slot disabled */

            int sched = g_sched[i].hour * 60 + g_sched[i].minute;

            if (current == sched && last_triggered[i] != current) {
                if (xEventGroupGetBits(system_event_group) & OTA_IN_PROGRESS_BIT) {
                    ESP_LOGW(TAG, "OTA in progress, deferring scheduled feed");
                    continue;
                }

                ESP_LOGI(TAG, "Scheduled feed [slot %d] at %02d:%02d (%d g)",
                         i, g_sched[i].hour, g_sched[i].minute, g_sched[i].grams);
                last_triggered[i] = current;

                logic_queue_item_t item = {
                    .type       = CMD_FEED_NOW,
                    .data.grams = g_sched[i].grams,
                };
                xQueueSend(logic_queue, &item, pdMS_TO_TICKS(1000));

            } else if (current != sched) {
                /* Reset so the next occurrence can fire */
                last_triggered[i] = -1;
            }
        }
    }
}
