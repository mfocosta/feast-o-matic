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
#define DISPENSE_STEPS      256
/* Safety cap: stop after this many cycles even if target not reached */
#define MAX_DISPENSE_CYCLES  40

void feeder_task(void *pvParameter)
{
    logic_queue_item_t item;

    for (;;) {
        if (xQueueReceive(logic_queue, &item, portMAX_DELAY) != pdTRUE) continue;

        switch (item.type) {

        case CMD_FEED_NOW: {
            if (xEventGroupGetBits(system_event_group) & OTA_IN_PROGRESS_BIT) {
                ESP_LOGW(TAG, "OTA in progress, ignoring feed command");
                break;
            }

            int target_grams = item.data.grams;
            ESP_LOGI(TAG, "Feed command: %d g", target_grams);

            display_post_dispensing(target_grams);

            /* Tare with the empty bowl already in place */
            gpio_set_level(LEDPIN, 1);

            float w = 0.0f;
            int cycles = 0;
            while (w < (float)target_grams && cycles < MAX_DISPENSE_CYCLES) {
                stepper.step(-DISPENSE_STEPS);
                vTaskDelay(pdMS_TO_TICKS(300)); /* let food settle before weighing */
                w = scale.get_units();
                ESP_LOGI(TAG, "  cycle %d: %.1f / %d g", cycles + 1, w, target_grams);
                cycles++;
            }

            disableMotor();
            gpio_set_level(LEDPIN, 0);

            if (cycles >= MAX_DISPENSE_CYCLES) {
                ESP_LOGW(TAG, "Safety cap reached. Final weight: %.1f g", w);
            } else {
                ESP_LOGI(TAG, "Target reached in %d cycles. Final: %.1f g", cycles, w);
            }

            /* Update reservoir status */
            xEventGroupSetBits(system_event_group, RESERVOIR_UPDATE_BIT);

            break;
        }

        case CMD_UPDATE_SCHEDULE: {
            g_sched_hour   = item.data.schedule.hour;
            g_sched_minute = item.data.schedule.minute;
            g_sched_grams  = item.data.schedule.grams;
            nvs_save_settings();
            ESP_LOGI(TAG, "Schedule updated: %02d:%02d, %d g",
                     g_sched_hour, g_sched_minute, g_sched_grams);
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
