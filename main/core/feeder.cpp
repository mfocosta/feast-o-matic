/* feeder_task
 *
 * Blocks on logic_queue waiting for commands.
 *
 * CMD_FEED_NOW      – tare scale, drive auger in increments until the bowl
 *                     reaches the requested weight or the safety cap fires.
 * CMD_UPDATE_SCHEDULE – update globals and persist to NVS.
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "events.h"
#include "feeder.h"
#include "init.h"
#include "scale.h"
#include "motor.h"
#include "display.h"
#include "pins.h"
#include "nfc.h"

static const char *TAG = "feeder";

/* Steps dispensed per auger cycle – tune for your specific auger */
#define DISPENSE_STEPS          -512
#define DISPENSE_STEPS_REVERSE   100
/* Safety cap: stop after this many cycles even if target not reached */
#define MAX_DISPENSE_CYCLES  40
/* Minimum time between consecutive feed events (guards against MQTT flooding) */
#define MIN_FEED_INTERVAL_MS  (CONFIG_MIN_FEED_INTERVAL_MIN * 60 * 1000)

void feeder_task(void *pvParameter)
{
    logic_queue_item_t item;

    stepper.setSpeed(10);

    for (;;) {
        if (xQueueReceive(logic_queue, &item, portMAX_DELAY) != pdTRUE) continue;

        switch (item.type) {

        case CMD_FEED_NOW: {
            if (xEventGroupGetBits(system_event_group) & OTA_IN_PROGRESS_BIT) {
                ESP_LOGW(TAG, "OTA in progress, ignoring feed command");
                break;
            }

            static TickType_t last_feed_ticks = 0;
#if CONFIG_MIN_FEED_INTERVAL_MIN > 0
            TickType_t now = xTaskGetTickCount();
            if (last_feed_ticks != 0 &&
                (now - last_feed_ticks) < pdMS_TO_TICKS(MIN_FEED_INTERVAL_MS)) {
                ESP_LOGW(TAG, "Feed too soon, ignoring (min interval %d min)",
                         CONFIG_MIN_FEED_INTERVAL_MIN);
                break;
            }
#endif

            int target_grams = item.data.grams;
            ESP_LOGI(TAG, "Feed command: %d g", target_grams);

            display_post_dispensing(target_grams);

            /* Turn on LED during dispensing */
            gpio_set_level(LEDPIN, 1);

            float weight = 0.0f;
            int cycles = 0;
            while (weight < (float)target_grams && cycles < MAX_DISPENSE_CYCLES) {
                stepper.step(DISPENSE_STEPS);
                stepper.step(DISPENSE_STEPS_REVERSE); /* helps prevent jamming */
                vTaskDelay(pdMS_TO_TICKS(300)); /* let food settle before weighing */
                xSemaphoreTake(i2c_mutex, portMAX_DELAY);
                weight = scale.get_units();
                xSemaphoreGive(i2c_mutex);
                ESP_LOGI(TAG, "  cycle %d: %.1f / %d g", cycles + 1, weight, target_grams);
                cycles++;
            }

            disableMotor();
            gpio_set_level(LEDPIN, 0);
            display_post_dispensing_done();

            if (cycles >= MAX_DISPENSE_CYCLES) {
                ESP_LOGW(TAG, "Safety cap reached. Final weight: %.1f g", weight);
            } else {
                ESP_LOGI(TAG, "Target reached in %d cycles. Final: %.1f g", cycles, weight);
            }

            /* Update reservoir status */
            xEventGroupSetBits(system_event_group, RESERVOIR_UPDATE_BIT);
            last_feed_ticks = xTaskGetTickCount();
            break;
        }

        case CMD_UPDATE_SCHEDULE: {
            int slot = item.data.schedule.slot;
            if (slot < 0 || slot >= CONFIG_SCHED_MAX) {
                ESP_LOGW(TAG, "Invalid schedule slot: %d", slot);
                break;
            }
            g_sched[slot].hour   = item.data.schedule.hour;
            g_sched[slot].minute = item.data.schedule.minute;
            g_sched[slot].grams  = item.data.schedule.grams;
            nvs_save_settings();
            ESP_LOGI(TAG, "Schedule[%d] updated: %02d:%02d, %d g",
                     slot, g_sched[slot].hour, g_sched[slot].minute, g_sched[slot].grams);
            break;
        }

        case CMD_WRITE_BOWL_TAG: {
            xSemaphoreTake(i2c_mutex, portMAX_DELAY);
            bool write_ok = nfc_write_bowl_weight(item.data.bowl_weight);
            xSemaphoreGive(i2c_mutex);
            if (write_ok)
                ESP_LOGI(TAG, "Bowl tag written: %.1f g", item.data.bowl_weight);
            else
                ESP_LOGW(TAG, "Bowl tag write failed (no tag?)");
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown command type: %d", item.type);
            break;
        }
    }
}
