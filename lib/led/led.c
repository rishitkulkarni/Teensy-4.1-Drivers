/* led.c */
#include "led.h"

/* ---- Register map (i.MX RT1060) ---- */

//refer page 507, IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03
//B0 group base = 0x401F8140 (B0_00), +4 bytes/pin -> B0_03 = 0x401F814C
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 (*(volatile uint32_t *)0x401F814C)

//refer page 1077
#define CCM_CCGR0 (*(volatile uint32_t *)0x400FC068)

//refer page 958, GPIO7 = fast alias of GPIO2 (default routing on reset, GPR27 = 0xFFFFFFFF)
#define GPIO7_GDIR      (*(volatile uint32_t *)0x42004004)
//refer page 971
#define GPIO7_DR_SET    (*(volatile uint32_t *)0x42004084)
//refer page 972
#define GPIO7_DR_CLEAR  (*(volatile uint32_t *)0x42004088)
//refer page 973
#define GPIO7_DR_TOGGLE (*(volatile uint32_t *)0x4200408C)

#define LED_MASK (1u << 3)

void led_init(void)
{
    CCM_CCGR0 |= (3u << 30);                     //enable clock gate
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 5;         //ALT5 -> GPIO2_IO03 / GPIO7_IO03
    GPIO7_GDIR |= LED_MASK;                       //configure as OUTPUT
    GPIO7_DR_CLEAR = LED_MASK;                    //start LED off
}

void led_on(void)
{
    GPIO7_DR_SET = LED_MASK;
}

void led_off(void)
{
    GPIO7_DR_CLEAR = LED_MASK;
}

void led_toggle(void)
{
    GPIO7_DR_TOGGLE = LED_MASK;
}
