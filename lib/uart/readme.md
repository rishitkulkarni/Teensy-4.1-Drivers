# lpuart_driver

A small, bare-metal UART driver for the **i.MX RT1060** (Teensy 4.x, RT1060-EVK, etc.). It talks straight to the LPUART registers: no HAL, no Arduino `Serial`, no dependencies beyond `<stdint.h>` and `<stddef.h>`.

**What you get:** one C++ class, `LpuartDriver`, with `init`, `writeByte` / `write`, `readByte`, `available`, and a few helpers. Everything is blocking and polled.

---

## Quick start

### 1. Add the files

Copy `lpuart_driver.h` and `lpuart_driver.cpp` into your project (C++11 or newer).

### 2. Do the two things this driver does NOT do for you

This driver only touches the LPUART registers themselves. Before `init()` you must:

1. **Enable the peripheral's clock gate** (the matching `CCM_CCGRx` bits) and make sure the LPUART root clock is configured. Accessing LPUART registers with the gate off will hang or fault.
2. **Mux the TX / RX pads** to the LPUART function in `IOMUXC` (plus the daisy-chain input select where the pin needs one).

Both are board / pin specific, so they live in your board-support code.

> On Teensy, `Serial1` to `Serial7` use these same LPUART peripherals. Don't drive a port with this driver and `HardwareSerial` at the same time.

### 3. Send and receive

```cpp
#include "lpuart_driver.h"

LpuartDriver uart(LPUART1_BASE);

void setup() {
    // clock gate + pin mux for LPUART1 done here (your code)

    uart.init(115200, 24000000);          // baud, LPUART root clock in Hz

    const uint8_t msg[] = "hello\r\n";
    uart.write(msg, sizeof(msg) - 1);
}

void loop() {
    if (uart.available()) {               // check first, readByte() blocks
        uint8_t b = uart.readByte();
        uart.writeByte(b);                // echo it back
    }
}
```

The second argument of `init()` must be the **actual frequency of your LPUART root clock**. If it's wrong, your baud rate is wrong by the same ratio. Typical values: 24 MHz (Teensyduino default) or 80 MHz (RT1060 reset default, PLL3 / 6). Check your own clock setup.

---

## API cheat sheet

```cpp
explicit LpuartDriver(uint32_t baseAddress);

void     init(uint32_t baudRate, uint32_t sourceClockHz);
bool     setBaudRate(uint32_t baudRate, uint32_t sourceClockHz);

void     writeByte(uint8_t data);                  // blocking
void     write(const uint8_t *buffer, size_t length);
uint8_t  readByte();                               // blocking
bool     available();                              // is a byte waiting?

void     enable();                                 // TX + RX on
void     disable();                                // TX + RX off
```

| Function | What it does |
|----------|--------------|
| `LpuartDriver(base)` | Binds the object to one LPUART. Use `LPUART1_BASE` ... `LPUART8_BASE` |
| `init(baud, clk)` | Disables TX/RX, sets baud, flushes both FIFOs, enables TX/RX |
| `setBaudRate(baud, clk)` | Programs the baud divider. Returns `false` if the baud / clock combo can't be made |
| `writeByte(b)` | Waits until the transmit register is empty, then writes one byte |
| `write(buf, n)` | Calls `writeByte` for each byte. `nullptr` buffer is ignored |
| `readByte()` | Waits until a byte arrives, then returns it |
| `available()` | `true` if a received byte is waiting to be read |
| `enable()` / `disable()` | Turn the transmitter and receiver on / off together |

**Base addresses** (from the header):

| Constant | Address |
|----------|---------|
| `LPUART1_BASE` | `0x40184000` |
| `LPUART2_BASE` | `0x40188000` |
| `LPUART3_BASE` | `0x4018C000` |
| `LPUART4_BASE` | `0x40190000` |
| `LPUART5_BASE` | `0x40194000` |
| `LPUART6_BASE` | `0x40198000` |
| `LPUART7_BASE` | `0x4019C000` |
| `LPUART8_BASE` | `0x401A0000` |

---

## Frame format

The driver never touches the frame settings, so you get the **reset defaults: 8 data bits, no parity, 1 stop bit (8N1)**, no hardware flow control, and FIFOs left disabled. Both ends of the link must be 8N1.

---

## Baud rate: what to know

The driver uses fixed 16x oversampling and calculates the divider like this:

```
SBR    = sourceClockHz / (baudRate * 16)      (integer division, rounds down)
actual = sourceClockHz / (SBR * 16)
```

Valid `SBR` is 1 to 8191, otherwise `setBaudRate` returns `false`. In practice, your baud rate must be **at most clock / 16**.

Because `SBR` is rounded down, high baud rates can be well off target. Aim to stay within about 2 to 3 % error:

| Target baud | 24 MHz root (SBR, error) | 80 MHz root (SBR, error) |
|------------:|:------------------------:|:------------------------:|
| 9600        | 156, +0.16 %             | 520, +0.16 %             |
| 115200      | 13, +0.16 %              | 43, +0.94 %              |
| 230400      | 6, **+8.5 %**            | 21, +3.3 %               |
| 921600      | 1, **+62.8 %**           | 5, **+8.5 %**            |

If you need fast UART, pick a root clock that divides cleanly (for example, a clock that is a multiple of `baud * 16`).

---

## Recipes

**Check that your baud rate is valid** (`init()` does not report failure, see Limitations):

```cpp
uart.disable();
if (!uart.setBaudRate(115200, 24000000)) {
    // bad baud / clock combo, handle it
}
uart.enable();
```

**Read with a timeout** (since `readByte()` waits forever):

```cpp
bool read_with_timeout(LpuartDriver &u, uint8_t &out, uint32_t max_polls) {
    while (max_polls--) {
        if (u.available()) { out = u.readByte(); return true; }
    }
    return false;
}
```

**Print a C string:**

```cpp
const char *s = "boot ok\r\n";
uart.write(reinterpret_cast<const uint8_t*>(s), strlen(s));
```

---

## Troubleshooting

| Problem | Likely cause |
|---------|--------------|
| Hang or hard fault on `init()` | LPUART clock gate not enabled |
| Nothing on the wire | Pads not muxed to LPUART, or TX / RX swapped |
| Garbage characters | Wrong `sourceClockHz`, baud error too high (see table), or other side isn't 8N1 |
| Works, then RX stops after a burst of data | Receiver overrun, never cleared (see Limitations) |
| Last byte missing after `write()` then `disable()` | `write` returns when the byte is handed to hardware, not when it's fully sent |
| Compile error: `LPUART_Type` / `LPUART1_BASE` redefined | You also included NXP's `MIMXRT1062.h`. Rename or wrap one set |

---

## Limitations

- **Blocking and polled.** No interrupts, no DMA. `writeByte` and `readByte` have **no timeouts** and can spin forever.
- **No FIFO use.** FIFOs are not enabled, so the receiver holds effectively one byte. If you don't poll fast enough, incoming bytes are lost. `available()` means "a byte is waiting", not "the FIFO has data".
- **No error handling.** Overrun, framing, noise and parity flags are never read or cleared. An uncleared overrun can stop reception until it is cleared.
- **`init()` ignores `setBaudRate()` failure.** With an impossible baud / clock combo, the baud register is left as it was and TX / RX still get enabled.
- **`setBaudRate()` while enabled.** The hardware wants TX and RX off when changing the baud. `init()` does this for you; if you call it directly, `disable()` first.
- **No "transmit complete" wait.** There is no flush call, so don't `disable()` or sleep immediately after `write()`.
- **Not thread-safe.** On an RTOS, guard each port with a mutex.
- Only the baud rate is configurable. Parity, stop bits, data bits, flow control and 9-bit mode are not.

---

## Under the hood (optional reading)

<details>
<summary>Register map and bits used</summary>

### Register map (`LPUART_Type`)

| Offset | Register | Used for |
|:------:|----------|----------|
| 0x00 | VERID  | not used |
| 0x04 | PARAM  | not used |
| 0x08 | GLOBAL | not used |
| 0x0C | PINCFG | not used |
| 0x10 | BAUD   | baud divider (SBR) and oversampling (OSR) |
| 0x14 | STAT   | RX full / TX empty flags |
| 0x18 | CTRL   | TX / RX enable |
| 0x1C | DATA   | read / write the data byte |
| 0x20 | MATCH  | not used |
| 0x24 | MODIR  | not used |
| 0x28 | FIFO   | TX / RX flush on init |
| 0x2C | WATER  | not used |

### Bits

| Register | Field | Bits | Value used |
|----------|-------|:----:|------------|
| BAUD | SBR | 0 to 12 | computed divider |
| BAUD | OSR | 24 to 28 | 15 (16x oversampling) |
| STAT | RDRF | 21 | receive data register full |
| STAT | TDRE | 23 | transmit data register empty |
| CTRL | RE | 18 | receiver enable |
| CTRL | TE | 19 | transmitter enable |
| FIFO | RXFLUSH | 14 | set on `init()` |
| FIFO | TXFLUSH | 15 | set on `init()` |

</details>
