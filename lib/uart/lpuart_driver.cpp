#include "lpuart_driver.h"

LpuartDriver::LpuartDriver(uint32_t baseAddress) {
    peripheral = reinterpret_cast<LPUART_Type*>(baseAddress);
}

void LpuartDriver::init(uint32_t baudRate, uint32_t sourceClockHz) {
    // Ensure TX and RX are disabled before modifying registers
    disable();

    // Configure the baud rate
    setBaudRate(baudRate, sourceClockHz);

    // Reset TX and RX FIFOs (assuming default configuration)
    peripheral->FIFO |= (1 << 14) | (1 << 15); // TXFLUSH and RXFLUSH bits

    // Enable the module
    enable();
}

bool LpuartDriver::available() {
    return (peripheral->STAT & LPUART_Bits::STAT_RDRF) != 0;
}

bool LpuartDriver::setBaudRate(uint32_t baudRate, uint32_t sourceClockHz) {
    if (baudRate == 0 || sourceClockHz == 0) {
        return false;
    }

    // Default oversampling ratio (OSR). Target is 16x oversampling.
    // The hardware formula uses (OSR+1), so we write 15 to the register.
    uint32_t osrValue = 15;
    uint32_t oversamplingMultiplier = osrValue + 1;

    // Calculate Modulo Divide (SBR)
    // Formula: Baud Rate = Clock / (SBR * (OSR + 1))
    // Therefore: SBR = Clock / (Baud Rate * (OSR + 1))
    uint32_t sbrValue = sourceClockHz / (baudRate * oversamplingMultiplier);

    // Ensure SBR falls within the valid 13-bit range (1 to 8191)
    if (sbrValue == 0 || sbrValue > 8191) {
        return false;
    }

    // Read current BAUD register, clear OSR and SBR bits, then write new values
    uint32_t baudReg = peripheral->BAUD;
    baudReg &= ~(LPUART_Bits::BAUD_OSR_MASK | LPUART_Bits::BAUD_SBR_MASK);

    baudReg |= (osrValue << LPUART_Bits::BAUD_OSR_SHIFT) & LPUART_Bits::BAUD_OSR_MASK;
    baudReg |= (sbrValue & LPUART_Bits::BAUD_SBR_MASK);

    peripheral->BAUD = baudReg;

    return true;
}

void LpuartDriver::writeByte(uint8_t data) {
    // Wait until the Transmit Data Register is Empty (TDRE = 1)
    while (!(peripheral->STAT & LPUART_Bits::STAT_TDRE)) {
        // Active waiting
    }

    // Write data to the lower 8 bits of the DATA register
    peripheral->DATA = data;
}

uint8_t LpuartDriver::readByte() {
    // Wait until the Receive Data Register is Full (RDRF = 1)
    while (!(peripheral->STAT & LPUART_Bits::STAT_RDRF)) {
        // Active waiting
    }

    // Return the lower 8 bits from the DATA register
    return static_cast<uint8_t>(peripheral->DATA & 0xFF);
}

void LpuartDriver::write(const uint8_t* buffer, size_t length) {
    if (!buffer) return;

    for (size_t i = 0; i < length; ++i) {
        writeByte(buffer[i]);
    }
}

void LpuartDriver::enable() {
    // Enable both Transmitter (TE) and Receiver (RE)
    peripheral->CTRL |= (LPUART_Bits::CTRL_TE | LPUART_Bits::CTRL_RE);
}

void LpuartDriver::disable() {
    // Disable both Transmitter (TE) and Receiver (RE)
    peripheral->CTRL &= ~(LPUART_Bits::CTRL_TE | LPUART_Bits::CTRL_RE);
}
