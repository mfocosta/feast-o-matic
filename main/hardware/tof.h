#ifndef TOF_HANDLER_H
#define TOF_HANDLER_H

#include "Adafruit_VL53L1X.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Physical calibration points ------------------------------------------------
 * TOF_FULL_MM  : sensor reading when the container is completely full  (0 mm)
 * TOF_EMPTY_MM : sensor reading when the container is completely empty (100 mm)
 * --------------------------------------------------------------------------- */
#define TOF_FULL_MM   0
#define TOF_EMPTY_MM  100

extern Adafruit_VL53L1X vl53;

void vl53_init(void);
bool vl53_read(int16_t *distance_mm);

/**
 * Convert a raw distance reading to a fill level quantised to 5 % steps.
 *
 *   0 mm  → 100 %  (full)
 *   100 mm → 0 %   (empty)
 *
 * The result is clamped to [0, 100] and rounded to the nearest multiple of 5.
 */
static inline int tof_fill_percent(int16_t distance_mm)
{
    /* Invert: closer to sensor = fuller */
    int pct = 100 * (TOF_EMPTY_MM - distance_mm) / (TOF_EMPTY_MM - TOF_FULL_MM);

    /* Clamp */
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    /* Quantise to nearest 5 % step */
    return ((pct + 2) / 5) * 5;
}

#ifdef __cplusplus
}
#endif

#endif // TOF_HANDLER_H