#ifndef PINS_H
#define PINS_H

#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DHT11 */
#define DHTPIN 4

/* LED */
#define LEDPIN 5

/* HX711 Load Cell */
#define LOADCELL_DOUT_PIN 25
#define LOADCELL_SCK_PIN  26

/* ULN2003 Stepper Motor Driver */
#define IN1 14
#define IN2 12
#define IN3 13
#define IN4 33


#ifdef __cplusplus
}
#endif

#endif // PINS_H