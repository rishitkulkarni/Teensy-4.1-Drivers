#ifndef LPUART_DRIVER_H
#define LPUART_DRIVER_H

#include <stdint.h>
#include <stddef.h>

/* LPUART Base Addresses for i.MX RT1060 */
constexpr uint32_t LPUART1_BASE = 0x40184000;
constexpr uint32_t LPUART2_BASE = 0x40188000;
constexpr uint32_t LPUART3_BASE = 0x4018C000;
constexpr uint32_t LPUART4_BASE = 0x40190000;
constexpr uint32_t LPUART5_BASE = 0x40194000;
constexpr uint32_t LPUART6_BASE = 0x40198000;
constexpr uint32_t LPUART7_BASE = 0x4019C000;
constexpr uint32_t LPUART8_BASE = 0x401A0000;

/* LPUART Hardware Register Map */
struct LPUART_Type {
    volatile uint32_t VERID;  // 0x00: Version ID Register
    volatile uint32_t PARAM;  // 0x04: Parameter Register
    volatile uint32_t GLOBAL; // 0x08: LPUART Global Register
    volatile uint32_t PINCFG; // 0x0C: LPUART Pin Configuration Register
    volatile uint32_t BAUD;   // 0x10: LPUART Baud Rate Register
    volatile uint32_t STAT;   // 0x14: LPUART Status Register
    volatile uint32_t CTRL;   // 0x18: LPUART Control Register
    volatile uint32_t DATA;   // 0x1C: LPUART Data Register
    volatile uint32_t MATCH;  // 0x20: LPUART Match Address Register
    volatile uint32_t MODIR;  // 0x24: LPUART Modem IrDA Register
    volatile uint32_t FIFO;   // 0x28: LPUART FIFO Register
    volatile uint32_t WATER;  // 0x2C: LPUART Watermark Register
};

/* LPUART Bit Definitions */
namespace LPUART_Bits {
    // BAUD Register
    constexpr uint32_t BAUD_SBR_MASK  = 0x00001FFF; // Bits 0-12
    constexpr uint32_t BAUD_OSR_SHIFT = 24;
    constexpr uint32_t BAUD_OSR_MASK  = 0x1F000000; // Bits 24-28

    // STAT Register
    constexpr uint32_t STAT_RDRF      = (1UL << 21); // Receive Data Register Full
    constexpr uint32_t STAT_TDRE      = (1UL << 23); // Transmit Data Register Empty

    // CTRL Register
    constexpr uint32_t CTRL_RE        = (1UL << 18); // Receiver Enable
    constexpr uint32_t CTRL_TE        = (1UL << 19); // Transmitter Enable
}

class LpuartDriver {
public:
    /**
     * @brief Construct a new LPUART Driver object
     * @param baseAddress Hardware base address of the specific LPUART peripheral
     */
    explicit LpuartDriver(uint32_t baseAddress);

    /**
     * @brief Initialize the LPUART module
     * @param baudRate Target baud rate (e.g., 115200)
     * @param sourceClockHz Frequency of the LPUART asynchronous module clock
     */
    void init(uint32_t baudRate, uint32_t sourceClockHz);

    /**
     * @brief Calculate and set the baud rate
     */
    bool setBaudRate(uint32_t baudRate, uint32_t sourceClockHz);

    /**
     * @brief Transmit a single byte (blocking)
     */
    void writeByte(uint8_t data);

    /**
     * @brief Receive a single byte (blocking)
     */
    uint8_t readByte();

    /**
     * @brief Transmit a buffer of data
     */
    void write(const uint8_t* buffer, size_t length);

    /**
     * @brief Enable Transmitter and Receiver
     */
    void enable();

    /**
     * @brief Disable Transmitter and Receiver
     */
    void disable();
    /**
     * @brief Check if data is available to be read
     * @return true if a byte is in the RX FIFO, false otherwise
     */
    bool available();

private:
    LPUART_Type* peripheral;
};


#endif // LPUART_DRIVER_H
