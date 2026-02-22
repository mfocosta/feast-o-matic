#ifndef OTA_H
#define OTA_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

extern TaskHandle_t ota_task_handle;

/* Trigger an immediate OTA check from any context */
void ota_trigger_check(void);

void ota_task(void *pvParameter);

#ifdef __cplusplus
}
#endif

#endif // OTA_H