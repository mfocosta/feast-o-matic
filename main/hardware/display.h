#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_SSD1306.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern Adafruit_SSD1306 display;

/* Display functions */
void display_init(void);
void display_show_startup(void);
void display_show_feast_logo(void);
void display_show_weight(float weight);
void display_show_dht(float temp, float humidity);
void display_show_error(const char* message);
/* Shows "Dispensando..." + target weight – clears and flushes internally */
void display_show_dispensing(int target_grams);
/* Shows weight + temp/humidity on one screen – clears and flushes internally */
void display_show_status(float weight, float temp, float humidity);
void display_clear(void);
void display_update(void);


#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H