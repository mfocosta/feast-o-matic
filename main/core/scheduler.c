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

#include "events.h"
#include "init.h"
#include "scheduler.h"

static const char *TAG = "scheduler";

static void sntp_sync(void)
{
    ESP_LOGI(TAG, "Initialising SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    /* Wait up to 30 s for the first sync */
    int retries = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retries < 30) {
        ESP_LOGD(TAG, "Waiting for NTP sync... (%d/30)", retries + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        retries++;
    }

    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET) {
        time_t now;
        struct tm t;
        time(&now);
        localtime_r(&now, &t);
        ESP_LOGI(TAG, "Time synced: %04d-%02d-%02d %02d:%02d",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                 t.tm_hour, t.tm_min);
    } else {
        ESP_LOGW(TAG, "NTP sync timed out – will retry on next wake");
    }
}

void scheduler_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Waiting for WiFi...");
    xEventGroupWaitBits(system_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    sntp_sync();

    /* Track the last encoded minute that triggered a feed (avoids re-firing) */
    int last_triggered = -1;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000)); /* check every 30 s */

        if (g_sched_hour < 0) continue; /* no schedule configured */

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
        int sched   = g_sched_hour * 60 + g_sched_minute;

        if (current == sched && last_triggered != current) {
            ESP_LOGI(TAG, "Scheduled feed at %02d:%02d (%d g)",
                     g_sched_hour, g_sched_minute, g_sched_grams);
            last_triggered = current;

            logic_queue_item_t item = {
                .type       = CMD_FEED_NOW,
                .data.grams = g_sched_grams,
            };
            xQueueSend(logic_queue, &item, pdMS_TO_TICKS(1000));

        } else if (current != sched) {
            /* Reset so the next occurrence can fire */
            last_triggered = -1;
        }
    }
}
