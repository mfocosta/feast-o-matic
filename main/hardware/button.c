/* button.c – Short/long press detection for BTN_UP and BTN_DOWN
 *
 * ISR fires on any edge (press = falling, release = rising).
 * Raw events are posted to a small button_event_queue.
 * button_task() drains that queue and measures press duration,
 * then posts DISPLAY_MSG_NAV to display_queue (or CMD_FEED_NOW to
 * logic_queue on a long NEXT press from the Manual Feed page).
 *
 * Button semantics
 *   BTN_UP  short  → next page
 *   BTN_UP  long   → confirm action (trigger manual feed)
 *   BTN_DOWN  short  → previous page
 *   BTN_DOWN  long   → return to HOME from any page
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "button.h"
#include "display.h"
#include "events.h"
#include "pins.h"

static const char *TAG = "button";

/* Raw event posted from ISR to button_task */
typedef struct {
    gpio_num_t gpio;
    bool       pressed;   /* true = falling edge (button down) */
    TickType_t tick;
} btn_raw_event_t;

static QueueHandle_t s_btn_queue;

/* ── ISR ────────────────────────────────────────────────────────────── */

static void IRAM_ATTR button_isr_handler(void *arg)
{
    btn_raw_event_t ev;
    ev.gpio    = (gpio_num_t)(uintptr_t)arg;
    ev.pressed = (gpio_get_level(ev.gpio) == 0); /* active-low with pull-up */
    ev.tick    = xTaskGetTickCountFromISR();

    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_btn_queue, &ev, &woken);
    if (woken) portYIELD_FROM_ISR();
}

/* ── Init ────────────────────────────────────────────────────────────── */

void button_init(void)
{
    s_btn_queue = xQueueCreate(8, sizeof(btn_raw_event_t));

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_UP, button_isr_handler, (void *)(uintptr_t)BTN_UP);
    gpio_isr_handler_add(BTN_DOWN, button_isr_handler, (void *)(uintptr_t)BTN_DOWN);

    ESP_LOGI(TAG, "Button ISRs installed (NEXT=GPIO%d, PREV=GPIO%d)", BTN_UP, BTN_DOWN);
}

/* ── Task ────────────────────────────────────────────────────────────── */

void button_task(void *pvParameter)
{
    btn_raw_event_t ev;

    /* Per-button state: when did the press start? */
    TickType_t press_start[2] = {0, 0};
    bool       pressing[2]    = {false, false};

    for (;;) {
        if (xQueueReceive(s_btn_queue, &ev, portMAX_DELAY) != pdTRUE) continue;

        int idx = (ev.gpio == BTN_UP) ? 0 : 1;

        if (ev.pressed) {
            /* Falling edge – button pressed down */
            pressing[idx]    = true;
            press_start[idx] = ev.tick;

        } else if (pressing[idx]) {
            /* Rising edge – button released */
            pressing[idx] = false;

            uint32_t duration_ms =
                (uint32_t)((ev.tick - press_start[idx]) * portTICK_PERIOD_MS);

            /* Discard glitches shorter than the debounce window */
            if (duration_ms < CONFIG_DEBOUNCE_MS) continue;

            bool long_press = (duration_ms >= CONFIG_LONG_PRESS_MS);

            ESP_LOGI(TAG, "GPIO%d released after %"PRIu32" ms (%s)",
                     ev.gpio, duration_ms, long_press ? "LONG" : "short");

            if (idx == 0) {
                /* BTN_UP */
                if (long_press) {
                    /* Long NEXT = confirm action on current page */
                    display_post_nav(0, true);
                } else {
                    /* Short NEXT = advance to next page */
                    display_post_nav(+1, false);
                }
            } else {
                /* BTN_DOWN */
                if (long_press) {
                    /* Long PREV = return to HOME */
                    display_post_nav(0, false);
                } else {
                    /* Short PREV = go back one page */
                    display_post_nav(-1, false);
                }
            }
        }
    }
}
