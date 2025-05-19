
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ws28xx_gpio, LOG_LEVEL_DBG);

#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>

#define LED_STRIP_NODE DT_NODELABEL(led_strip)

#if DT_NODE_HAS_PROP(LED_STRIP_NODE, chain_length)
#define STRIP_NUM_PIXELS	DT_PROP(LED_STRIP_NODE, chain_length)
#else
#error Unable to determine length of LED strip
#endif

static struct led_rgb pixels[STRIP_NUM_PIXELS] = { 0 };

// get the LED strip device
static const struct device *const led_strip_dev = DEVICE_DT_GET(DT_NODELABEL(led_strip));

int ws28xx_gpio_init(void)
{
    int ret = 0;
    // check if the LED strip device is ready
    if (!device_is_ready(led_strip_dev)) {
        LOG_ERR("LED strip device not ready");
        return -1;
    }

    // set the LED strip to off
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i].r = 0;
        pixels[i].g = 0;
        pixels[i].b = 0;
    }
    // update the LED strip
    ret = led_strip_update_rgb(led_strip_dev, pixels, STRIP_NUM_PIXELS);
    if (ret < 0) {
        LOG_ERR("Failed to turn off LED strip: %d", ret);
        return ret;
    }
    LOG_INF("LED strip initialized");
    return ret;
}

int ws28xx_gpio_set_color(uint8_t r, uint8_t g, uint8_t b, uint16_t led)
{
    int ret = 0;

    // check if the LED index is valid
    if (led >= STRIP_NUM_PIXELS) {
        return -1;
    }

    // set the color of the LED
    pixels[led].r = r;
    pixels[led].g = g;
    pixels[led].b = b;

    // update the LED strip
    ret = led_strip_update_rgb(led_strip_dev, pixels, STRIP_NUM_PIXELS);
    if (ret < 0) {
        LOG_ERR("Failed to update LED strip [%d]: %d", led, ret);
        goto exit;
    }

exit:
    return ret;
}

int ws28xx_gpio_set_color_all(uint8_t r, uint8_t g, uint8_t b)
{
    int ret = 0;
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i].r = r;
        pixels[i].g = g;
        pixels[i].b = b;
    }

    // update the LED strip
    ret = led_strip_update_rgb(led_strip_dev, pixels, STRIP_NUM_PIXELS);
    if (ret < 0) {
        LOG_ERR("Failed to update LED strip [all]: %d", ret);
        goto exit;
    }

exit:
    return ret;
}

int ws28xx_gpio_get_color(uint8_t *r, uint8_t *g, uint8_t *b, uint16_t led)
{
    // check if the LED index is valid
    if (led >= STRIP_NUM_PIXELS) {
        return -1;
    }

    // get the color of the LED
    *r = pixels[led].r;
    *g = pixels[led].g;
    *b = pixels[led].b;

    return 0;
}