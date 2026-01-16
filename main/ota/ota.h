#ifndef OTA_H
#define OTA_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

extern SemaphoreHandle_t ota_semaphore;

void ota_task(void *pvParameter);

#ifdef __cplusplus
}
#endif

#endif // OTA_H