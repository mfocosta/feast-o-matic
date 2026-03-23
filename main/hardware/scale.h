#ifndef SCALE_H
#define SCALE_H

#include "HX711.h"

#ifdef __cplusplus
extern "C" {
#endif

extern HX711 scale;

bool scale_init(void);

#ifdef __cplusplus
}
#endif

#endif // SCALE_H