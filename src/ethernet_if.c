#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ethernet_if, LOG_LEVEL_DBG);

#include <stdio.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket_service.h>

#include "inc/settings.h"

#define MAX_POLLABLE_SOCKETS 5
#define MAX_RCV_BUFFER_SIZE 1024

static struct pollfd poll_fds[MAX_POLLABLE_SOCKETS] = {
    .fd = -1, // initially invalid
};

static char addr_str[INET_ADDRSTRLEN];

static void receive_data(struct net_socket_service_event *pev, char *buf, size_t buf_len);

static void tcp_service_handler(struct net_socket_service_event *pev) {
    static char buf[MAX_RCV_BUFFER_SIZE];

    receive_data(pev, buf, sizeof(buf));
}

// Define socket service
NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(service_tcp, tcp_service_handler, MAX_POLLABLE_SOCKETS);

static void receive_data(struct net_socket_service_event *pev,
            char *buf, size_t buf_len)
{
    struct pollfd *pfd = &pev->event;
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
            LOG_ERR("Receive error: %d", errno);
        }

        // if the socket is closed, mark it as invalid
        for (int i = 0; i < MAX_POLLABLE_SOCKETS; i++) {
            if (poll_fds[i].fd == client) {
                poll_fds[i].fd = -1;
                break;
            }
        }

        // update socket service registration
        (void)net_socket_service_register(&service_tcp, poll_fds, ARRAY_SIZE(poll_fds), NULL);
        // close the socket
        zsock_close(client);
        LOG_INF("Connection to %s closed", inet_ntoa(addr.sin_addr));
    } else {
        LOG_INF("Received message: %.*s", rev_len, buf);
    }
}

int tcp_server_init(void)
{
    int ret = 0;
    static int counter = 0;
    struct sockaddr_in addr_ipv4 = {
        .sin_family = AF_INET,
        .sin_port = htons(settings.tcp_port),
        .sin_addr = {.s4_addr = [
            settings.ip_address_0,
            settings.ip_address_1,
            settings.ip_address_2,
            settings.ip_address_3
        ]},
    };
    
    // Create a TCP socket
    int sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Failed to create socket: %d", errno);
        return -1;
    }

    // Check if the IPV6_V6ONLY option is set
    int opt;
    ret = zsock_getsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
    if (ret ==0 && opt) {
        LOG_INF("IPV6_V6ONLY option is set, turning it off");
        // Turn off the IPV6_V6ONLY option
        opt = 0;
        ret = zsock_setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
        if (ret < 0) {
            LOG_ERR("Cannot turn off IPV6_V6ONLY option: %d", errno);
            goto exit;
        } else {
            LOG_INF("IPV6_V6ONLY option is turned off");
        }
    }
    }

    // Bind the socket to the address
    if (ret = zsock_bind(sock, (struct sockaddr *)&addr_ipv4, sizeof(addr_ipv4)) < 0 ) {
        LOG_ERR("Failed to bind socket: %d", errno);
        goto exit;
    }

    // Listen for incoming connections
    if (ret = zsock_listen(sock, 5) < 0) {
        LOG_ERR("Failed to listen on socket: %d", errno);
        goto exit;
    }

    LOG_INF("Listening on %d.%d.%d.%d:%d",
            settings.ip_address_0,
            settings.ip_address_1,
            settings.ip_address_2,
            settings.ip_address_3,
            settings.tcp_port);

    // Accept incoming connections
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client = zsock_accept(sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client < 0) {
            LOG_ERR("Failed to accept connection: %d", errno);
            continue;
        }

        counter++;
        inet_ntop(client.sin_family, &client_addr.sin_addr, addr_str, sizeof(addr_str));
        LOG_INF("Accepted connection #%d from %s (%d)", counter, addr, client);

        // Register the socket with the socket service
        for (int i = 0; i < MAX_POLLABLE_SOCKETS; i++) {
            if (poll_fds[i].fd == -1) {
                poll_fds[i].fd = client;
                poll_fds[i].events = POLLIN;
                break;
            }
        }

        // update socket service registration
        ret = net_socket_service_register(&service_tcp, poll_fds, ARRAY_SIZE(poll_fds), NULL);
        if (ret < 0) {
            LOG_ERR("Failed to register socket service: %d", ret);
            break;
        }
    }

exit:
    (void)net_socket_service_unregister(&service_tcp);

    if (sock >= 0) {
        zsock_close(sock);
    }
    return ret;
}
