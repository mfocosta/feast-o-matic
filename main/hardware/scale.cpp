#include <HX711.h>

HX711 scale;

/* Initialize the scale */
bool scale_init(void)
{
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(g_cal_factor);
    scale.set_offset(g_raw_offset);   /* use the known empty-scale raw value */

    return true;
}