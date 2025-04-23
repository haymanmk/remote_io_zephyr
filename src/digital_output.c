#include "stm32f7xx_remote_io.h"


/* Private functions */

// create digital output instance
CREATE_DIGITAL_OUTPUT_INSTANCE(1);
CREATE_DIGITAL_OUTPUT_INSTANCE(2);
CREATE_DIGITAL_OUTPUT_INSTANCE(3);
CREATE_DIGITAL_OUTPUT_INSTANCE(4);
CREATE_DIGITAL_OUTPUT_INSTANCE(5);
CREATE_DIGITAL_OUTPUT_INSTANCE(6);
CREATE_DIGITAL_OUTPUT_INSTANCE(7);
CREATE_DIGITAL_OUTPUT_INSTANCE(8);
CREATE_DIGITAL_OUTPUT_INSTANCE(9);
CREATE_DIGITAL_OUTPUT_INSTANCE(10);
CREATE_DIGITAL_OUTPUT_INSTANCE(11);
CREATE_DIGITAL_OUTPUT_INSTANCE(12);
CREATE_DIGITAL_OUTPUT_INSTANCE(13);
CREATE_DIGITAL_OUTPUT_INSTANCE(14);
CREATE_DIGITAL_OUTPUT_INSTANCE(15);
CREATE_DIGITAL_OUTPUT_INSTANCE(16);
#ifdef DIGITAL_OUTPUT_17_PIN
CREATE_DIGITAL_OUTPUT_INSTANCE(17);
#endif
#ifdef DIGITAL_OUTPUT_18_PIN
CREATE_DIGITAL_OUTPUT_INSTANCE(18);
#endif

void digital_output_init()
{
    // read all digital outputs to update the state in each digital output instance
    digital_output_read_all();
}

digital_output_t* digital_output_instance(uint8_t index)
{
    switch(index)
    {
        case 1: return &DIGITAL_OUTPUT_INSTANCE(1);
        case 2: return &DIGITAL_OUTPUT_INSTANCE(2);
        case 3: return &DIGITAL_OUTPUT_INSTANCE(3);
        case 4: return &DIGITAL_OUTPUT_INSTANCE(4);
        case 5: return &DIGITAL_OUTPUT_INSTANCE(5);
        case 6: return &DIGITAL_OUTPUT_INSTANCE(6);
        case 7: return &DIGITAL_OUTPUT_INSTANCE(7);
        case 8: return &DIGITAL_OUTPUT_INSTANCE(8);
        case 9: return &DIGITAL_OUTPUT_INSTANCE(9);
        case 10: return &DIGITAL_OUTPUT_INSTANCE(10);
        case 11: return &DIGITAL_OUTPUT_INSTANCE(11);
        case 12: return &DIGITAL_OUTPUT_INSTANCE(12);
        case 13: return &DIGITAL_OUTPUT_INSTANCE(13);
        case 14: return &DIGITAL_OUTPUT_INSTANCE(14);
        case 15: return &DIGITAL_OUTPUT_INSTANCE(15);
        case 16: return &DIGITAL_OUTPUT_INSTANCE(16);
#ifdef DIGITAL_OUTPUT_17_PIN
        case 17: return &DIGITAL_OUTPUT_INSTANCE(17);
#endif
#ifdef DIGITAL_OUTPUT_18_PIN
        case 18: return &DIGITAL_OUTPUT_INSTANCE(18);
#endif
    }
    return NULL;
}

// read data from digital output and update the state in the digital output instance
bool digital_output_read(uint8_t index)
{
    // assert if index is out of range
    if (index >= DIGITAL_OUTPUT_MAX)
    {
        return false;
    }

    digital_output_t *output = digital_output_instance(index);
    bool state = false;
    GPIO_PinState pinState = HAL_GPIO_ReadPin(output->port, output->pin);

    switch (pinState)
    {
        case GPIO_PIN_RESET:
            state = false;
            break;
        case GPIO_PIN_SET:
            state = true;
            break;
    }
    // update output state
    output->state = state;

    return state;
}

// read all digital outputs
uint32_t digital_output_read_all()
{
    uint32_t data = 0;
    for (uint8_t i = 1; i < DIGITAL_OUTPUT_MAX; i++)
    {
        data |= (digital_output_read(i) << (i - 1));
    }
    return data;
}

// write data to digital output
void digital_output_write(uint8_t index, bool state)
{
    // assert if index is out of range
    if (index >= DIGITAL_OUTPUT_MAX)
    {
        return;
    }

    digital_output_t *output = digital_output_instance(index);
    GPIO_PinState pinState = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(output->port, output->pin, pinState);
    output->state = state;
}

// write data to digital outputs
void digital_output_write_multiple(uint32_t data, uint8_t start_index, uint8_t length)
{
    for (uint8_t i = 0; i < length; i++)
    {
        digital_output_write(start_index + i, (data >> i) & 0x01);
    }
}