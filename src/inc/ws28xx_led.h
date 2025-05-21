#ifndef __WS28XX_GPIO_H
#define __WS28XX_GPIO_H


/* Public Function Prototype */
int ws28xx_led_init(void);
int ws28xx_led_set_color(uint8_t r, uint8_t g, uint8_t b, uint16_t led);
int ws28xx_led_set_color_all(uint8_t r, uint8_t g, uint8_t b);
int ws28xx_led_get_color(uint8_t *r, uint8_t *g, uint8_t *b, uint16_t led);

#endif