#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_SSD1306.h>
#include <stdbool.h>
#include "events.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Adafruit_SSD1306 display;

/* Initialise hardware */
void display_init(void);

/* FreeRTOS task – processes display_queue messages */
void display_task(void *pvParameter);

/* Non-blocking post helpers (send to display_queue from any task) */
void display_post_status(float weight, float temp, float hum);
void display_post_dispensing(int target_grams);
void display_post_error(const char *message);
void display_post_startup(void);
void display_post_logo(void);

/* Low-level draw functions (called only from display_task) */


#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H