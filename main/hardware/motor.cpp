#include "Arduino.h"
#include <Stepper.h>
#include <stdio.h>
#include "config/pins.h"

/* Initialize stepper motor */
Stepper stepper(CONFIG_28BYJ48_STEPS_PER_REV, STEPPER_IN1, STEPPER_IN3, STEPPER_IN2, STEPPER_IN4);

/*
 * Disable the stepper motor by setting all control pins to LOW.
 */
void disableMotor(void) {
    digitalWrite(STEPPER_IN1, LOW);
    digitalWrite(STEPPER_IN2, LOW);
    digitalWrite(STEPPER_IN3, LOW);
    digitalWrite(STEPPER_IN4, LOW);
}