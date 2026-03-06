#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set to true while a dispensing cycle is running (read by sensor_task) */
extern volatile bool is_feeding;

void feeder_task(void *pvParameter);

#ifdef __cplusplus
}
#endif
