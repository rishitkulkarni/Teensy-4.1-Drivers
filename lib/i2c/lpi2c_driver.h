#ifndef LPI2C1_DRIVER_H
#define LPI2C1_DRIVER_H

#include <Arduino.h> // Brings in imxrt.h and Teensy core definitions

// Master Transmit Data Register (MTDR) Command Macros
#define LPI2C_CMD_TXD    (0x00000000) // Transmit DATA
#define LPI2C_CMD_RXD    (0x00000100) // Receive DATA
#define LPI2C_CMD_STOP   (0x00000200) // Generate STOP
#define LPI2C_CMD_START  (0x00000400) // Generate START and send address

// Initialize LPI2C1 on Pins 18 (SDA) and 19 (SCL) at 1 Mbps
void lpi2c1_init(void);

// Write data to an I2C device
// Returns true on success, false on failure (e.g., NACK)
bool lpi2c1_write(uint8_t device_addr, const uint8_t *data, uint32_t length);

// Read data from an I2C device
// Returns true on success, false on failure
bool lpi2c1_read(uint8_t device_addr, uint8_t *data, uint32_t length);

#endif // LPI2C1_DRIVER_H
