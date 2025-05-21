#ifndef WS28XX_PWM_H
#define WS28XX_PWM_H

#include <stdint.h>
#include "stm32f7xx_hal.h"

#define WS2812

#define WS28XX_PWM_FREQ 800000                        // 800kHz
#define WS28XX_PWM_PERIOD (1000000 / WS28XX_PWM_FREQ) // 1.25us

#ifdef WS2812
    // timer period is in 135 ticks equal to 800kHz
    #define DUTY_CYCLE_HIGH_BIT 76 // unit: ticks, 0.7us
    #define DUTY_CYCLE_LOW_BIT 38 // 0.35us
    #define NUM_PWM_CYCLES_RESET 40 // > 50us
#endif

/**
 * @brief Number of LEDs per strip
 */
#define NUMBER_OF_LEDS 25

/**
 * @brief Number of LEDs updated per ISR
 * @note This value should be at least 1.
 *       And it depends on the space of the available memory on the device.
 *       For instance, if it is a RGB LED, a 24-bit data is required to manipulate a LED.
 *       Each bit is represented by a period of PWM signal, which means there are total 24 periods for a LED.
 *       As far as we know, each bit, 0 or 1, is represented by varying the duty cycle of the PWM signal.
 *       If the timer used for PWM signal generation is 16-bit, it will need a 16-bit data to carry the value of the duty cycle in a PWM period.
 *       So, the total memory required for a LED is 24 * 16 = 384 bits = 48 bytes.
 *       The required memory and the available memory on the device should be considered to determine the value of this macro.
 */
#define NUMBER_OF_LEDS_UPDATED_PER_ISR 5

/**
 * @brief Number of basic colors
 * @note The basic colors is likely to be white, Red, Green, and Blue.
 *       The number of basic colors is 3, which is RGB.
 *       The number of basic colors is 4, which is RGBW.
 */
#if defined(WS2812)
#define NUMBER_OF_BASIC_COLORS 3 // RGB
#endif

/**
 * @brief Size of the buffer to store the PWM data
 * @note The size of the buffer has to be 2 * NUMBER_OF_LEDS * (8 * NUMBER_OF_BASIC_COLORS) * NUMBER_OF_LEDS_UPDATED_PER_ISR.
 *       The first factor 2 is for the double buffering.
 *       The factor 8 is for the number of bits in a basic color. (0 ~ 255)
 *       The NUMBER_OF_BASIC_COLORS is for the number of basic colors.
 *       The NUMBER_OF_LEDS_UPDATED_PER_ISR is for the number of LEDs updated per ISR.
 */
#define WS28XX_PWM_BUFFER_SIZE (2 * (8 * NUMBER_OF_BASIC_COLORS) * NUMBER_OF_LEDS_UPDATED_PER_ISR)

// Flag for the operation of the LED strip
#define FLAG_OPERATION_UPDATING (1 << 0)
#define FLAG_OPERATION_RESET_SIGNAL (1 << 1)
#define FLAG_OPERATION_DMA_STOP (1 << 2)

/* user-defined type */
#if defined(WS2812)
typedef struct color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws_color_t;
#endif

/* Macro */
#define ASSERT(expr) while((expr) != 1);

/* Function Prototype */
void ws28xx_pwm_init(TIM_HandleTypeDef *_htim, uint32_t _tim_channel);
HAL_StatusTypeDef ws28xx_pwm_set_color(uint8_t r, uint8_t g, uint8_t b, uint16_t led);
void ws28xx_pwm_set_color_all(uint8_t r, uint8_t g, uint8_t b);
void ws28xx_pwm_set_color_all_off(void);
HAL_StatusTypeDef ws28xx_pwm_update(void);
void ws28xx_pwm_dma_half_complete_callback(void);
void ws28xx_pwm_dma_complete_callback(void);
int ws28xx_pwm_get_color(uint8_t *r, uint8_t *g, uint8_t *b, uint16_t led);

#define ws28xx_led_init() ws28xx_pwm_init(NULL, 0)
#define ws28xx_led_set_color(r, g, b, led) ws28xx_pwm_set_color(r, g, b, led)
#define ws28xx_led_set_color_all(r, g, b) ws28xx_pwm_set_color_all(r, g, b)
#define ws28xx_led_set_color_all_off() ws28xx_pwm_set_color_all_off()
#define ws28xx_led_update() ws28xx_pwm_update()
#define ws28xx_led_get_color(r, g, b, led) ws28xx_pwm_get_color(r, g, b, led)

#endif // WS28XX_PWM_H