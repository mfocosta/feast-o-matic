#ifndef TOF_HANDLER_H
#define TOF_HANDLER_H

#include "Adafruit_VL53L1X.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Adafruit_VL53L1X vl53;

void vl53_init(void);
bool vl53_read(int16_t *distance_mm);

#ifdef __cplusplus
}
#endif

#endif // TOF_HANDLER_H