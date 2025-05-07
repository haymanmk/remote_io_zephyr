#ifndef __ETHERNET_IF_H__
#define __ETHERNET_IF_H__

#include <zephyr/net/socket_service.h>
#include <zephyr/net/socket_poll.h>
#include "utils.h"

#define MAX_POLLABLE_SOCKETS 6
#define TCP_SERVICE_NAME tcp_service

// The index starts from 0 and goes up to MAX_POLLABLE_SOCKETS - 1
#define _GET_TCP_SERV_NAME(INDX) _CONCAT(_CONCAT(TCP_SERVICE_NAME, _), INDX)

// define 1 socket service
#define TCP_SERV_DEFINE_1(SERV_HANDLER) \
        NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(_GET_TCP_SERV_NAME(0), \
        SERV_HANDLER, 1)

// define 2 socket services
#define TCP_SERV_DEFINE_2(SERV_HANDLER) \
        TCP_SERV_DEFINE_1(SERV_HANDLER); \
        NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(_GET_TCP_SERV_NAME(1), \
        SERV_HANDLER, 1)

// define 3 socket services
#define TCP_SERV_DEFINE_3(SERV_HANDLER) \
        TCP_SERV_DEFINE_2(SERV_HANDLER); \
        NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(_GET_TCP_SERV_NAME(2), \
        SERV_HANDLER, 1)

// define 4 socket services
#define TCP_SERV_DEFINE_4(SERV_HANDLER) \
        TCP_SERV_DEFINE_3(SERV_HANDLER); \
        NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(_GET_TCP_SERV_NAME(3), \
        SERV_HANDLER, 1)

// define 5 socket services
#define TCP_SERV_DEFINE_5(SERV_HANDLER) \
        TCP_SERV_DEFINE_4(SERV_HANDLER); \
        NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(_GET_TCP_SERV_NAME(4), \
        SERV_HANDLER, 1)

// define 6 socket services
#define TCP_SERV_DEFINE_6(SERV_HANDLER) \
        TCP_SERV_DEFINE_5(SERV_HANDLER); \
        NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(_GET_TCP_SERV_NAME(5), \
        SERV_HANDLER, 1)

// define n socket services
#define TCP_SERV_DEFINE_N(SERV_HANDLER, N) \
        _CONCAT(TCP_SERV_DEFINE_, N)(SERV_HANDLER)

// get new data event
#define GET_NEW_DATA_EVENT(INDX) \
        INDX == 0 ? API_EVENT_SERV_0 : \
        INDX == 1 ? API_EVENT_SERV_1 : \
        INDX == 2 ? API_EVENT_SERV_2 : \
        INDX == 3 ? API_EVENT_SERV_3 : \
        INDX == 4 ? API_EVENT_SERV_4 : \
        INDX == 5 ? API_EVENT_SERV_5 : \
        0

// define a struct to hold socket service information
typedef struct ethernet_if_socket_service {
        struct zsock_pollfd poll_fds;
        const struct net_socket_service_desc *service;
        // rx ring buffer
        struct UtilsRingBuffer *rx_buffer;
        // pointer to the thread that is processing the socket service
        struct k_thread thread;
        // thread id
        k_tid_t thread_id;
        // pointer to the stack of the thread
        k_thread_stack_t *stack;
        // event for receiving new data
        uint32_t event;
} ethernet_if_socket_service_t;


/* Function prototypes */
int tcp_server_init();
int ethernet_if_send(ethernet_if_socket_service_t *service, const char *format_string, ...);

#endif // __ETHERNET_IF_H__