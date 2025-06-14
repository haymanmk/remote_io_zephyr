#ifndef __ETHERNET_IF_H__
#define __ETHERNET_IF_H__

#include <zephyr/net/socket_service.h>
#include <zephyr/net/socket_poll.h>
#include <zephyr/sys/util_macro.h>
#include "utils.h"
#include "api.h"

#define MAX_POLLABLE_SOCKETS 6
#define TCP_SERVICE_NAME tcp_service

// The index starts from 0 and goes up to MAX_POLLABLE_SOCKETS - 1
#define _GET_TCP_SERV_NAME(INDX) _CONCAT(_CONCAT(TCP_SERVICE_NAME, _), INDX)

// used by LISTIFY
#define _TCP_SERV_DEFINE(ID, HANDLER) \
        NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(_GET_TCP_SERV_NAME(ID), \
        HANDLER, 1)

// define n socket services
#define TCP_SERV_DEFINE_N(SERV_HANDLER, N) \
        LISTIFY(N, _TCP_SERV_DEFINE, (;), SERV_HANDLER)

// define ethernet interface events
#define ETHERNET_IF_EVENT_IPV4_CONNECTED    (1 << 0)
#define ETHERNET_IF_EVENT_READY             (1 << 1)

// define a struct to hold socket service information
typedef struct ethernet_if_socket_service {
        api_service_context_t service_context;
        struct zsock_pollfd poll_fds;
        const struct net_socket_service_desc *service;
        // pointer to the thread that is processing the socket service
        struct k_thread thread;
        // thread id
        k_tid_t thread_id;
        // pointer to the stack of the thread
        k_thread_stack_t *stack;
} ethernet_if_socket_service_t;


/* Function prototypes */
int ethernet_if_configure(void);
int tcp_server_init();
int ethernet_if_send(ethernet_if_socket_service_t *service, const char *format_string, ...);

#endif // __ETHERNET_IF_H__