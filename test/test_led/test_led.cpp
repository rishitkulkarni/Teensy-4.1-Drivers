#include <Arduino.h>
#include <unity.h>

#include "led.h"

void test_led_toggle(void)
{
    led_init();

    led_toggle();

    delay(3000);

    led_toggle();

    TEST_PASS();
}

void setup()
{
    delay(2000);

    UNITY_BEGIN();

    RUN_TEST(test_led_toggle);

    UNITY_END();
}

void loop()
{
}
