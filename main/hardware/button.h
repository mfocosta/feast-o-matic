#ifndef BUTTON_H
#define BUTTON_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Installs the GPIO ISR service and hooks both button GPIOs */
void button_init(void);

/* FreeRTOS task – decodes short/long presses and posts to display_queue */
void button_task(void *pvParameter);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
