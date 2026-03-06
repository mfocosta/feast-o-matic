#ifndef SCALE_H
#define SCALE_H

#include "HX711.h"

#ifdef __cplusplus
extern "C" {
#endif

extern HX711 scale;
extern float target_weight;
extern float current_weight;
extern float previous_weight;

#ifdef __cplusplus
}
#endif

#endif // SCALE_H