#include <HX711.h>

HX711 scale;

float target_weight   = 0;   /* Fetched later from NVS (grams) */ 
float current_weight  = 0;         
float previous_weight = 0;      