#ifndef PINS_H
#define PINS_H

#ifdef __cplusplus
extern "C" {
#endif

/* LED */
#define LEDPIN  GPIO_NUM_5


#define GPIO_OUTPUT_PIN_SEL  (1ULL<<LEDPIN)

#define GPIO_INPUT_PIN_SEL   0

/* 
 * Initialized by the respective drivers/libraries
 */
/* DHT11 */
#define DHTPIN  GPIO_NUM_4

/* HX711 Load Cell */
#define LOADCELL_DOUT_PIN  GPIO_NUM_25
#define LOADCELL_SCK_PIN   GPIO_NUM_26

/* ULN2003 Stepper Motor Driver */
#define IN1  GPIO_NUM_14
#define IN2  GPIO_NUM_12
#define IN3  GPIO_NUM_13
#define IN4  GPIO_NUM_33


#ifdef __cplusplus
}
#endif

#endif // PINS_H