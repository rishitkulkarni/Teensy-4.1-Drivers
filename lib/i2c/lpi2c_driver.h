#ifndef LPI2C_DRIVER_H
#define LPI2C_DRIVER_H

#include <Arduino.h>

#define LPI2C_CMD_TXD    (0x00000000)
#define LPI2C_CMD_RXD    (0x00000100)
#define LPI2C_CMD_STOP   (0x00000200)
#define LPI2C_CMD_START  (0x00000400)

// Initializes the bus and returns the hardware struct pointer
// bus_number: 1 (Pins 18/19), 3 (Pins 16/17)
IMXRT_LPI2C_t* lpi2c_init(uint8_t bus_number);

// Pass the port pointer (e.g., from lpi2c_init) to target a specific bus
// Add the send_stop parameter (defaults to true)
bool lpi2c_write(IMXRT_LPI2C_t *port, uint8_t device_addr, const uint8_t *data, uint32_t length, bool send_stop = true);
bool lpi2c_read(IMXRT_LPI2C_t *port, uint8_t device_addr, uint8_t *data, uint32_t length);

#endif // LPI2C_DRIVER_H
