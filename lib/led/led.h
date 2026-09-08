/* led.h */
#ifndef LED_H
#define LED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Public driver API */
void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */
