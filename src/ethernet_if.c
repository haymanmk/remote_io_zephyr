#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ethernet_if, LOG_LEVEL_DBG);

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/posix/poll.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/arpa/inet.h>

#include "settings.h"
#include "ethernet_if.h"
#include "api.h"

// extern settings_t settings;


#define MAX_RCV_BUFFER_SIZE 1024
#define POLLABLE_SOCKETS 5

#if defined(CONFIG_NET_MAX_CONTEXTS)
/* POLLABLE_SOCKETS must be less than CONFIG_NET_MAX_CONTEXTS */
_Static_assert(POLLABLE_SOCKETS < CONFIG_NET_MAX_CONTEXTS,
            "POLLABLE_SOCKETS must be less than CONFIG_NET_MAX_CONTEXTS=" STRINGIFY(CONFIG_NET_MAX_CONTEXTS));
#endif

/* Local function prototypes */
static void receive_data(struct net_socket_service_event *pev, char *buf, size_t buf_len);
static void tcp_service_handler(struct net_socket_service_event *pev);
static struct ethernet_if_socket_service *register_client_at_socket_service(int client);
static int unregister_client_at_socket_service(int client);
static int unregister_all_clients_at_socket_service(void);


// define the locking mechanism
static K_MUTEX_DEFINE(lock);

static char addr_str[INET_ADDRSTRLEN];

// Define socket services
TCP_SERV_DEFINE_N(tcp_service_handler, POLLABLE_SOCKETS);

// define a table to hold the socket services
static struct ethernet_if_socket_service *socket_service_table[POLLABLE_SOCKETS];


/* Functions */

static void tcp_service_handler(struct net_socket_service_event *pev)
{
    static char buf[MAX_RCV_BUFFER_SIZE];

    receive_data(pev, buf, sizeof(buf));
}

static const struct net_socket_service_desc *get_socket_service(int index)
{
    if (index < 0 || index >= POLLABLE_SOCKETS) {
        return NULL;
    }
    switch (index) {
        case 0: return &_GET_TCP_SERV_NAME(0);
    #if POLLABLE_SOCKETS > 1
        case 1: return &_GET_TCP_SERV_NAME(1);
    #endif
    #if POLLABLE_SOCKETS > 2
        case 2: return &_GET_TCP_SERV_NAME(2);
    #endif
    #if POLLABLE_SOCKETS > 3
        case 3: return &_GET_TCP_SERV_NAME(3);
    #endif
    #if POLLABLE_SOCKETS > 4
        case 4: return &_GET_TCP_SERV_NAME(4);
    #endif
    #if POLLABLE_SOCKETS > 5
        case 5: return &_GET_TCP_SERV_NAME(5);
    #endif
        default: return NULL;
    }
}

static void receive_data(struct net_socket_service_event *pev,
            char *buf, size_t buf_len)
{
    struct zsock_pollfd *pfd = &pev->event;
    int client = pfd->fd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int rev_len;

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

        // close the socket
        zsock_close(client);
        LOG_INF("Connection to %s closed", inet_ntoa(addr.sin_addr));
    } else {
        LOG_INF("Received message: %.*s", rev_len, buf);
        api_append_to_rx_ring_buffer(buf, rev_len);
    }
}

int tcp_server_init(void)
{
    int ret = 0;
    static int counter = 0;
    struct sockaddr_in addr_ipv4 = {
        .sin_family = AF_INET,
        // .sin_port = htons(settings.tcp_port),
        // .sin_addr = {.s4_addr = {
        //     settings.ip_address_0,
        //     settings.ip_address_1,
        //     settings.ip_address_2,
        //     settings.ip_address_3
        // }},
        .sin_port = htons(8500),
        .sin_addr = {
            .s4_addr = {192, 168, 1, 10}
        }
    };

    // initialize the socket service table
    for (int i = 0; i < POLLABLE_SOCKETS; i++) {
        struct ethernet_if_socket_service *service = malloc(sizeof(struct ethernet_if_socket_service));
        if (service == NULL) {
            LOG_ERR("Failed to allocate memory for socket service");
            return -ENOMEM;
        }
        service->poll_fds.fd = -1; // initially invalid
        service->service = get_socket_service(i);
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
    // LOG_INF("Listening on %d.%d.%d.%d:%d",
    //         settings.ip_address_0,
    //         settings.ip_address_1,
    //         settings.ip_address_2,
    //         settings.ip_address_3,
    //         settings.tcp_port);

    // Accept incoming connections
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client = zsock_accept(sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client < 0) {
            LOG_ERR("Failed to accept connection: %d", -errno);
            continue;
        }

        counter++;
        inet_ntop(client_addr.sin_family, &client_addr.sin_addr, addr_str, sizeof(addr_str));
        LOG_INF("Accepted connection #%d from %s (%d)", counter, addr_str, client);

        // Register the client socket at a socket service
        struct ethernet_if_socket_service *service = register_client_at_socket_service(client);
        if (service == NULL) {
            LOG_ERR("Failed to register client at socket service");
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

        // send welcome message to the client
        const char *welcome_msg = "Welcome to the server\r\n";
        ret = zsock_send(client, welcome_msg, strlen(welcome_msg), 0);
        if (ret < 0) {
            LOG_ERR("Failed to send welcome message: %d", -errno);
            unregister_client_at_socket_service(client);
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

/**
 * @brief   register a client at a socket service atomically
 * @param   client  the client socket to register
 * @return  pointer to the socket service on success, NULL on failure
 */
static struct ethernet_if_socket_service *register_client_at_socket_service(int client)
{
    struct ethernet_if_socket_service *service = NULL;
    // lock the mutex
    k_mutex_lock(&lock, K_FOREVER);
    // retrieve the first available socket service from table
    for (int i = 0; i < POLLABLE_SOCKETS; i++) {
        if (socket_service_table[i]->poll_fds.fd == -1){
            service = socket_service_table[i];
            service->poll_fds.fd = client; // mark it as registered
            service->rx_buffer = malloc(MAX_RCV_BUFFER_SIZE);
            if (service->rx_buffer == NULL) {
                LOG_ERR("Failed to allocate memory for socket service RX buffer");
                k_mutex_unlock(&lock);
                return NULL;
            }
            service->rx_buffer_head = 0;
            service->rx_buffer_tail = 0;
            service->tx_buffer = malloc(MAX_RCV_BUFFER_SIZE);
            if (service->tx_buffer == NULL) {
                LOG_ERR("Failed to allocate memory for socket service TX buffer");
                free(service->rx_buffer);
                service->rx_buffer = NULL;
                k_mutex_unlock(&lock);
                return NULL;
            }
            service->tx_buffer_head = 0;
            service->tx_buffer_tail = 0;
            break;
        }
    }

    // unlock the mutex
    k_mutex_unlock(&lock);
    return service;
}

static void reset_socket_service(struct ethernet_if_socket_service *service)
{
    if (service == NULL) {
        return;
    }
    // reset the socket service
    service->poll_fds.fd = -1; // mark it as unregistered
    service->poll_fds.events = 0;
    service->poll_fds.revents = 0;
    // free RX buffer
    if (service->rx_buffer != NULL) {
        free(service->rx_buffer);
        service->rx_buffer = NULL;
    }
    // free TX buffer
    if (service->tx_buffer != NULL) {
        free(service->tx_buffer);
        service->tx_buffer = NULL;
    }
    // reset the buffer pointers
    service->rx_buffer_head = 0;
    service->rx_buffer_tail = 0;
    service->tx_buffer_head = 0;
    service->tx_buffer_tail = 0;

    return;
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