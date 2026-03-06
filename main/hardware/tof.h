#ifndef TOF_HANDLER_H
#define TOF_HANDLER_H

#include "Adafruit_VL53L1X.h"

#ifdef __cplusplus
extern "C" {
#endif

Adafruit_VL53L1X vl53;
extern void vl53_init(void);

#ifdef __cplusplus
}
#endif

#endif // TOF_HANDLER_H