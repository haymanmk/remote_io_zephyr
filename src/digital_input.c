#include "stm32f7xx_remote_io.h"

CREATE_DIGITAL_INPUT_INSTANCE(1);
CREATE_DIGITAL_INPUT_INSTANCE(2);
CREATE_DIGITAL_INPUT_INSTANCE(3);
CREATE_DIGITAL_INPUT_INSTANCE(4);
CREATE_DIGITAL_INPUT_INSTANCE(5);
CREATE_DIGITAL_INPUT_INSTANCE(6);
CREATE_DIGITAL_INPUT_INSTANCE(7);
CREATE_DIGITAL_INPUT_INSTANCE(8);
CREATE_DIGITAL_INPUT_INSTANCE(9);
CREATE_DIGITAL_INPUT_INSTANCE(10);
CREATE_DIGITAL_INPUT_INSTANCE(11);
CREATE_DIGITAL_INPUT_INSTANCE(12);
CREATE_DIGITAL_INPUT_INSTANCE(13);
CREATE_DIGITAL_INPUT_INSTANCE(14);
CREATE_DIGITAL_INPUT_INSTANCE(15);
CREATE_DIGITAL_INPUT_INSTANCE(16);
#ifdef DIGITAL_INPUT_17_PIN
CREATE_DIGITAL_INPUT_INSTANCE(17);
#endif
#ifdef DIGITAL_INPUT_18_PIN
CREATE_DIGITAL_INPUT_INSTANCE(18);
#endif

#define DIGITAL_INPUT_UPDATE_INTERVAL 10 // ms

/* type definition */
typedef struct NodeSubscribedInputs
{
    uint8_t index;
    struct NodeSubscribedInputs* next;
} node_subscribed_inputs_t;

/* private functions */
void digital_input_task(void* parameters);
digital_input_t* digital_input_instance(uint8_t index);
node_subscribed_inputs_t* digital_input_create_node(uint8_t index);
void digital_input_delete_node(node_subscribed_inputs_t* node);
void digital_input_append_node(node_subscribed_inputs_t* node);
void digital_input_remove_node(node_subscribed_inputs_t* node);

/* variables */
TaskHandle_t digitalInputTaskHandle = NULL;
node_subscribed_inputs_t* headNodeSubscribedInputs = NULL;
node_subscribed_inputs_t* tailNodeSubscribedInputs = NULL;

io_status_t digital_input_init()
{
    // create task for monitoring digital inputs
    xTaskCreate(digital_input_task, "Digital Input Task", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY+2, &digitalInputTaskHandle);

    return STATUS_OK;
}

// return the number of digital inputs
uint8_t digital_input_count()
{
    return DIGITAL_INPUT_MAX-1;
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
                api_printf("S%d %d %d\r\n", SERVICE_ID_SUBSCRIBE_INPUT, current->index, state);
            }
            current = current->next;
        }

        // delay few milliseconds
        vTaskDelay(pdMS_TO_TICKS(DIGITAL_INPUT_UPDATE_INTERVAL));
    }
}

digital_input_t* digital_input_instance(uint8_t index)
{
    switch (index)
    {
        case 1: return &DIGITAL_INPUT_INSTANCE(1);
        case 2: return &DIGITAL_INPUT_INSTANCE(2);
        case 3: return &DIGITAL_INPUT_INSTANCE(3);
        case 4: return &DIGITAL_INPUT_INSTANCE(4);
        case 5: return &DIGITAL_INPUT_INSTANCE(5);
        case 6: return &DIGITAL_INPUT_INSTANCE(6);
        case 7: return &DIGITAL_INPUT_INSTANCE(7);
        case 8: return &DIGITAL_INPUT_INSTANCE(8);
        case 9: return &DIGITAL_INPUT_INSTANCE(9);
        case 10: return &DIGITAL_INPUT_INSTANCE(10);
        case 11: return &DIGITAL_INPUT_INSTANCE(11);
        case 12: return &DIGITAL_INPUT_INSTANCE(12);
        case 13: return &DIGITAL_INPUT_INSTANCE(13);
        case 14: return &DIGITAL_INPUT_INSTANCE(14);
        case 15: return &DIGITAL_INPUT_INSTANCE(15);
        case 16: return &DIGITAL_INPUT_INSTANCE(16);
    #ifdef DIGITAL_INPUT_17_PIN
        case 17: return &DIGITAL_INPUT_INSTANCE(17);
    #endif
    #ifdef DIGITAL_INPUT_18_PIN
        case 18: return &DIGITAL_INPUT_INSTANCE(18);
    #endif
    }
    return NULL;
}

// read the state of a digital input specified by the index
bool digital_input_read(uint8_t index)
{
    switch (index)
    {
        case 1: return READ_DIGITAL_INPUT_STATE(1);
        case 2: return READ_DIGITAL_INPUT_STATE(2);
        case 3: return READ_DIGITAL_INPUT_STATE(3);
        case 4: return READ_DIGITAL_INPUT_STATE(4);
        case 5: return READ_DIGITAL_INPUT_STATE(5);
        case 6: return READ_DIGITAL_INPUT_STATE(6);
        case 7: return READ_DIGITAL_INPUT_STATE(7);
        case 8: return READ_DIGITAL_INPUT_STATE(8);
        case 9: return READ_DIGITAL_INPUT_STATE(9);
        case 10: return READ_DIGITAL_INPUT_STATE(10);
        case 11: return READ_DIGITAL_INPUT_STATE(11);
        case 12: return READ_DIGITAL_INPUT_STATE(12);
        case 13: return READ_DIGITAL_INPUT_STATE(13);
        case 14: return READ_DIGITAL_INPUT_STATE(14);
        case 15: return READ_DIGITAL_INPUT_STATE(15);
        case 16: return READ_DIGITAL_INPUT_STATE(16);
#ifdef DIGITAL_INPUT_17_PIN
        case 17: return READ_DIGITAL_INPUT_STATE(17);
#endif
#ifdef DIGITAL_INPUT_18_PIN
        case 18: return READ_DIGITAL_INPUT_STATE(18);
#endif
        default: Error_Handler();
    }

    // should never reach here
    return false;
}

uint32_t digital_input_read_all()
{
    uint32_t data = 0;
    for (uint8_t i = 1; i < DIGITAL_INPUT_MAX; i++)
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
    if (index >= DIGITAL_INPUT_MAX)
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
    if (index >= DIGITAL_INPUT_MAX)
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
        api_printf("%d", current->index);
        current = current->next;
        if (current != NULL)
        {
            api_printf(" ");
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