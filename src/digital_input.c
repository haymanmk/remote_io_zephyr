#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(digital_input, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/debug/thread_analyzer.h>

#include "stm32f7xx_remote_io.h"
#include "digital_input.h"

#define DIGITAL_INPUT_UPDATE_INTERVAL 1 // ms

/* type definition */
typedef struct ServiceNode {
	struct ServiceNode *next;
    digital_input_callback_fn_t cb;
	void *user_data;
} service_node_t;

typedef struct NodeSubscribedInputs {
	struct NodeSubscribedInputs *next;
	uint8_t index;
	bool state;
	service_node_t *services; // linked list of services
} node_subscribed_inputs_t;

/* private functions */
void digital_input_poll_task(void *parameters);
struct gpio_dt_spec *digital_input_get_gpio_spec(uint8_t index);

/* variables */
// get gpio spec
#if !defined(DIGITAL_INPUT_MAX) || DIGITAL_INPUT_MAX <= 0
#error "DIGITAL_INPUT_MAX must be defined and greater than 0"
#endif

#define GET_GPIO_SPEC(id, _)                                                                      \
	static const struct gpio_dt_spec digital_input_##id =                                      \
		GPIO_DT_SPEC_GET(DT_NODELABEL(usr_in_##id), gpios)
// listify the GPIO spec
LISTIFY(DIGITAL_INPUT_MAX, GET_GPIO_SPEC, (;));

node_subscribed_inputs_t *headNodeSubscribedInputs = NULL;

// used by listify
#define NOT_DEVICE_IS_READY(id, _)  !device_is_ready(digital_input_##id.port)
#define CONFIG_GPIO_AS_INPUT(id, _) gpio_pin_configure_dt(&digital_input_##id, GPIO_INPUT) < 0

// statically define a task for digital input polling
K_KERNEL_THREAD_DEFINE(digital_input_polling_task, 2048,
		       digital_input_poll_task, NULL, NULL, NULL,
		       CONFIG_REMOTEIO_SERVICE_PRIORITY - 1, 0, 500);

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

// polling task for monitoring subscribed digital inputs
void digital_input_poll_task(void *parameters)
{
	for (;;) {
		// check if there is a subscribed digital input
		if (headNodeSubscribedInputs == NULL) {
            LOG_DBG("suspend");
			// suspend the task if there is no subscribed digital input
			k_thread_suspend(digital_input_polling_task);
		}

		// check the state of the subscribed digital inputs
		node_subscribed_inputs_t *current = headNodeSubscribedInputs;
		bool state = false;

		while (current != NULL) {
			// read the state of the digital input
			state = digital_input_read(current->index);

			// check if the state has changed
			if (state != current->state) {
				// update the state
				current->state = state;

				// Notify clients with the format: "S<Service ID> <Input Index>
				// <State>"
				// execute the callback function with user data and results for each
				// subscriber.
				service_node_t *current_service = current->services;
				while (current_service != NULL) {
					current_service->cb(current_service->user_data,
							    current->index, state);
					current_service = current_service->next;
				}
			}
			current = current->next;
		}

		// delay few milliseconds by putting the task to sleep
		k_sleep(K_MSEC(DIGITAL_INPUT_UPDATE_INTERVAL));
	}
}

// used by listify
#define CASE_GPIO_SPEC(id, _)                                                                         \
	case id:                                                                                   \
		return (struct gpio_dt_spec *)&digital_input_##id

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

    // read the state of the digital input
    // if the state is active, return 1
    // if the state is inactive, return 0
    // active and inactive are defined in the device tree
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
void digital_input_subscribe(void *user_data, uint8_t index, digital_input_callback_fn_t callback)
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
			service_node_t *current_service = current->services;
            service_node_t *prev_service = NULL;
			while (current_service != NULL) {
				if (current_service == user_data) {
					// service is already registered
					return;
				}
                prev_service = current_service;
				current_service = current_service->next;
			}
			// create a new service node
			service_node_t *new_service_node =
				(service_node_t *)malloc(sizeof(service_node_t));
			if (new_service_node == NULL) {
				LOG_ERR("Failed to allocate memory for service node");
				return;
			}
			new_service_node->cb = callback;
			new_service_node->user_data = user_data;
			new_service_node->next = NULL;
			// add the service node to the linked list
            if (prev_service == NULL) {
                current->services = new_service_node;
            } else {
                prev_service->next = new_service_node;
            }
			return;
		}
		current = current->next;
	}

	// create a node for the subscribed digital input
	node_subscribed_inputs_t *newNode =
		(node_subscribed_inputs_t *)malloc(sizeof(node_subscribed_inputs_t));
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
	// update its input state
	newNode->state = digital_input_read(index);
    // append the new node to the end of the list
    utils_append_node((utils_node_t *)newNode, (utils_node_t *)headNodeSubscribedInputs);

	// resume the task if it is suspended
	k_thread_resume(digital_input_polling_task);
}

void digital_input_unsubscribe(void *user_data, uint8_t index)
{
	// make sure the index is valid
	if (index >= DIGITAL_INPUT_MAX) {
		return;
	}

	// check if it is already subscribed
	node_subscribed_inputs_t *current = headNodeSubscribedInputs;
    node_subscribed_inputs_t *prev = NULL;
	while (current != NULL) {
		if (current->index == index) {
            // remove service node from the linked list
            service_node_t *current_service = current->services;
            service_node_t *prev_service = NULL;
            while (current_service != NULL) {
                if (current_service->user_data == user_data) {
                    // remove the service node
                    if (prev_service == NULL) {
                        current->services = current_service->next;
                    } else {
                        prev_service->next = current_service->next;
                    }
                    free(current_service);
                    break;
                }
                prev_service = current_service;
                current_service = current_service->next;
            }

            // check if there are no more services subscribed to this digital input
            if (current->services == NULL) {
                // remove the node from the linked list
                if (prev == NULL) {
                    headNodeSubscribedInputs = current->next;
                } else {
                    prev->next = current->next;
                }
                // free the node
			    utils_free_node((utils_node_t *)current);
            }
			return;
		}
        prev = current;
		current = current->next;
	}
}

void digital_input_unsubscribe_all(void *user_data)
{
    node_subscribed_inputs_t *current = headNodeSubscribedInputs;
    while (current != NULL) {
        service_node_t *current_service = current->services;
        while (current_service != NULL) {
            if (current_service->user_data == user_data) {
                digital_input_unsubscribe(user_data, current->index);
                break;
            }
            current_service = current_service->next;
        }
        current = current->next;
    }
}

// print current subscribed digital inputs
// note: this function only prints the index of the subscribed digital inputs with a space as the
// delimiter.
void digital_input_print_subscribed_inputs(void *user_data, digital_input_callback_fn_t cb)
{
	node_subscribed_inputs_t *current = headNodeSubscribedInputs;
restart:
	while (current != NULL) {
        // go through the linked list of services
        // check if the user_data is equal to the service of the current node
        service_node_t *current_service = current->services;
        while (current_service != NULL) {
            if (current_service->user_data == user_data) {
                break;
            }
            if ((current_service = current_service->next) == NULL) {
                current = current->next;
                goto restart;
            }
        }
        // print the index of the subscribed digital input
        cb(user_data, "%d", current->index + 1);
		current = current->next;
		if (current != NULL) {
            cb(user_data, " ");
		}
	}
}