#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "events.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise hardware */
void display_init(void);

/* FreeRTOS task – processes display_queue messages */
void display_task(void *pvParameter);

/* Non-blocking post helpers (send to display_queue from any task) */
void display_post_status(float weight, float temp, float hum, int fill_pct);
void display_post_dispensing(int target_grams);
void display_post_dispensing_done(void);
void display_post_error(const char *message);
void display_post_ota_progress(int pct, const char *old_ver, const char *new_ver);
void display_post_nav(int8_t dir, bool confirm);  /* called by button_task */
void display_post_schedule_hint(int hour, int minute, int grams);
void display_post_startup(void);
void display_post_logo(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H