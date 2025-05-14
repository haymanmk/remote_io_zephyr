#include "ethernet_if.h"
#include "digital_input.h"
#include "flash.h"
#include "settings.h"

int main(void) {
    // initialize flash memory
    flash_init();

    // initialize settings
    settings_init();

    // initialize digital input
    digital_input_init();

    tcp_server_init();

    return 0;
}