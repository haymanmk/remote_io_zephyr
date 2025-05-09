#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(digital_input, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util_macro.h>

#include "stm32f7xx_remote_io.h"
#include "digital_input.h"

#define DIGITAL_INPUT_UPDATE_INTERVAL 1 // ms

/* type definition */
typedef struct Node {
	struct Node *next;
} node_t;

typedef struct ServiceNode {
	struct ServiceNode *next;
	callback_fn_t *cb;
    void *user_data;
} service_node_t;

typedef struct NodeSubscribedInputs {
	struct NodeSubscribedInputs *next;
	uint8_t index;
	service_node_t *services; // linked list of services
} node_subscribed_inputs_t;

/* private functions */
void digital_input_task(void *parameters);
struct gpio_dt_spec *digital_input_get_gpio_spec(uint8_t index);
void digital_input_delete_node(node_t *node);
void digital_input_append_node(node_t *node, node_t *head);
void digital_input_remove_node(node_t *node, node_t *head);

/* variables */
// get gpio spec
#define GET_GPIO_SPEC(id)                                                                          \
	static const struct gpio_dt_spec digital_input_##id =                                      \
		GPIO_DT_SPEC_GET(DT_NODELABEL(usr_in_##id), gpios)
// listify the GPIO spec
LISTIFY(DIGITAL_INPUT_MAX, GET_GPIO_SPEC, (;));

TaskHandle_t digitalInputTaskHandle = NULL;
node_subscribed_inputs_t *headNodeSubscribedInputs = NULL;

// used by listify
#define NOT_DEVICE_IS_READY(id)  !device_is_ready(digital_input_##id.port)
#define CONFIG_GPIO_AS_INPUT(id) gpio_pin_configure_dt(&digital_input_##id, GPIO_INPUT) < 0

io_status_t digital_input_init()
{
	// check if the GPIO is valid
	/**
	    if (!device_is_ready(digital_input_0.port) || !device_is_ready(digital_input_1.port) ||
		!device_is_ready(digital_input_2.port) || !device_is_ready(digital_input_3.port) ||
		...
		!device_is_ready(digital_input_12.port) || !device_is_ready(digital_input_13.port)
	   || !device_is_ready(digital_input_14.port) || !device_is_ready(digital_input_15.port))
	 */
	if (LISTIFY(DIGITAL_INPUT_MAX, NOT_DEVICE_IS_READY, (||))) {
		LOG_ERR("Digital input GPIO is not ready");
		return STATUS_ERROR;
	}

	// configure the GPIO pins as input
	/**
	    if (gpio_pin_configure_dt(&digital_input_0, GPIO_INPUT) < 0 ||
		gpio_pin_configure_dt(&digital_input_1, GPIO_INPUT) < 0 ||
		gpio_pin_configure_dt(&digital_input_2, GPIO_INPUT) < 0 ||
		...
		gpio_pin_configure_dt(&digital_input_14, GPIO_INPUT) < 0 ||
		gpio_pin_configure_dt(&digital_input_15, GPIO_INPUT) < 0)
	 */
	if (LISTIFY(DIGITAL_INPUT_MAX, CONFIG_GPIO_AS_INPUT, (||))) {
		LOG_ERR("Failed to configure digital input GPIO");
		return STATUS_ERROR;
	}

	return STATUS_OK;
}

// task for monitoring subscribed digital inputs
void digital_input_task(void *parameters)
{
	for (;;) {
		// check if there is a subscribed digital input
		if (headNodeSubscribedInputs == NULL) {
			// wait for new subscription
			ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		}

		// check the state of the subscribed digital inputs
		node_subscribed_inputs_t *current = headNodeSubscribedInputs;
		bool state = false;
		digital_input_t *input = NULL;

		while (current != NULL) {
			// read the state of the digital input
			state = digital_input_read(current->index);
			input = digital_input_instance(current->index);

			// check if the state has changed
			if (state != input->state) {
				// update the state
				input->state = state;

				// Notify clients with the format: "S<Service ID> <Input Index>
				// <State>"
                /**
                 * TODO: check if this code segment is correct
                 */
                // execute the callback function with user data and results
                service_node_t *service_current = current->services;
                while (service_current != NULL) {
                    service_current->cb(service_current->user_data, current->index, state);
                    service_current = service_current->next;
                }

			}
			current = current->next;
		}

		// delay few milliseconds
		vTaskDelay(pdMS_TO_TICKS(DIGITAL_INPUT_UPDATE_INTERVAL));
	}
}

// used by listify
#define CASE_GPIO_SPEC(id)                                                                         \
	case id:                                                                                   \
		return &digital_input_##id

struct gpio_dt_spec *digital_input_get_gpio_spec(uint8_t index)
{
	switch (index) {
		/**
		 * case 0: return &digital_input_0;
		 * case 1: return &digital_input_1;
		 * case 2: return &digital_input_2;
		 * ...
		 * case 14: return &digital_input_14;
		 * case 15: return &digital_input_15;
		 */
		LISTIFY(DIGITAL_INPUT_MAX, CASE_GPIO_SPEC, (;));
	default:
		return (struct gpio_dt_spec *){0};
	}
}

// read the state of a digital input specified by the index
bool digital_input_read(uint8_t index)
{
	int ret = gpio_pin_get_dt(digital_input_get_gpio_spec(index));

	if (ret < 0) {
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
	for (uint8_t i = 0; i < DIGITAL_INPUT_MAX; i++) {
		data |= (digital_input_read(i) << (i));
	}
	return data;
}

// subscribe to a digital input specified by the index
// to create an event-driven feature for clients.
void digital_input_subscribe(void *user_data, uint8_t index, callback_fn_t *callback)
{
	// make sure the index is valid
	if (index >= DIGITAL_INPUT_MAX) {
		return;
	}

	// check if it is already subscribed
	node_subscribed_inputs_t *current = headNodeSubscribedInputs;
	while (current != NULL) {
		if (current->index == index) {
            // check if the service is already registered
            service_node_t *service_current = current->services;
            while (service_current != NULL) {
                if (service_current == user_data) {
                    // service is already registered
                    return;
                }
                service_current = service_current->next;
            }
            // create a new service node
            service_node_t *new_service_node = (service_node_t *)malloc(sizeof(service_node_t));
            if (new_service_node == NULL) {
                LOG_ERR("Failed to allocate memory for service node");
                return;
            }
            new_service_node->cb = callback;
            new_service_node->user_data = user_data;
            new_service_node->next = NULL;
            // add the service node to the linked list
            if (current->services == NULL) {
                current->services = new_service_node;
            } else {
                digital_input_append_node((node_t*)new_service_node, (node_t*)current->services);
            }
			return;
		}
		current = current->next;
	}

	// create a node for the subscribed digital input
	node_subscribed_inputs_t *newNode = (node_subscribed_inputs_t*)malloc(sizeof(node_subscribed_inputs_t));
    if (newNode == NULL) {
        LOG_ERR("Failed to allocate memory for subscribed digital input node");
        return;
    }
    newNode->index = index;
    newNode->next = NULL;
    newNode->services = NULL;
    // create a new service node
    service_node_t *new_service_node = (service_node_t *)malloc(sizeof(service_node_t));
    if (new_service_node == NULL) {
        LOG_ERR("Failed to allocate memory for service node");
        free(newNode);
        return;
    }
    new_service_node->cb = callback;
    new_service_node->user_data = user_data;
    new_service_node->next = NULL;
    // add the service node to the linked list
    newNode->services = new_service_node;
    // check if the head node is NULL
    if (headNodeSubscribedInputs == NULL) {
        headNodeSubscribedInputs = newNode;
    } else {
        // append the new node to the end of the list
        digital_input_append_node((node_t*)newNode, (node_t*)headNodeSubscribedInputs);
    }

    // check if the task is already created
    /**
     * TODO: check if the task is already created
     */
	// update input state
	digital_input_t *input = digital_input_instance(index);
	input->state = digital_input_read(index);

	// give a notification to the task
	xTaskNotifyGive(digitalInputTaskHandle);
}

void digital_input_unsubscribe(uint8_t index)
{
	// make sure the index is valid
	if (index >= DIGITAL_INPUT_MAX) {
		return;
	}

	// check if it is already subscribed
	node_subscribed_inputs_t *current = headNodeSubscribedInputs;
	while (current != NULL) {
		if (current->index == index) {
			digital_input_remove_node(current);
			digital_input_delete_node(current);
			return;
		}
		current = current->next;
	}
}

// print current subscribed digital inputs
// note: this function only prints the index of the subscribed digital inputs with a space as the
// delimiter.
void digital_input_print_subscribed_inputs()
{
	node_subscribed_inputs_t *current = headNodeSubscribedInputs;
	while (current != NULL) {
		ethernet_if_send(service, "%d", current->index);
		current = current->next;
		if (current != NULL) {
			ethernet_if_send(service, " ");
		}
	}
}

// delete a node for the subscribed digital input
void digital_input_delete_node(node_t *node)
{
	node->next = NULL;
	free(node);
}

// append a node to the linked list
void digital_input_append_node(node_t *node, node_t *head)
{
	if (head == NULL) {
		head = node;
	} else {
		node_t *current = head;
		// find the last node
		while (current->next != NULL) {
			current = current->next;
		}
		// append the new node to the end of the list
		current->next = node;
	}
}

// remove a node from the linked list
void digital_input_remove_node(node_t *node, node_t *head)
{
	if (head == NULL) {
		return;
	}

	if (head == node) {
		node_subscribed_inputs_t *temp = head;
		head = head->next;
		temp->next = NULL;
	} else {
		node_subscribed_inputs_t *current = headNodeSubscribedInputs;
		while (current->next != NULL) {
			if (current->next == node) {
				current->next = node->next;
				node->next = NULL;
				break;
			}
			current = current->next;
		}
	}
}
