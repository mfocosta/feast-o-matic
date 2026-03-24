#ifndef TOF_HANDLER_H
#define TOF_HANDLER_H

#include "Adafruit_VL53L1X.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern Adafruit_VL53L1X vl53;

void vl53_init(void);
bool vl53_read(int16_t *distance_mm);

/**
 * Convert a raw distance reading to a fill level.
 *
 *   0 mm  → 100 %  (full)
 *   100 mm → 0 %   (empty)
 *
 * The result is clamped to [0, 100]
 */
static inline int tof_fill_percent(int16_t distance_mm)
{
    /* Invert: closer to sensor = fuller */
    int pct = 100 * (CONFIG_TOF_EMPTY_MM - distance_mm) / (CONFIG_TOF_EMPTY_MM - CONFIG_TOF_FULL_MM);

    /* Clamp */
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    return pct;
}

#ifdef __cplusplus
}
#endif

#endif // TOF_HANDLER_H