#include "lpi2c_driver.h"

void lpi2c1_init(void) {
    // 1. Enable the clock gate for LPI2C1
    // CCM_CCGR2, cg3 is LPI2C1. Set to 3 (Clock on during all modes)
    CCM_CCGR2 |= CCM_CCGR2_LPI2C1(CCM_CCGR_ON);

    // 2. Configure IOMUXC for Pins 18 and 19 to act as LPI2C1
    // Pin 19 (SCL) = GPIO_AD_B1_00 (ALT3)
    // Pin 18 (SDA) = GPIO_AD_B1_01 (ALT3)

    // Set MUX to ALT3 (LPI2C) and enable SION (Software Input On)
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_00 = 3 | 0x10;
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_01 = 3 | 0x10;

    // Set Daisy Chain input selects so the peripheral reads the correct pad
    IOMUXC_LPI2C1_SCL_SELECT_INPUT = 1; // 1 maps to GPIO_AD_B1_00
    IOMUXC_LPI2C1_SDA_SELECT_INPUT = 1; // 1 maps to GPIO_AD_B1_01

    // Pad Configuration: 22K Pull-up, Open Drain, Fast Slew Rate, DSE (Drive Strength)
    uint32_t pad_cfg = 0x1F8B0;
    IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_00 = pad_cfg;
    IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_01 = pad_cfg;

    // 3. Reset and Disable the LPI2C1 Master before configuring
    IMXRT_LPI2C1.MCR &= ~LPI2C_MCR_MEN;
    IMXRT_LPI2C1.MCR |= LPI2C_MCR_RST;
    IMXRT_LPI2C1.MCR &= ~LPI2C_MCR_RST;

    // 4. Set Timing Parameters for 1 Mbps (Fast-Mode Plus)
    // Assuming Teensy default LPI2C root clock of 60 MHz.
    // Based on NXP manual Table 47-5 (60MHz, 1 Mbps)
    IMXRT_LPI2C1.MCFGR1 = LPI2C_MCFGR1_PRESCALE(2); // Prescale by 4 (2^2)
    IMXRT_LPI2C1.MCFGR2 = LPI2C_MCFGR2_FILTSCL(2) | LPI2C_MCFGR2_FILTSDA(2); // Glitch filters

    IMXRT_LPI2C1.MCCR0 = LPI2C_MCCR0_SETHOLD(0x0F) |
                         LPI2C_MCCR0_CLKLO(0x11) |
                         LPI2C_MCCR0_CLKHI(0x06) |
                         LPI2C_MCCR0_DATAVD(0x05);

    // 5. FIFO configuration (0 = transmit when empty, receive when >0)
    IMXRT_LPI2C1.MFCR = LPI2C_MFCR_TXWATER(0) | LPI2C_MFCR_RXWATER(0);

    // 6. Enable the Master
    IMXRT_LPI2C1.MCR |= LPI2C_MCR_MEN;
}

bool lpi2c1_write(uint8_t device_addr, const uint8_t *data, uint32_t length) {
    // Wait for Bus Idle (MSR[BBF] == 0)
    while (IMXRT_LPI2C1.MSR & LPI2C_MSR_BBF);

    // Clear any previous STOP detect flags
    IMXRT_LPI2C1.MSR = LPI2C_MSR_SDF | LPI2C_MSR_NDF;

    // Send START condition + Device Address (Write bit = 0)
    IMXRT_LPI2C1.MTDR = LPI2C_CMD_START | (device_addr << 1);

    // Transmit data bytes
    for (uint32_t i = 0; i < length; i++) {
        // Wait until Transmit FIFO has space (TDF = Transmit Data Flag)
        while (!(IMXRT_LPI2C1.MSR & LPI2C_MSR_TDF));
        IMXRT_LPI2C1.MTDR = LPI2C_CMD_TXD | data[i];
    }

    // Send STOP condition
    IMXRT_LPI2C1.MTDR = LPI2C_CMD_STOP;

    // Wait for STOP to complete (SDF) or a NACK (NDF)
    while (!(IMXRT_LPI2C1.MSR & LPI2C_MSR_SDF)) {
        if (IMXRT_LPI2C1.MSR & LPI2C_MSR_NDF) { // NACK detected
            IMXRT_LPI2C1.MSR = LPI2C_MSR_NDF; // Clear NACK flag
            IMXRT_LPI2C1.MTDR = LPI2C_CMD_STOP; // Force a STOP to release bus
            return false;
        }
    }

    // Clear STOP flag
    IMXRT_LPI2C1.MSR = LPI2C_MSR_SDF;
    return true;
}

bool lpi2c1_read(uint8_t device_addr, uint8_t *data, uint32_t length) {
    if (length == 0 || length > 256) return false;

    // Wait for Bus Idle
    while (IMXRT_LPI2C1.MSR & LPI2C_MSR_BBF);

    IMXRT_LPI2C1.MSR = LPI2C_MSR_SDF | LPI2C_MSR_NDF;

    // Send START + Device Address (Read bit = 1)
    IMXRT_LPI2C1.MTDR = LPI2C_CMD_START | (device_addr << 1) | 0x01;

    // Issue Receive Command to RX FIFO (Instructs peripheral to clock in `length` bytes)
    IMXRT_LPI2C1.MTDR = LPI2C_CMD_RXD | (length - 1);

    // Read data from Receive FIFO
    for (uint32_t i = 0; i < length; i++) {
        // Wait for data to arrive in RX FIFO (RDF = Receive Data Flag)
        while (!(IMXRT_LPI2C1.MSR & LPI2C_MSR_RDF)) {
             if (IMXRT_LPI2C1.MSR & LPI2C_MSR_NDF) { // NACK detected on address
                 IMXRT_LPI2C1.MSR = LPI2C_MSR_NDF;
                 IMXRT_LPI2C1.MTDR = LPI2C_CMD_STOP;
                 return false;
             }
        }
        // Extract 8 bits of data
        // data[i] = (IMXRT_LPI2C1.MRDR & LPI2C_MRDR_DATA_MASK);
        data[i] = (IMXRT_LPI2C1.MRDR & 0xFF);
    }

    // Send STOP condition
    IMXRT_LPI2C1.MTDR = LPI2C_CMD_STOP;

    // Wait for STOP to hit the bus
    while (!(IMXRT_LPI2C1.MSR & LPI2C_MSR_SDF));
    IMXRT_LPI2C1.MSR = LPI2C_MSR_SDF;

    return true;
}
