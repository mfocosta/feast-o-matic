#include "Arduino.h"
#include <Stepper.h>
#include <stdio.h>
#include "config/pins.h"

/* Initialize stepper motor */
Stepper stepper(CONFIG_28BYJ48_STEPS_PER_REV, IN1, IN3, IN2, IN4);

/*
 * Disable the stepper motor by setting all control pins to LOW.
 */
void disableMotor() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
}