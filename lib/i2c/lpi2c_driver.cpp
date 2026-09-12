#include "lpi2c_driver.h"

IMXRT_LPI2C_t* lpi2c_init(uint8_t bus_number) {
    IMXRT_LPI2C_t *port = nullptr;
    uint32_t pad_cfg = 0x1F8B0; // 22K Pull-up, Open Drain, Fast Slew Rate

    if (bus_number == 1) {
        // Teensy Pins 18 (SDA) / 19 (SCL) -> LPI2C1
        port = &IMXRT_LPI2C1;
        CCM_CCGR2 |= CCM_CCGR2_LPI2C1(CCM_CCGR_ON);

        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_00 = 3 | 0x10; // SCL
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_01 = 3 | 0x10; // SDA
        IOMUXC_LPI2C1_SCL_SELECT_INPUT = 0;
        IOMUXC_LPI2C1_SDA_SELECT_INPUT = 0;
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_00 = pad_cfg;
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_01 = pad_cfg;

    } else if (bus_number == 3) {
        // Teensy Pins 17 (SDA) / 16 (SCL) -> LPI2C3
        port = &IMXRT_LPI2C3;
        CCM_CCGR2 |= CCM_CCGR2_LPI2C3(CCM_CCGR_ON);

        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_07 = 1 | 0x10; // SCL
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_06 = 1 | 0x10; // SDA
        IOMUXC_LPI2C3_SCL_SELECT_INPUT = 2;
        IOMUXC_LPI2C3_SDA_SELECT_INPUT = 2;
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_07 = pad_cfg;
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_06 = pad_cfg;

    }
    else if (bus_number == 4) {
        // Teensy Pins 25 (SDA) / 24 (SCL) -> LPI2C4
        port = &IMXRT_LPI2C4;

        // FIX: Manually enable Clock Gate 12 (bits 24-25) in CCGR6 for LPI2C4
        CCM_CCGR6 |= (3 << 24);

        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_12 = 0 | 0x10; // SCL
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_13 = 0 | 0x10; // SDA
        IOMUXC_LPI2C4_SCL_SELECT_INPUT = 0;
        IOMUXC_LPI2C4_SDA_SELECT_INPUT = 0;
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B0_12 = pad_cfg;
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B0_13 = pad_cfg;
    }
    else {
        return nullptr; // Unsupported bus
    }

    // Common Configuration
    port->MCR &= ~LPI2C_MCR_MEN;
    port->MCR |= LPI2C_MCR_RST;
    port->MCR &= ~LPI2C_MCR_RST;

    port->MCFGR1 = LPI2C_MCFGR1_PRESCALE(2);
    port->MCFGR2 = LPI2C_MCFGR2_FILTSCL(2) | LPI2C_MCFGR2_FILTSDA(2);

    port->MCCR0 = LPI2C_MCCR0_SETHOLD(0x0F) | LPI2C_MCCR0_CLKLO(0x11) |
                  LPI2C_MCCR0_CLKHI(0x06) | LPI2C_MCCR0_DATAVD(0x05);

    port->MFCR = LPI2C_MFCR_TXWATER(0) | LPI2C_MFCR_RXWATER(0);
    port->MCR |= LPI2C_MCR_MEN;

    return port;
}

bool lpi2c_write(IMXRT_LPI2C_t *port, uint8_t device_addr, const uint8_t *data, uint32_t length, bool send_stop) {
    if (!port) return false;
    uint32_t timeout = 100000;

    // 1. Wait for Bus Idle with a timeout
    while (port->MSR & LPI2C_MSR_BBF) {
        if (--timeout == 0) return false;
    }

    // 2. Clear any previous STOP or NACK flags
    port->MSR = LPI2C_MSR_SDF | LPI2C_MSR_NDF;

    // 3. Send START condition + Device Address (Write bit = 0)
    port->MTDR = LPI2C_CMD_START | (device_addr << 1);

    // 4. Transmit data bytes
    for (uint32_t i = 0; i < length; i++) {
        timeout = 100000;

        // Wait for Transmit Data Flag (TDF), watch for NACK (NDF)
        while (!(port->MSR & LPI2C_MSR_TDF)) {
            if (port->MSR & LPI2C_MSR_NDF) {
                port->MSR = LPI2C_MSR_NDF;
                port->MTDR = LPI2C_CMD_STOP;
                return false;
            }
            if (--timeout == 0) {
                port->MTDR = LPI2C_CMD_STOP;
                return false;
            }
        }
        port->MTDR = LPI2C_CMD_TXD | data[i];
    }

    // 5. Send STOP condition only if requested (IMUs often need this set to false for Repeated Start)
    if (send_stop) {
        port->MTDR = LPI2C_CMD_STOP;

        // Wait for STOP to complete (SDF) or a NACK (NDF)
        timeout = 100000;
        while (!(port->MSR & LPI2C_MSR_SDF)) {
            if (port->MSR & LPI2C_MSR_NDF) {
                port->MSR = LPI2C_MSR_NDF;
                port->MTDR = LPI2C_CMD_STOP;
                return false;
            }
            if (--timeout == 0) return false;
        }

        // Clear STOP flag
        port->MSR = LPI2C_MSR_SDF;
    }

    return true;
}

bool lpi2c_read(IMXRT_LPI2C_t *port, uint8_t device_addr, uint8_t *data, uint32_t length) {
    if (!port || length == 0 || length > 256) return false;
    uint32_t timeout = 100000;

    while (port->MSR & LPI2C_MSR_BBF) {
        if (--timeout == 0) return false;
    }

    port->MSR = LPI2C_MSR_SDF | LPI2C_MSR_NDF;
    port->MTDR = LPI2C_CMD_START | (device_addr << 1) | 0x01;
    port->MTDR = LPI2C_CMD_RXD | (length - 1);

    for (uint32_t i = 0; i < length; i++) {
        timeout = 100000;
        while (!(port->MSR & LPI2C_MSR_RDF)) {
             if (port->MSR & LPI2C_MSR_NDF) {
                 port->MSR = LPI2C_MSR_NDF;
                 port->MTDR = LPI2C_CMD_STOP;
                 return false;
             }
             if (--timeout == 0) {
                 port->MTDR = LPI2C_CMD_STOP;
                 return false;
             }
        }
        data[i] = (port->MRDR & 0xFF);
    }

    port->MTDR = LPI2C_CMD_STOP;

    timeout = 100000;
    while (!(port->MSR & LPI2C_MSR_SDF)) {
        if (port->MSR & LPI2C_MSR_NDF) {
            port->MSR = LPI2C_MSR_NDF;
            port->MTDR = LPI2C_CMD_STOP;
            return false;
        }
        if (--timeout == 0) return false;
    }

    port->MSR = LPI2C_MSR_SDF;
    return true;
}
