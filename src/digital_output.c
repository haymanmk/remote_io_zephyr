#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(digital_output, LOG_LEVEL_DBG);

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util_macro.h>

#include "stm32f7xx_remote_io.h"
#include "digital_output.h"

#define GET_GPIO_SPEC(id, _) \
    static const struct gpio_dt_spec digital_output_##id = \
        GPIO_DT_SPEC_GET(DT_NODELABEL(usr_out_##id), gpios)
#define NOT_DEVICE_IS_READY(id, _)  !device_is_ready(digital_output_##id.port)
#define CONFIG_GPIO_AS_OUTPUT(id, _) gpio_pin_configure_dt(&digital_output_##id, GPIO_OUTPUT_ACTIVE) < 0
#define CASE_GPIO_SPEC(id, _) \
    case id: \
        return (struct gpio_dt_spec *)&digital_output_##id

// listify the GPIO spec
LISTIFY(DIGITAL_OUTPUT_MAX, GET_GPIO_SPEC, (;));

void digital_output_init()
{
    // check if GPIO is ready
    if (LISTIFY(DIGITAL_OUTPUT_MAX, NOT_DEVICE_IS_READY, (||)))
    {
        LOG_ERR("Digital output GPIO is not ready");
        return;
    }

    // configure the GPIO pins as output
    if (LISTIFY(DIGITAL_OUTPUT_MAX, CONFIG_GPIO_AS_OUTPUT, (||)))
    {
        LOG_ERR("Failed to configure digital output GPIO");
        return;
    }
}

// get gpio spec
struct gpio_dt_spec *digital_output_get_gpio_spec(uint8_t index)
{
    if (index < 0 || index >= DIGITAL_OUTPUT_MAX)
    {
        return NULL;
    }
    switch (index)
    {
        LISTIFY(DIGITAL_OUTPUT_MAX, CASE_GPIO_SPEC, (;));
        default: return NULL;
    }
}

// read data from digital output and update the state in the digital output instance
int digital_output_read(uint8_t index)
{
    int ret = gpio_pin_get_dt(digital_output_get_gpio_spec(index));
    if (ret < 0)
    {
        LOG_ERR("Failed to read digital output %d", index);
        return ret;
    }

    return ret; // 1/active, 0/inactive
}

// read all digital outputs
uint32_t digital_output_read_all()
{
    uint32_t data = 0;
    int ret = 0;
    for (uint8_t i = 0; i < DIGITAL_OUTPUT_MAX; i++)
    {
        ret = digital_output_read(i);
        if (ret < 0)
        {
            LOG_ERR("Failed to read digital output %d", i);
            return (uint32_t)ret;
        }
        data |= (ret << (i));
    }
    return data;
}

// write data to digital output
int digital_output_write(uint8_t index, bool state)
{
    if (index < 0 || index >= DIGITAL_OUTPUT_MAX)
    {
        LOG_ERR("Invalid digital output index %d", index);
        return -1;
    }

    // write the state to the GPIO pin
    return gpio_pin_set_dt(digital_output_get_gpio_spec(index), state);
}

// write data to digital outputs
int digital_output_write_multiple(uint32_t data, uint8_t start_index, uint8_t length)
{
    int ret = 0;

    for (uint8_t i = 0; i < length; i++)
    {
        if ((ret = digital_output_write(start_index + i, (data >> i) & 0x01)) < 0)
        {
            LOG_ERR("Failed to write digital output %d", start_index + i);
            return ret;
        }
    }

    return ret;
}