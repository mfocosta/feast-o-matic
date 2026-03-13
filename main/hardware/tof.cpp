#include "tof.h"
#include <Wire.h>

Adafruit_VL53L1X vl53 = Adafruit_VL53L1X(-1, -1);
static bool vl53_present = false;

static bool i2c_device_present(uint8_t addr) {
    Wire.beginTransmission(addr);
    return (Wire.endTransmission() == ESP_OK);
}

static bool vl53_try_init(void) {
    if (!i2c_device_present(0x29)) {
        return false;   // Silent: nothing on the bus, skip library init entirely.
    }
    if (!vl53.begin(0x29, &Wire)) {
        return false;
    }
    if (!vl53.startRanging()) {
        return false;
    }
    vl53.setTimingBudget(50);
    return true;
}

void vl53_init(void) {
    vl53_present = vl53_try_init();
}

/**
 * Read the current distance from the ToF sensor.
 * If the sensor is absent or disconnected, it attempts to re-initialise once.
 * Returns true and sets *distance_mm on success, false otherwise.
 */
bool vl53_read(int16_t *distance_mm) {
    if (!vl53_present) {
        // Sensor was absent — try to (re-)connect.
        vl53_present = vl53_try_init();
        if (!vl53_present) {
            return false;
        }
        // Just (re-)initialised: the sensor needs at least one timing budget
        // period before data is ready. Skip the dataReady check this cycle so
        // we don't misread a transient vl_status and mark it absent again.
        return false;
    }

    if (!vl53.dataReady()) {
        // Use a raw I²C probe to distinguish "bus error / disconnected" from
        // "simply no measurement ready yet". vl_status is unreliable here
        // because it can be non-zero right after a fresh init.
        if (!i2c_device_present(0x29)) {
            vl53_present = false;
        }
        return false;
    }

    int16_t d = vl53.distance();
    if (d == -1) {
        // Read error after dataReady — sensor removed between the two calls.
        vl53.clearInterrupt();
        vl53_present = false;
        return false;
    }

    vl53.clearInterrupt();
    *distance_mm = d;
    return true;
}

/*
void setup() {

    
  Serial.begin(115200);
  while (!Serial) delay(10);


  if (! vl53.begin(0x29, &Wire)) {
    Serial.print(F("Error on init of VL sensor: "));
    Serial.println(vl53.vl_status);
    while (1)       delay(10);
  }
  Serial.println(F("VL53L1X sensor OK!"));

  Serial.print(F("Sensor ID: 0x"));
  Serial.println(vl53.sensorID(), HEX);

  if (! vl53.startRanging()) {
    Serial.print(F("Couldn't start ranging: "));
    Serial.println(vl53.vl_status);
    while (1)       delay(10);
  }
  Serial.println(F("Ranging started"));

  // Valid timing budgets: 15, 20, 33, 50, 100, 200 and 500ms!
  vl53.setTimingBudget(50);
  Serial.print(F("Timing budget (ms): "));
  Serial.println(vl53.getTimingBudget());

  
  vl.VL53L1X_SetDistanceThreshold(100, 300, 3, 1);
  vl.VL53L1X_SetInterruptPolarity(0);
  
}
*/


