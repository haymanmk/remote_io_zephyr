#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ethernet_if, LOG_LEVEL_INF);

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/kernel/thread_stack.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/posix/poll.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "settings.h"
#include "ethernet_if.h"
#include "digital_input.h"

// extern settings_t settings;

#define PRESS_MORE_THAN_100MS 50 // 5s
#define DEFAULT_PORT 8500
#define MAX_RX_BUFFER_SIZE 128
#define MAX_TX_BUFFER_SIZE 128
#define POLLABLE_SOCKETS 2 // only one socket service

#if defined(CONFIG_NET_MAX_CONTEXTS)
/* POLLABLE_SOCKETS must be less than CONFIG_NET_MAX_CONTEXTS */
_Static_assert(POLLABLE_SOCKETS < CONFIG_NET_MAX_CONTEXTS,
            "POLLABLE_SOCKETS must be less than CONFIG_NET_MAX_CONTEXTS=" STRINGIFY(CONFIG_NET_MAX_CONTEXTS));
#endif

/* Local function prototypes */
static void receive_data(struct net_socket_service_event *pev, char *buf, size_t buf_len);
static void tcp_service_handler(struct net_socket_service_event *pev);
static int create_socket_service_thread(ethernet_if_socket_service_t *service);
static int close_socket_service(ethernet_if_socket_service_t *service);
static ethernet_if_socket_service_t *register_client_at_socket_service(int client);
static int unregister_client_at_socket_service(int client);
static int unregister_all_clients_at_socket_service(void);
void ethernet_if_respond_handler(ethernet_if_socket_service_t *service, const char *format, ...);
void ethernet_if_respond_raw_bytes_handler(ethernet_if_socket_service_t *service, const uint8_t *buf, size_t len);


// define the locking mechanism
static K_MUTEX_DEFINE(lock);
static K_MUTEX_DEFINE(lock_sock_send);

static char addr_str[INET_ADDRSTRLEN];
static uint8_t mac_addr[NET_LINK_ADDR_MAX_LENGTH];

// Define socket services
TCP_SERV_DEFINE_N(tcp_service_handler, POLLABLE_SOCKETS);

// define a table to hold the socket services which are managed locally
static ethernet_if_socket_service_t *socket_service_table[POLLABLE_SOCKETS];

// define stack pool for the socket services
static K_THREAD_STACK_ARRAY_DEFINE(socket_service_stack_pool,
                            POLLABLE_SOCKETS,
                            CONFIG_REMOTEIO_SERVICE_STACK_SIZE);

// event for receiving new data
extern struct k_event apiNewDataEvent;

extern settings_t defaults;

// get on-board user button GPIO spec
static const struct gpio_dt_spec user_button =
    GPIO_DT_SPEC_GET(DT_NODELABEL(user_button), gpios);
// get on-board user LED GPIO spec
static const struct gpio_dt_spec green_led =
    GPIO_DT_SPEC_GET(DT_NODELABEL(green_led), gpios);
static const struct gpio_dt_spec blue_led =
    GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led), gpios);
static const struct gpio_dt_spec red_led =
    GPIO_DT_SPEC_GET(DT_NODELABEL(red_led), gpios);

/* Functions */


static void tcp_service_handler(struct net_socket_service_event *pev)
{
    static char buf[MAX_RX_BUFFER_SIZE];

    receive_data(pev, buf, sizeof(buf));
}

#define _SERV_DESC_CASE_RETURN(ID, _) \
        case (ID): return &_GET_TCP_SERV_NAME(ID);

static const struct net_socket_service_desc *get_net_socket_service(int index)
{
    if (index < 0 || index >= POLLABLE_SOCKETS) {
        return NULL;
    }
    switch (index) {
        LISTIFY(POLLABLE_SOCKETS, _SERV_DESC_CASE_RETURN, (;))
        default: return NULL;
    }
}

/**
 * @brief Get ethernet_if_socket_service via client
 * @param client The client socket
 * @return Pointer to the ethernet_if_socket_service structure
 */
static ethernet_if_socket_service_t *get_eth_socket_service_via_client(int client)
{
    for (int i = 0; i < POLLABLE_SOCKETS; i++) {
        if (socket_service_table[i]->poll_fds.fd == client) {
            return socket_service_table[i];
        }
    }
    return NULL;
}

static void receive_data(struct net_socket_service_event *pev,
            char *buf, size_t buf_len)
{
    struct zsock_pollfd *pfd = &pev->event;
    int client = pfd->fd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int rev_len;
    // get the socket service
    ethernet_if_socket_service_t *service = get_eth_socket_service_via_client(client);

    rev_len = zsock_recvfrom(client, buf, buf_len, 0,
            (struct sockaddr *)&addr, &addr_len);

    if (rev_len <= 0) {
        if (rev_len == 0) {
            LOG_WRN("Connection closed");
        } else {
            LOG_ERR("Receive error: %d", -errno);
        }
        // unregister the socket service
        int ret = unregister_client_at_socket_service(client);
        if (ret < 0) {
            LOG_ERR("Failed to unregister socket service: %d", ret);
        }

        // destroy the socket service thread
        close_socket_service(service);
        // close the socket
        zsock_close(client);
        LOG_INF("Connection to %s closed", inet_ntoa(addr.sin_addr));
    } else {
        LOG_DBG("Received message: %.*s", rev_len, buf);
        if (service == NULL) {
            LOG_ERR("Failed to get socket service");
            return;
        }
        // append the received data to the rx buffer
        utils_append_to_buffer(service->service_context.rx_buffer, buf, rev_len);
        // send event to notify the API task to process the data
        k_event_post(&apiNewDataEvent, service->service_context.event);
    }
}

/**
 * @brief Register a client at a socket service
 */
int tcp_server_init(void)
{
    // wait for settings loaded event
    k_event_wait(&settingsLoadedEvent, SETTINGS_LOADED_EVENT, false, K_FOREVER);

    int ret = 0;
    static int counter = 0;
    struct sockaddr_in addr_ipv4 = {
        .sin_family = AF_INET,
        .sin_port = htons((DEFAULT_PORT + settings.tcp_port)),
        .sin_addr = {.s4_addr = {
            settings.ip_address_0,
            settings.ip_address_1,
            settings.ip_address_2,
            settings.ip_address_3
        }},
    };
    struct in_addr gateway = {
        .s4_addr = {
            settings.gateway_0,
            settings.gateway_1,
            settings.gateway_2,
            settings.gateway_3
        }
    };
    struct in_addr netmask = {
        .s4_addr = {
            settings.netmask_0,
            settings.netmask_1,
            settings.netmask_2,
            settings.netmask_3
        }
    };
    mac_addr[0] = settings.mac_address_0;
    mac_addr[1] = settings.mac_address_1;
    mac_addr[2] = settings.mac_address_2;
    mac_addr[3] = settings.mac_address_3;
    mac_addr[4] = settings.mac_address_4;
    mac_addr[5] = settings.mac_address_5;

    // initialize the user button
    if (!device_is_ready(user_button.port)) {
        LOG_ERR("User button device not ready");
        return -1;
    }
    if (gpio_pin_configure_dt(&user_button, GPIO_INPUT)) {
        LOG_ERR("Failed to configure user button");
        return -1;
    }
    // initialize the user LED
    if (!device_is_ready(green_led.port) ||
        !device_is_ready(blue_led.port) ||
        !device_is_ready(red_led.port)) {
        LOG_ERR("User LED device not ready");
        return -1;
    }
    if (gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE) ||
        gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE) ||
        gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE)) {
        LOG_ERR("Failed to configure user LED");
        return -1;
    }
    // set the user LED to off
    gpio_pin_set_dt(&green_led, 0);
    gpio_pin_set_dt(&blue_led, 0);
    gpio_pin_set_dt(&red_led, 0);

    // use default network settings if user button is pressed
    int elapsed_time_counter = 0;
    while (gpio_pin_get_dt(&user_button) == 0) {
        // wait for the user button to be released
        k_sleep(K_MSEC(100));
        elapsed_time_counter++;
        if (elapsed_time_counter > PRESS_MORE_THAN_100MS) {
            LOG_INF("Use default network settings");

            // ipv4 address
            addr_ipv4.sin_addr.s4_addr[0] = defaults.ip_address_0;
            addr_ipv4.sin_addr.s4_addr[1] = defaults.ip_address_1;
            addr_ipv4.sin_addr.s4_addr[2] = defaults.ip_address_2;
            addr_ipv4.sin_addr.s4_addr[3] = defaults.ip_address_3;
            // port
            addr_ipv4.sin_port = htons((DEFAULT_PORT + defaults.tcp_port));
            // netmask
            netmask.s4_addr[0] = defaults.netmask_0;
            netmask.s4_addr[1] = defaults.netmask_1;
            netmask.s4_addr[2] = defaults.netmask_2;
            netmask.s4_addr[3] = defaults.netmask_3;
            // gateway
            gateway.s4_addr[0] = defaults.gateway_0;
            gateway.s4_addr[1] = defaults.gateway_1;
            gateway.s4_addr[2] = defaults.gateway_2;
            gateway.s4_addr[3] = defaults.gateway_3;

            // turn on all user LEDs
            gpio_pin_set_dt(&green_led, 1);
            gpio_pin_set_dt(&blue_led, 1);
            gpio_pin_set_dt(&red_led, 1);

            break;
        }
    }

    // initialize the socket service table
    for (int i = 0; i < POLLABLE_SOCKETS; i++) {
        ethernet_if_socket_service_t *service = malloc(sizeof(ethernet_if_socket_service_t));
        // reset the socket service
        memset(service, 0, sizeof(ethernet_if_socket_service_t));
        if (service == NULL) {
            LOG_ERR("Failed to allocate memory for socket service");
            return -ENOMEM;
        }
        service->poll_fds.fd = -1; // initially invalid
        service->service = get_net_socket_service(i);
        service->service_context.rx_buffer = malloc(sizeof(utils_ring_buffer_t));
        if (service->service_context.rx_buffer == NULL) {
            LOG_ERR("Failed to allocate memory for rx buffer");
            free(service);
            return -ENOMEM;
        }
        service->service_context.rx_buffer->buffer = NULL;
        service->service_context.event =(1 << i);
        service->service_context.response_cb = (api_response_callback_t)&ethernet_if_respond_handler;
        service->service_context.response_cb_bytes = (api_response_callback_t)&ethernet_if_respond_raw_bytes_handler;
        service->service_context.user_data = service;
        service->stack = socket_service_stack_pool[i];
        // add the service to the table
        socket_service_table[i] = service;
    }

    // get default network interface
    struct net_if *iface = net_if_get_default();
    struct net_if_addr *ifaddr;

    if (iface == NULL) {
        LOG_ERR("No default network interface found");
        return -1;
    }

    // set the interface address
    ifaddr = net_if_ipv4_addr_add(iface, &addr_ipv4.sin_addr, NET_ADDR_MANUAL, 0);
    // set netmask
    net_if_ipv4_set_netmask_by_addr(iface, &addr_ipv4.sin_addr, &netmask);
    // set gateway
    net_if_ipv4_set_gw(iface, &gateway);
    // set MAC address
    net_if_set_link_addr(iface, mac_addr, sizeof(mac_addr), NET_LINK_ETHERNET);
    
    // Create a TCP socket
    int sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Failed to create socket: %d", -errno);
        return -1;
    }

    // Check if the IPV6_V6ONLY option is set
    int opt;
    socklen_t optlen = sizeof(opt);
    ret = zsock_getsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, &optlen);
    if (ret ==0 && opt) {
        LOG_INF("IPV6_V6ONLY option is set, turning it off");
        // Turn off the IPV6_V6ONLY option
        opt = 0;
        ret = zsock_setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
        if (ret < 0) {
            LOG_ERR("Cannot turn off IPV6_V6ONLY option: %d", -errno);
            goto exit;
        } else {
            LOG_INF("IPV6_V6ONLY option is turned off");
        }
    }
    
    // set keepalive option
    int keepalive = 1;
    ret = zsock_setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    if (ret < 0) {
        LOG_ERR("Failed to set SO_KEEPALIVE option: %d", -errno);
        goto exit;
    }

    // Bind the socket to the address
    if ((ret = zsock_bind(sock, (struct sockaddr *)&addr_ipv4, sizeof(addr_ipv4))) < 0 ) {
        LOG_ERR("Failed to bind socket: %d", -errno);
        goto exit;
    }

    // Listen for incoming connections
    if ((ret = zsock_listen(sock, POLLABLE_SOCKETS)) < 0) {
        LOG_ERR("Failed to listen on socket: %d", -errno);
        goto exit;
    }

    LOG_INF("Listening on %s:%d",
            inet_ntoa(addr_ipv4.sin_addr),
            ntohs(addr_ipv4.sin_port));

    // get default network interface link address
    struct net_linkaddr *link_addr = net_if_get_link_addr(iface);

    LOG_INF("MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
            link_addr->addr[0],
            link_addr->addr[1],
            link_addr->addr[2],
            link_addr->addr[3],
            link_addr->addr[4],
            link_addr->addr[5]);

    // Accept incoming connections
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        thread_analyzer_print(0);
        int client = zsock_accept(sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client < 0) {
            LOG_ERR("Failed to accept connection: %d", -errno);
            continue;
        }

        counter++;
        inet_ntop(client_addr.sin_family, &client_addr.sin_addr, addr_str, sizeof(addr_str));
        LOG_INF("Accepted connection #%d from %s (%d)", counter, addr_str, client);

        // Register the client socket at a socket service
        ethernet_if_socket_service_t *service = register_client_at_socket_service(client);
        if (service == NULL) {
            LOG_ERR("Failed to register client at socket service");
            // thread_analyzer_print(0);
            zsock_close(client);
            continue;
        }
        service->poll_fds.events = POLLIN;
        service->poll_fds.revents = 0;
        
        // update socket service registration
        ret = net_socket_service_register(service->service, &service->poll_fds, 1, NULL);
        if (ret < 0) {
            LOG_ERR("Failed to register socket service: %d", ret);
            continue;
        }

        // create a thread for the socket service
        ret = create_socket_service_thread(service);
        if (ret < 0) {
            LOG_ERR("Failed to create socket service thread: %d", ret);
            unregister_client_at_socket_service(client);
            zsock_close(client);
            continue;
        }
        // send welcome message to the client
        const char *welcome_msg = "Welcome to Remote I/O!\r\n";
        ret = ethernet_if_send(service, "%s", welcome_msg);
        if (ret < 0) {
            LOG_ERR("Failed to send welcome message: %d", -errno);
            unregister_client_at_socket_service(client);
            close_socket_service(service);
            zsock_close(client);
            continue;
        }
    }

exit:
    // unregister all clients at a socket service
    ret = unregister_all_clients_at_socket_service();
    if (ret < 0) {
        LOG_ERR("Failed to unregister all clients at socket service: %d", ret);
    }

    // close the listening socket
    if (sock >= 0) {
        zsock_close(sock);
    }
    return ret;
}

static void reset_socket_service(ethernet_if_socket_service_t *service)
{
    if (service == NULL) {
        return;
    }
    // reset the socket service
    service->poll_fds.fd = -1; // mark it as unregistered
    service->poll_fds.events = 0;
    service->poll_fds.revents = 0;
    // free RX buffer
    if (service->service_context.rx_buffer->buffer != NULL) {
        free(service->service_context.rx_buffer->buffer);
        service->service_context.rx_buffer->buffer = NULL;
    }
    // reset the buffer pointers
    service->service_context.rx_buffer->head = 0;
    service->service_context.rx_buffer->tail = 0;
    service->service_context.rx_buffer->size = 0;

    return;
}

static int intialize_socket_service(ethernet_if_socket_service_t *service)
{
    if (service == NULL) {
        return -1;
    }
    // reset the socket service
    reset_socket_service(service);

    // allocate memory for RX buffer
    service->service_context.rx_buffer->buffer = malloc(MAX_RX_BUFFER_SIZE);
    if (service->service_context.rx_buffer->buffer == NULL) {
        LOG_ERR("Failed to allocate memory for RX buffer");
        return -1;
    }
    service->service_context.rx_buffer->size = MAX_RX_BUFFER_SIZE;

    return 0;
}

static int create_socket_service_thread(ethernet_if_socket_service_t *service)
{
    if (service == NULL) {
        return -1;
    }
    
    // crate the thread
    service->thread_id = k_thread_create(
        &service->thread,
        service->stack,
        CONFIG_REMOTEIO_SERVICE_STACK_SIZE,
        api_task,
        (void *)&service->service_context,
        NULL,
        NULL,
        CONFIG_REMOTEIO_SERVICE_PRIORITY,
        0, K_NO_WAIT);
    // set the thread name
    k_thread_name_set(service->thread_id, "socket_service_thread");
    
    return 0;
}

static int close_socket_service(ethernet_if_socket_service_t *service)
{
    if (service == NULL) {
        return -1;
    }
    // delete the thread
    k_thread_abort(service->thread_id);

    // unsubscribe all subscibed inputs
    digital_input_unsubscribe_all((void *)&service->service_context);

    return 0;
}

/**
 * @brief   register a client at a socket service atomically
 * @param   client  the client socket to register
 * @return  pointer to the socket service on success, NULL on failure
 */
static ethernet_if_socket_service_t *register_client_at_socket_service(int client)
{
    ethernet_if_socket_service_t *service = NULL;
    // lock the mutex
    k_mutex_lock(&lock, K_FOREVER);
    // retrieve the first available socket service from table
    for (int i = 0; i < POLLABLE_SOCKETS; i++) {
        if (socket_service_table[i]->poll_fds.fd == -1){
            service = socket_service_table[i];
            // initialize the socket service
            if (intialize_socket_service(service) < 0) {
                LOG_ERR("Failed to initialize socket service");
                service = NULL;
                goto exit;
            }
            service->poll_fds.fd = client; // mark it as registered
            break;
        }
    }

exit:
    // unlock the mutex
    k_mutex_unlock(&lock);
    return service;
}


/**
 * @brief   unregister a client at a socket service atomically
 * @param   client  the client socket to unregister
 * @return  0 on success, -1 on failure
 */
static int unregister_client_at_socket_service(int client)
{
    int ret = -1;
    // lock the mutex
    k_mutex_lock(&lock, K_FOREVER);
    // iterate through the socket service table
    for (int i = 0; i < POLLABLE_SOCKETS; i++) {
        if (socket_service_table[i]->poll_fds.fd == client) {
            // unregister the socket service
            ret = net_socket_service_unregister(socket_service_table[i]->service);
            if (ret < 0) {
                LOG_ERR("Failed to unregister socket service: %d", ret);
                break;
            }
            // reset the socket service
            reset_socket_service(socket_service_table[i]);
            ret = 0;
            break;
        }
    }
    // unlock the mutex
    k_mutex_unlock(&lock);

    return ret;
}

/**
 * @brief   unregister all clients at a socket service and close client socket atomically
 * @return  0 on success, -1 on failure
 */
static int unregister_all_clients_at_socket_service(void)
{
    int ret = -1;
    // lock the mutex
    k_mutex_lock(&lock, K_FOREVER);
    // iterate through the socket service table
    for (int i = 0; i < POLLABLE_SOCKETS; i++) {
        if (socket_service_table[i]->poll_fds.fd != -1) {
            // close the client socket
            int client = socket_service_table[i]->poll_fds.fd;
            zsock_close(client);
            LOG_INF("Closed client socket: %d", client);

            // reset the socket service
            reset_socket_service(socket_service_table[i]);

            // unregister the socket service
            ret = net_socket_service_unregister(socket_service_table[i]->service);
            if (ret < 0) {
                LOG_ERR("Failed to unregister socket service: %d", ret);
                break;
            }

            ret = 0;
        }
    }
    // unlock the mutex
    k_mutex_unlock(&lock);

    return ret;
}

int ethernet_if_send(ethernet_if_socket_service_t *service, const char *format_string, ...)
{
    if (service == NULL) {
        return -1;
    }

    char buffer[MAX_TX_BUFFER_SIZE] = {'\0'};
    va_list args;
    va_start(args, format_string);
    vsnprintf(buffer, sizeof(buffer), format_string, args);
    va_end(args);

    int ret = 0;

    // lock the mutex
    k_mutex_lock(&lock_sock_send, K_FOREVER);

    // check if the client is still connected
    if (service->poll_fds.fd == -1) {
        LOG_ERR("Client is not connected");
        ret = -1;
        goto exit;
    }
        
    // send data to the client
    ret = zsock_send(service->poll_fds.fd, buffer, strlen(buffer), 0);
    if (ret < 0) {
        LOG_ERR("Failed to send data: %d", -errno);
        ret = -1;
        goto exit;
    }

    LOG_DBG("Sent data: %s", buffer);

exit:
    // unlock the mutex
    k_mutex_unlock(&lock_sock_send);
    return ret;
}

int ethernet_if_send_raw_bytes(ethernet_if_socket_service_t *service, const uint8_t *buf, size_t len)
{
    if (service == NULL) {
        return -1;
    }

    int ret = 0;

    // lock the mutex
    k_mutex_lock(&lock_sock_send, K_FOREVER);

    // check if the client is still connected
    if (service->poll_fds.fd == -1) {
        LOG_ERR("Client is not connected");
        ret = -1;
        goto exit;
    }

    // send data to the client
    ret = zsock_send(service->poll_fds.fd, buf, len, 0);
    if (ret < 0) {
        LOG_ERR("Failed to send data: %d", -errno);
        goto exit;
    }
    LOG_DBG("Sent data: %.*s", len, buf);
exit:
    // unlock the mutex
    k_mutex_unlock(&lock_sock_send);
    return ret;
}

/**
 * @brief   Respond to the client with a formatted string
 * @param   service  pointer to the socket service
 * @param   format   format string
 * @return  void
 */
void ethernet_if_respond_handler(ethernet_if_socket_service_t *service, const char *format, ...)
{
    if (service == NULL) {
        return;
    }

    char buffer[MAX_TX_BUFFER_SIZE] = {'\0'};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    ethernet_if_send(service, "%s", buffer);
}

/**
 * @brief   Respond to the client with raw bytes
 * @param   service  pointer to the socket service
 * @param   buf      pointer to the buffer
 * @param   len      length of the buffer
 * @return  void
 */
void ethernet_if_respond_raw_bytes_handler(ethernet_if_socket_service_t *service, const uint8_t *buf, size_t len)
{
    if (service == NULL) {
        return;
    }

    ethernet_if_send_raw_bytes(service, buf, len);
}
