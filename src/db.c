#include "config.h"
#include "server.h"
#include "util.h"

#define ADDR "127.0.0.1"
#define PORT 6969

int main(int argc, char** argv) {

    int default_port = PORT;

    // if (done_from_args(argc, argv) == 1) {
    //     return 0;
    // }

    // configure_from_args(argc, argv);

    // server(ADDR, PORT);

    Configuration* config = config_new();

    config_add_option(&config, "--address", "-a", COT_STRING, ADDR,
                      "the address to listen on");
    config_add_option(&config, "--port", "-p", COT_INT, &default_port,
                      "the port to listen on");
    config_add_option(&config, "--loglevel", "-ll", COT_STRING, "none",
                      "the amount to log");
    config_add_option(&config, "--logfile", "-lf", COT_STRING, "lexi.log",
                      "the file to log to");

    configure(config, argc, argv);

    config_free(config);

    return 0;
}
