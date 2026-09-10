#include <Arduino.h>
//
// #include "i2c/lpi2c_driver.h"
#include <lpi2c_driver.h>
void setup() {
    Serial.begin(9600);
    lpi2c1_init();
}

void loop() {
    uint8_t buffer[2] = {0x00, 0x01};

    if (lpi2c1_write(0x77, buffer, 2)) {
        Serial.println("Write Successful!");
    } else {
        Serial.println("Write Failed (NACK).");
    }

    delay(1000);
}
