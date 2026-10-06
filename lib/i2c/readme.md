# lpi2c_driver

A small, simple I2C driver for **Teensy 4.0 / 4.1** that talks directly to the chip's I2C hardware (LPI2C), without using Arduino's `Wire` library.

Use it to talk to sensors like IMUs, barometers and magnetometers, where you want full control over the bus and predictable behaviour.

**What you get:** 3 functions: `init`, `write`, `read`. That's it.

---

## Quick start

### 1. Add the files

Copy `lpi2c_driver.h` and `lpi2c_driver.cpp` into your project.

### 2. Wire the sensor

Pick a bus and connect the sensor's SDA / SCL to the matching Teensy pins, plus 3.3V and GND.

| Bus | SDA pin | SCL pin |
|:---:|:-------:|:-------:|
| 1   | 18      | 19      |
| 3   | 17      | 16      |
| 4   | 25      | 24      |

> Add **external pull-up resistors (2.2k to 4.7k to 3.3V)** on SDA and SCL. The Teensy's internal ones are weak (22k) and only OK for short wires.

> Don't use `Wire` / `Wire1` / `Wire2` on the same bus as this driver. They use the same hardware.

### 3. Read your first register

```cpp
#include "lpi2c_driver.h"

IMXRT_LPI2C_t *bus;

void setup() {
    Serial.begin(115200);
    bus = lpi2c_init(1);                 // bus 1 -> pins 18/19

    uint8_t reg = 0x75;                  // register to read (WHO_AM_I on many IMUs)
    uint8_t value;

    lpi2c_write(bus, 0x68, &reg, 1, false);   // "I want register 0x75" (no STOP)
    lpi2c_read (bus, 0x68, &value, 1, true);  // "give it to me"

    Serial.println(value, HEX);
}

void loop() {}
```

`0x68` is the sensor's I2C address. Check your sensor's datasheet for yours.

---

## API cheat sheet

```cpp
IMXRT_LPI2C_t* lpi2c_init(uint8_t bus_number);

bool lpi2c_write(IMXRT_LPI2C_t *port, uint8_t device_addr,
                 const uint8_t *data, uint32_t length,
                 bool send_stop = true);

bool lpi2c_read(IMXRT_LPI2C_t *port, uint8_t device_addr,
                uint8_t *data, uint32_t length,
                bool repeated_start = false);
```

| Function | What it does | Returns |
|----------|--------------|---------|
| `lpi2c_init(bus)` | Sets up pins, clock and the I2C hardware for bus 1, 3 or 4 | Port pointer, or `nullptr` if the bus number is invalid |
| `lpi2c_write(...)` | Sends `length` bytes to the device | `true` if OK |
| `lpi2c_read(...)` | Reads `length` bytes (1 to 256) from the device | `true` if OK |

**Parameters worth knowing**

- `device_addr`: the **7-bit** address, exactly as in the datasheet. Don't shift it, the driver does that.
- `send_stop` (write): `true` = finish the transfer normally. `false` = keep the bus held so you can do a read right after (repeated START).
- `repeated_start` (read): set to `true` only if the previous call was a `lpi2c_write(..., false)`.

Both `write` and `read` return `false` for: NACK (device didn't answer), timeout, bus stuck busy, null port, or bad length. They don't tell you which one.

---

## Common recipes

**Write a register** (e.g. wake up a sensor):

```cpp
uint8_t cmd[2] = { 0x6B, 0x00 };         // { register, value }
lpi2c_write(bus, 0x68, cmd, 2);
```

**Read a register** (write the register number, then read, with repeated START):

```cpp
bool read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint32_t n) {
    if (!lpi2c_write(bus, addr, &reg, 1, false)) return false;
    return lpi2c_read(bus, addr, buf, n, true);
}
```

**Scan the bus** (find which addresses respond):

```cpp
for (uint8_t a = 1; a < 0x78; a++) {
    if (lpi2c_write(bus, a, nullptr, 0)) {
        Serial.printf("Found device at 0x%02X\n", a);
    }
}
```

---

## Two I2C concepts, quickly

**7-bit address.** I2C devices have a 7-bit address (e.g. `0x68`). On the wire it's sent with a read/write bit attached, making 8 bits. Some datasheets quote that 8-bit form (e.g. `0xD0`). This driver wants the **7-bit** one.

**Repeated START.** To read a register you usually first *write* the register number, then *read*. Many sensors want no STOP in between. That's what `send_stop = false` followed by `repeated_start = true` does.

---

## Troubleshooting

| Problem | Likely cause |
|---------|--------------|
| `lpi2c_init` returns `nullptr` | Bus number isn't 1, 3 or 4 |
| Everything returns `false`, even a bus scan | No pull-ups, wrong pins, or SDA / SCL swapped |
| Write works but read fails | Forgot `repeated_start = true` on the read after `send_stop = false` |
| Wrong data from sensor | Used an 8-bit address instead of the 7-bit one |
| Worked once, then always fails | A device is holding SDA low after an aborted transfer. Needs bus recovery + `lpi2c_init` again |
| Nothing works on a bus | `Wire` / `Wire1` / `Wire2` is also using that bus |

---

## Limitations

- **Blocking.** Calls wait (poll) until done. No interrupts or DMA.
- **Master only.** No slave mode, no 10-bit addresses.
- **Not thread-safe.** On FreeRTOS, protect each bus with a mutex, and keep a write + read pair inside one lock.
- **Basic error handling.** NACK and timeout are handled. Arbitration loss and other hardware error flags are not checked, and the peripheral is not auto-reset after a timeout.
- **Timeouts are loop counts** (100000 iterations), not real time, so they vary with CPU speed and compiler optimisation.
- Teensy 4.0 / 4.1 only.

---

## Under the hood (optional reading)

<details>
<summary>Timing and pad configuration</summary>

### Clock speed

Set in `lpi2c_init`:

```
PRESCALE = 2         -> divide by 4
FILTSCL/FILTSDA = 2  -> glitch filter on SCL and SDA
CLKLO=0x11, CLKHI=0x06, SETHOLD=0x0F, DATAVD=0x05
```

Assuming the default 24 MHz LPI2C root clock on Teensy:

```
f_func = 24 MHz / 4 = 6 MHz
f_SCL ~= 6 MHz / (CLKLO + CLKHI + 2 + latency) ~= 6 MHz / ~26 ~= 230-240 kHz
```

So this is **not** 400 kHz. Confirm on a scope, since the filter and rise time shift the real value. To change speed, retune `CLKLO` / `CLKHI` (and `SETHOLD` / `DATAVD` for fast-mode margins).

### Pad settings (`0x1F8B0`)

| Field | Value |
|-------|-------|
| Pull-up | 22k, enabled |
| Output | open-drain |
| Hysteresis | on |
| Speed | medium (100 MHz) |
| Drive strength | R0/6 |
| Slew rate | slow (SRE = 0) |

### Supported pads

| Bus | Peripheral | SCL pad | SDA pad |
|:---:|:----------:|---------|---------|
| 1 | LPI2C1 | `GPIO_AD_B1_00` | `GPIO_AD_B1_01` |
| 3 | LPI2C3 | `GPIO_AD_B1_07` | `GPIO_AD_B1_06` |
| 4 | LPI2C4 | `GPIO_AD_B0_12` | `GPIO_AD_B0_13` |

</details>
