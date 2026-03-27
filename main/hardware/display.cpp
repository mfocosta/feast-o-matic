#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "display.h"
#include "events.h"
#include <bitmaps.h>

static const char *TAG = "display_handler";

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1   /* Not used */

/* ── Pages & overlays ───────────────────────────────────────────────── */

typedef enum {
    PAGE_HOME = 0,
    PAGE_SCHEDULE,
    PAGE_MANUAL_FEED,
    PAGE_INFO,
    PAGE_COUNT,
} display_page_t;

typedef enum {
    OVERLAY_NONE = 0,
    OVERLAY_DISPENSING,
    OVERLAY_OTA,
    OVERLAY_ERROR,
} display_overlay_t;

/* ── Hardware object ────────────────────────────────────────────────── */

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ── Cached sensor / schedule data (written from display_task only) ─── */

static float   s_weight   = 0;
static float   s_temp     = 0;
static float   s_hum      = 0;
static int     s_fill_pct = -1;

static int     s_sched_hour  = -1;
static int     s_sched_min   = 0;
static int     s_sched_grams = 0;

/* ── Page state ─────────────────────────────────────────────────────── */

static display_page_t    s_page         = PAGE_HOME;
static display_overlay_t s_overlay      = OVERLAY_NONE;
static display_page_t    s_saved_page   = PAGE_HOME;  /* restored after overlay */
static TickType_t        s_error_dismiss_at = 0;      /* for auto-dismiss       */
static int               s_dispense_grams  = 0;

/* ── Draw helpers ───────────────────────────────────────────────────── */

static void draw_top_bar(const char *title)
{
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 2);
    display.print(title);

    if (xEventGroupGetBits(system_event_group) & WIFI_CONNECTED_BIT) {
        display.setCursor(91, 2);
        display.print("WiFi");
    }

    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
}

/* ── PAGE_HOME ──────────────────────────────────────────────────────── */

static void draw_home_static(void)
{
    display.clearDisplay();
    draw_top_bar("Feast-O-Matic");

    display.drawFastVLine(64, 13, 39, SSD1306_WHITE);
    display.setCursor(5,  18); display.print("RESERV.");
    display.setCursor(72, 18); display.print("NA TACA");

    display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
    display.display();
}

static void draw_home_data(void)
{
    /* Dynamic regions: clear then redraw */
    display.fillRect(5,  28, 55, 22, SSD1306_BLACK);
    display.fillRect(68, 28, 57, 22, SSD1306_BLACK);
    display.fillRect(0,  53, 128, 9, SSD1306_BLACK);

    display.setTextSize(2);
    display.setCursor(5, 30);
    if (s_fill_pct >= 0) {
        display.print(s_fill_pct); display.setTextSize(1); display.print("%");
    } else {
        display.print("--"); display.setTextSize(1); display.print("%");
    }

    display.setTextSize(2);
    display.setCursor(68, 30);
    display.print((int)s_weight); display.setTextSize(1); display.print("g");

    display.setTextSize(1);
    display.setCursor(1, 54);
    display.print("T:"); display.print(s_temp, 1); display.print("C");
    display.setCursor(68, 54);
    display.print("H:"); display.print((int)s_hum); display.print("%");

    display.display();
}

/* ── PAGE_SCHEDULE ──────────────────────────────────────────────────── */

static void draw_schedule_page(void)
{
    display.clearDisplay();
    draw_top_bar("Proximo horario");

    display.setTextSize(1);
    display.setCursor(0, 18);

    if (s_sched_hour < 0) {
        display.println("Sem horario");
        display.println("configurado.");
    } else {
        display.print("Hora:  ");
        if (s_sched_hour < 10) display.print("0");
        display.print(s_sched_hour); display.print(":");
        if (s_sched_min < 10) display.print("0");
        display.println(s_sched_min);

        display.print("Racao: ");
        display.print(s_sched_grams); display.println("g");
    }

    display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 55); display.print("<");
    display.setCursor(119, 55); display.print(">");
    display.display();
}

/* ── PAGE_MANUAL_FEED ───────────────────────────────────────────────── */

static void draw_manual_feed_page(void)
{
    display.clearDisplay();
    draw_top_bar("Alimentar agora");

    display.setTextSize(1);
    display.setCursor(0, 18);
    display.println("Prima e segure NEXT");
    display.println("para dispensar.");

    display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
    display.setCursor(0,  55); display.print("<");
    display.setCursor(119, 55); display.print(">");
    display.setCursor(40, 55); display.print("[HOLD>]");
    display.display();
}

/* ── PAGE_INFO ──────────────────────────────────────────────────────── */

static void draw_info_page(void)
{
    display.clearDisplay();
    draw_top_bar("Informacao");

    display.setTextSize(1);
    display.setCursor(0, 18);

    /* WiFi status */
    bool wifi_ok = xEventGroupGetBits(system_event_group) & WIFI_CONNECTED_BIT;
    display.print("WiFi: "); display.println(wifi_ok ? "Ligado" : "Deslig.");

    /* Firmware version */
    const esp_app_desc_t *desc = esp_app_get_description();
    display.print("FW:"); display.println(desc->version);

    display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
    display.setCursor(0, 55); display.print("<");
    display.setCursor(119, 55); display.print(">");
    display.display();
}

/* ── OVERLAY_DISPENSING ─────────────────────────────────────────────── */

static void draw_dispensing(int grams)
{
    display.clearDisplay();
    draw_top_bar("Dispensando...");

    display.setTextSize(2);
    display.setCursor(20, 24);
    display.print(grams); display.print("g");

    display.setTextSize(1);
    display.setCursor(10, 48);
    display.print("Aguarde...");
    display.display();
}

/* ── OVERLAY_OTA ────────────────────────────────────────────────────── */

static void draw_ota_progress(int pct)
{
    display.clearDisplay();
    draw_top_bar("Actualizando FW");

    /* Progress bar */
    display.drawRect(4, 24, 120, 12, SSD1306_WHITE);
    int filled = (int)(120 * pct / 100);
    if (filled > 0) display.fillRect(4, 24, filled, 12, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(52, 40);
    display.print(pct); display.print("%");
    display.display();
}

/* ── OVERLAY_ERROR ──────────────────────────────────────────────────── */

static void draw_error(const char *msg)
{
    display.clearDisplay();
    draw_top_bar("ERRO");

    display.setTextSize(1);
    display.setCursor(0, 18);
    display.println(msg);

    display.setCursor(0, 52);
    display.print("(auto-dismiss 5s)");
    display.display();
}

/* ── Routing ────────────────────────────────────────────────────────── */

/* Redraw the current normal page from scratch */
static void draw_current_page(void)
{
    switch (s_page) {
        case PAGE_HOME:
            draw_home_static();
            draw_home_data();
            break;
        case PAGE_SCHEDULE:
            draw_schedule_page();
            break;
        case PAGE_MANUAL_FEED:
            draw_manual_feed_page();
            break;
        case PAGE_INFO:
            draw_info_page();
            break;
        default:
            break;
    }
}

/* ── Init ───────────────────────────────────────────────────────────── */

void display_init(void)
{
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        ESP_LOGW(TAG, "OLED display initialization failed - continuing without display");
        return;
    }
    ESP_LOGI(TAG, "OLED display initialized successfully");
}

/* ── Logo / startup ─────────────────────────────────────────────────── */

void display_show_feast_logo(void)
{
    display.clearDisplay();
    display.drawBitmap(0, 0, feastLogoUpgraded, 128, 64, WHITE);
    display.display();
}

/* ── Display task ───────────────────────────────────────────────────── */

void display_task(void *pvParameter)
{
    display_msg_t msg;

    display_init();

    xSemaphoreTake(i2c_mutex, portMAX_DELAY);
    display_show_feast_logo();
    xSemaphoreGive(i2c_mutex);

    vTaskDelay(pdMS_TO_TICKS(3000));

    xSemaphoreTake(i2c_mutex, portMAX_DELAY);
    draw_home_static();
    xSemaphoreGive(i2c_mutex);

    for (;;) {
        /* Use a short timeout when waiting for error auto-dismiss */
        TickType_t wait = portMAX_DELAY;
        if (s_overlay == OVERLAY_ERROR) {
            TickType_t now = xTaskGetTickCount();
            if ((TickType_t)(s_error_dismiss_at - now) < pdMS_TO_TICKS(5500)) {
                wait = s_error_dismiss_at - now;
                if ((int32_t)wait <= 0) wait = 1;
            }
        }

        BaseType_t got = xQueueReceive(display_queue, &msg, wait);

        /* ── Error auto-dismiss ──────────────────────────────────────── */
        if (got != pdTRUE) {
            if (s_overlay == OVERLAY_ERROR &&
                (int32_t)(xTaskGetTickCount() - s_error_dismiss_at) >= 0) {
                s_overlay = OVERLAY_NONE;
                s_page    = s_saved_page;
                xSemaphoreTake(i2c_mutex, portMAX_DELAY);
                draw_current_page();
                xSemaphoreGive(i2c_mutex);
            }
            continue;
        }

        xSemaphoreTake(i2c_mutex, portMAX_DELAY);

        switch (msg.type) {

        /* ── Status update (sensor task) ────────────────────────────── */
        case DISPLAY_MSG_STATUS:
            s_weight   = msg.data.status.weight;
            s_temp     = msg.data.status.temp;
            s_hum      = msg.data.status.hum;
            s_fill_pct = msg.data.status.fill_pct;
            /* Only refresh home data when home is visible */
            if (s_overlay == OVERLAY_NONE && s_page == PAGE_HOME) {
                draw_home_data();
            }
            break;

        /* ── Schedule hint (scheduler task) ────────────────────────── */
        case DISPLAY_MSG_SCHEDULE_HINT:
            s_sched_hour  = msg.data.schedule_hint.hour;
            s_sched_min   = msg.data.schedule_hint.minute;
            s_sched_grams = msg.data.schedule_hint.grams;
            if (s_overlay == OVERLAY_NONE && s_page == PAGE_SCHEDULE) {
                draw_schedule_page();
            }
            break;

        /* ── Navigation (button task) ───────────────────────────────── */
        case DISPLAY_MSG_NAV: {
            /* Ignore navigation while an overlay is active (except to
             * dismiss a finished OTA screen via any button press)      */
            if (s_overlay != OVERLAY_NONE) break;

            int8_t dir     = msg.data.nav.dir;
            bool   confirm = msg.data.nav.confirm;

            if (confirm) {
                /* Long NEXT: confirm action on current page */
                if (s_page == PAGE_MANUAL_FEED) {
                    logic_queue_item_t cmd = { .type = CMD_FEED_NOW };
                    cmd.data.grams = s_sched_grams > 0 ? s_sched_grams : 50;
                    xQueueSend(logic_queue, &cmd, 0);
                }
            } else if (dir == 0) {
                /* Long PREV: go to HOME */
                s_page = PAGE_HOME;
                draw_current_page();
            } else {
                /* Short press: cycle through pages */
                s_page = (display_page_t)
                    ((s_page + dir + PAGE_COUNT) % PAGE_COUNT);
                draw_current_page();
            }
            break;
        }

        /* ── Dispensing started (feeder task) ───────────────────────── */
        case DISPLAY_MSG_DISPENSING:
            if (s_overlay == OVERLAY_NONE) {
                s_saved_page = s_page;
            }
            s_overlay      = OVERLAY_DISPENSING;
            s_dispense_grams = msg.data.dispensing.grams;
            draw_dispensing(s_dispense_grams);
            break;

        /* ── Dispensing finished (feeder task) ──────────────────────── */
        case DISPLAY_MSG_DISPENSING_DONE:
            if (s_overlay == OVERLAY_DISPENSING) {
                s_overlay = OVERLAY_NONE;
                s_page    = s_saved_page;
                draw_current_page();
            }
            break;

        /* ── OTA progress (ota task) ────────────────────────────────── */
        case DISPLAY_MSG_OTA_PROGRESS:
            if (s_overlay == OVERLAY_NONE || s_overlay == OVERLAY_OTA) {
                if (s_overlay == OVERLAY_NONE) s_saved_page = s_page;
                s_overlay = OVERLAY_OTA;
            }
            draw_ota_progress(msg.data.ota.pct);
            break;

        /* ── Error (any task) ───────────────────────────────────────── */
        case DISPLAY_MSG_ERROR:
            if (s_overlay == OVERLAY_NONE) s_saved_page = s_page;
            s_overlay         = OVERLAY_ERROR;
            s_error_dismiss_at = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
            draw_error(msg.data.error);
            break;

        case DISPLAY_MSG_LOGO:
            display_show_feast_logo();
            break;

        default:
            break;
        }

        xSemaphoreGive(i2c_mutex);
    }
}

/* ── Post helpers (non-blocking, safe to call from any task) ─────────── */

void display_post_status(float weight, float temp, float hum, int fill_pct)
{
    display_msg_t msg = { .type = DISPLAY_MSG_STATUS };
    msg.data.status.weight   = weight;
    msg.data.status.temp     = temp;
    msg.data.status.hum      = hum;
    msg.data.status.fill_pct = fill_pct;
    xQueueSend(display_queue, &msg, 0);
}

void display_post_dispensing(int target_grams)
{
    display_msg_t msg = { .type = DISPLAY_MSG_DISPENSING };
    msg.data.dispensing.grams = target_grams;
    xQueueSendToFront(display_queue, &msg, 0);
}

void display_post_dispensing_done(void)
{
    display_msg_t msg = { .type = DISPLAY_MSG_DISPENSING_DONE };
    xQueueSend(display_queue, &msg, 0);
}

void display_post_error(const char *message)
{
    display_msg_t msg = { .type = DISPLAY_MSG_ERROR };
    strncpy(msg.data.error, message, sizeof(msg.data.error) - 1);
    msg.data.error[sizeof(msg.data.error) - 1] = '\0';
    xQueueSendToFront(display_queue, &msg, 0);
}

void display_post_ota_progress(int pct)
{
    display_msg_t msg = { .type = DISPLAY_MSG_OTA_PROGRESS };
    msg.data.ota.pct = pct;
    xQueueSend(display_queue, &msg, 0);
}

void display_post_nav(int8_t dir, bool confirm)
{
    display_msg_t msg = { .type = DISPLAY_MSG_NAV };
    msg.data.nav.dir     = dir;
    msg.data.nav.confirm = confirm;
    xQueueSend(display_queue, &msg, 0);
}

void display_post_schedule_hint(int hour, int minute, int grams)
{
    display_msg_t msg = { .type = DISPLAY_MSG_SCHEDULE_HINT };
    msg.data.schedule_hint.hour   = hour;
    msg.data.schedule_hint.minute = minute;
    msg.data.schedule_hint.grams  = grams;
    xQueueSend(display_queue, &msg, 0);
}

void display_post_startup(void)
{
    display_msg_t msg = { .type = DISPLAY_MSG_STARTUP };
    xQueueSend(display_queue, &msg, 0);
}

void display_post_logo(void)
{
    display_msg_t msg = { .type = DISPLAY_MSG_LOGO };
    xQueueSend(display_queue, &msg, 0);
}

