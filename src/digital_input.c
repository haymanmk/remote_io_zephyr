#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(digital_input, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "stm32f7xx_remote_io.h"

/* Define Input Node ID */
#define IN_1    DT_NODELABEL(usr_in_1)
#define IN_2    DT_NODELABEL(usr_in_2)
#define IN_3    DT_NODELABEL(usr_in_3)
#define IN_4    DT_NODELABEL(usr_in_4)
#define IN_5    DT_NODELABEL(usr_in_5)
#define IN_6    DT_NODELABEL(usr_in_6)
#define IN_7    DT_NODELABEL(usr_in_7)
#define IN_8    DT_NODELABEL(usr_in_8)
#define IN_9    DT_NODELABEL(usr_in_9)
#define IN_10   DT_NODELABEL(usr_in_10)
#define IN_11   DT_NODELABEL(usr_in_11)
#define IN_12   DT_NODELABEL(usr_in_12)
#define IN_13   DT_NODELABEL(usr_in_13)
#define IN_14   DT_NODELABEL(usr_in_14)
#define IN_15   DT_NODELABEL(usr_in_15)
#define IN_16   DT_NODELABEL(usr_in_16)

#define DIGITAL_INPUT_UPDATE_INTERVAL 10 // ms

/* type definition */
typedef struct NodeSubscribedInputs
{
    uint8_t index;
    struct NodeSubscribedInputs* next;
} node_subscribed_inputs_t;

/* private functions */
void digital_input_task(void* parameters);
struct gpio_dt_spec* digital_input_get_gpio_spec(uint8_t index);
node_subscribed_inputs_t* digital_input_create_node(uint8_t index);
void digital_input_delete_node(node_subscribed_inputs_t* node);
void digital_input_append_node(node_subscribed_inputs_t* node);
void digital_input_remove_node(node_subscribed_inputs_t* node);

/* variables */
// get gpio spec
static const struct gpio_dt_spec digital_input_1 = GPIO_DT_SPEC_GET(IN_1, gpios);
static const struct gpio_dt_spec digital_input_2 = GPIO_DT_SPEC_GET(IN_2, gpios);
static const struct gpio_dt_spec digital_input_3 = GPIO_DT_SPEC_GET(IN_3, gpios);
static const struct gpio_dt_spec digital_input_4 = GPIO_DT_SPEC_GET(IN_4, gpios);
static const struct gpio_dt_spec digital_input_5 = GPIO_DT_SPEC_GET(IN_5, gpios);
static const struct gpio_dt_spec digital_input_6 = GPIO_DT_SPEC_GET(IN_6, gpios);
static const struct gpio_dt_spec digital_input_7 = GPIO_DT_SPEC_GET(IN_7, gpios);
static const struct gpio_dt_spec digital_input_8 = GPIO_DT_SPEC_GET(IN_8, gpios);
static const struct gpio_dt_spec digital_input_9 = GPIO_DT_SPEC_GET(IN_9, gpios);
static const struct gpio_dt_spec digital_input_10 = GPIO_DT_SPEC_GET(IN_10, gpios);
static const struct gpio_dt_spec digital_input_11 = GPIO_DT_SPEC_GET(IN_11, gpios);
static const struct gpio_dt_spec digital_input_12 = GPIO_DT_SPEC_GET(IN_12, gpios);
static const struct gpio_dt_spec digital_input_13 = GPIO_DT_SPEC_GET(IN_13, gpios);
static const struct gpio_dt_spec digital_input_14 = GPIO_DT_SPEC_GET(IN_14, gpios);
static const struct gpio_dt_spec digital_input_15 = GPIO_DT_SPEC_GET(IN_15, gpios);
static const struct gpio_dt_spec digital_input_16 = GPIO_DT_SPEC_GET(IN_16, gpios);

TaskHandle_t digitalInputTaskHandle = NULL;
node_subscribed_inputs_t* headNodeSubscribedInputs = NULL;
node_subscribed_inputs_t* tailNodeSubscribedInputs = NULL;

io_status_t digital_input_init()
{
    // check if the GPIO is valid
    if (!device_is_ready(digital_input_1.port) || !device_is_ready(digital_input_2.port) ||
        !device_is_ready(digital_input_3.port) || !device_is_ready(digital_input_4.port) ||
        !device_is_ready(digital_input_5.port) || !device_is_ready(digital_input_6.port) ||
        !device_is_ready(digital_input_7.port) || !device_is_ready(digital_input_8.port) ||
        !device_is_ready(digital_input_9.port) || !device_is_ready(digital_input_10.port) ||
        !device_is_ready(digital_input_11.port) || !device_is_ready(digital_input_12.port) ||
        !device_is_ready(digital_input_13.port) || !device_is_ready(digital_input_14.port) ||
        !device_is_ready(digital_input_15.port) || !device_is_ready(digital_input_16.port))
    {
        LOG_ERR("Digital input GPIO is not ready");
        return STATUS_ERROR;
    }

    // configure the GPIO pins as input
    if (gpio_pin_configure_dt(&digital_input_1, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_2, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_3, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_4, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_5, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_6, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_7, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_8, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_9, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_10, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_11, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_12, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_13, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_14, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_15, GPIO_INPUT) < 0 ||
    gpio_pin_configure_dt(&digital_input_16, GPIO_INPUT) < 0)
    {
        LOG_ERR("Failed to configure digital input GPIO");
        return STATUS_ERROR;
    }

    return STATUS_OK;
}

// task for monitoring subscribed digital inputs
void digital_input_task(void* parameters)
{
    for (;;)
    {
        // check if there is a subscribed digital input
        if (headNodeSubscribedInputs == NULL)
        {
            // wait for new subscription
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        // check the state of the subscribed digital inputs
        node_subscribed_inputs_t* current = headNodeSubscribedInputs;
        bool state = false;
        digital_input_t* input = NULL;

        while (current != NULL)
        {
            // read the state of the digital input
            state = digital_input_read(current->index);
            input = digital_input_instance(current->index);

            // check if the state has changed
            if (state != input->state)
            {
                // update the state
                input->state = state;

                // Notify clients with the format: "S<Service ID> <Input Index> <State>"
                ethernet_if_send(service, "S%d %d %d\r\n", SERVICE_ID_SUBSCRIBE_INPUT, current->index, state);
            }
            current = current->next;
        }

        // delay few milliseconds
        vTaskDelay(pdMS_TO_TICKS(DIGITAL_INPUT_UPDATE_INTERVAL));
    }
}

struct gpio_dt_spec* digital_input_get_gpio_spec(uint8_t index)
{
    switch (index)
    {
        case 1: return &digital_input_1;
        case 2: return &digital_input_2;
        case 3: return &digital_input_3;
        case 4: return &digital_input_4;
        case 5: return &digital_input_5;
        case 6: return &digital_input_6;
        case 7: return &digital_input_7;
        case 8: return &digital_input_8;
        case 9: return &digital_input_9;
        case 10: return &digital_input_10;
        case 11: return &digital_input_11;
        case 12: return &digital_input_12;
        case 13: return &digital_input_13;
        case 14: return &digital_input_14;
        case 15: return &digital_input_15;
        case 16: return &digital_input_16;
        default: return (struct gpio_dt_spec*){0};
    }
}

// read the state of a digital input specified by the index
bool digital_input_read(uint8_t index)
{
    int ret = gpio_pin_get_dt(digital_input_get_gpio_spec(index));

    if (ret < 0)
    {
        LOG_ERR("Failed to read digital input %d", index);
        return false;
    }
    return (bool)ret; // 1/active, 0/inactive
}

/**
 * @brief   read the state of all digital inputs
 * @return  the state of all digital inputs
 *          1/active, 0/inactive
 *          e.g. 0b0000000000000001 = digital input 1 is active
 */
uint32_t digital_input_read_all()
{
    uint32_t data = 0;
    for (uint8_t i = 1; i <= DIGITAL_INPUT_MAX; i++)
    {
        data |= (digital_input_read(i) << (i - 1));
    }
    return data;
}

// subscribe to a digital input specified by the index
// to create an event-driven feature for clients.
void digital_input_subscribe(uint8_t index)
{
    // make sure the index is valid
    if (index > DIGITAL_INPUT_MAX)
    {
        return;
    }

    // check if it is already subscribed
    node_subscribed_inputs_t* current = headNodeSubscribedInputs;
    while (current != NULL)
    {
        if (current->index == index)
        {
            return;
        }
        current = current->next;
    }

    // create a node for the subscribed digital input
    node_subscribed_inputs_t* newNode = digital_input_create_node(index);

    // add the node to the linked list
    digital_input_append_node(newNode);

    // update input state
    digital_input_t* input = digital_input_instance(index);
    input->state = digital_input_read(index);

    // give a notification to the task
    xTaskNotifyGive(digitalInputTaskHandle);
}

void digital_input_unsubscribe(uint8_t index)
{
    // make sure the index is valid
    if (index > DIGITAL_INPUT_MAX)
    {
        return;
    }

    // check if it is already subscribed
    node_subscribed_inputs_t* current = headNodeSubscribedInputs;
    while (current != NULL)
    {
        if (current->index == index)
        {
            digital_input_remove_node(current);
            digital_input_delete_node(current);
            return;
        }
        current = current->next;
    }
}

// print current subscribed digital inputs
// note: this function only prints the index of the subscribed digital inputs with a space as the delimiter.
void digital_input_print_subscribed_inputs()
{
    node_subscribed_inputs_t* current = headNodeSubscribedInputs;
    while (current != NULL)
    {
        ethernet_if_send(service, "%d", current->index);
        current = current->next;
        if (current != NULL)
        {
            ethernet_if_send(service, " ");
        }
    }
}

// create a node for the subscribed digital input
node_subscribed_inputs_t* digital_input_create_node(uint8_t index)
{
    node_subscribed_inputs_t* newNode = (node_subscribed_inputs_t*)pvPortMalloc(sizeof(node_subscribed_inputs_t));
    newNode->index = index;
    newNode->next = NULL;
    return newNode;
}

// delete a node for the subscribed digital input
void digital_input_delete_node(node_subscribed_inputs_t* node)
{
    node->next = NULL;
    vPortFree(node);
}

// append a node to the linked list
void digital_input_append_node(node_subscribed_inputs_t* node)
{
    if (headNodeSubscribedInputs == NULL)
    {
        headNodeSubscribedInputs = node;
        tailNodeSubscribedInputs = node;
    }
    else
    {
        tailNodeSubscribedInputs->next = node;
        tailNodeSubscribedInputs = node;
    }
}

// remove a node from the linked list
void digital_input_remove_node(node_subscribed_inputs_t* node)
{
    if (headNodeSubscribedInputs == NULL)
    {
        return;
    }

    if (headNodeSubscribedInputs == node)
    {
        headNodeSubscribedInputs = headNodeSubscribedInputs->next;
        if (headNodeSubscribedInputs == NULL)
        {
            tailNodeSubscribedInputs = NULL;
        }
    }
    else
    {
        node_subscribed_inputs_t* current = headNodeSubscribedInputs;
        while (current->next != NULL)
        {
            if (current->next == node)
            {
                current->next = node->next;
                if (current->next == NULL)
                {
                    tailNodeSubscribedInputs = current;
                }
                break;
            }
            current = current->next;
        }
    }
}