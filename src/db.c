#include "config.h"
#include "server.h"
#include "util.h"

#define ADDR "127.0.0.1"
#define PORT 6969

int main(int argc, char** argv) {

    int default_port = PORT;
    Ht* args;

    Configuration* config = config_new();

    config_add_option(&config, "--address", "-a", COT_STRING, ADDR,
                      "the address to listen on");
    config_add_option(&config, "--port", "-p", COT_INT, &default_port,
                      "the port to listen on");
    config_add_option(&config, "--loglevel", "-ll", COT_STRING, "none",
                      "the amount to log");
    config_add_option(&config, "--logfile", "-lf", COT_STRING, "lexi.log",
                      "the file to log to");

    args = configure(config, argc, argv);

    if (args) {
        Object* port = ht_get(args, (uint8_t*)"--port", 6);
        Object* addr = ht_get(args, (uint8_t*)"--address", 9);
        server(addr->data.str, (uint16_t)port->data.i64);

        config_free(config);
        free_configuration_ht(args);

        return 0;
    } else {
        config_free(config);
        return 1;
    }
}
