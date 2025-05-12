#include "ethernet_if.h"
#include "digital_input.h"

int main(void) {
    // initialize digital input
    digital_input_init();
    tcp_server_init();

    return 0;
}