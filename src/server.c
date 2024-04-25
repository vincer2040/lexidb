#include "server.h"
#include "ev.h"
#include "util.h"
#include "vnet.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* global server */
lexi_server server = {0};

static void server_free(void);
static void free_user_in_vec(void* ptr);
static void sigint_handler(int mode);

static int server_init(int argc, char** argv) {
    if (create_signal_handler(sigint_handler, SIGINT) == -1) {
        return -1;
    }
    server.pid = getpid();
    server.thread_id = pthread_self();
    if (argc > 1) {
        char* path_to_config = argv[1];
        server.config_file = get_real_path(path_to_config);
    } else {
        server.config_file = get_real_path("../lexi.conf");
    }
    if (server.config_file == NULL) {
        return -1;
    }
    server.executable = get_executable_path();
    if (server.executable == NULL) {
        free(server.config_file);
        return -1;
    }
    server.argc = argc;
    server.argv = argv;
    server.users = vec_new(sizeof(user));
    if (server.users == NULL) {
        free(server.config_file);
        free(server.executable);
        return -1;
    }
    server.clients = vec_new(sizeof(client));
    if (server.clients == NULL) {
        free(server.config_file);
        free(server.executable);
        vec_free(server.users, NULL);
        return -1;
    }
    server.sfd = -1;
    init_vmap_type();
    return 0;
}

static int server_configure(void) {
    char* config_file_contents;
    ssize_t config_file_contents_len = 0;
    int config_res;
    vstr error;
    config_file_contents = read_file(server.config_file, &config_file_contents_len);
    if (config_file_contents == NULL) {
        return -1;
    }
    config_res = configure_server(&server, config_file_contents, config_file_contents_len, &error);
    if (config_res == -1) {
        printf("config error: %s\n", vstr_data(&error));
        vstr_free(&error);
        return -1;
    }
    free(config_file_contents);
    return 0;
}

static int server_init_databases(void) {
    uint64_t i;
    vmap_type* vt = get_vmap_type();
    server.databases = calloc(server.num_databases, sizeof(lexi_db));
    if (server.databases == NULL) {
        return -1;
    }
    for (i = 0; i < server.num_databases; ++i) {
        server.databases[i].id = i;
        server.databases[i].keys = vmap_new(vt);
        if (server.databases[i].keys == NULL) {
            // set this so that we don't free unallocated
            server.num_databases = i;
            return -1;
        }
    }
    return 0;
}

int server_run(int argc, char** argv) {
    int init;
    init = server_init(argc, argv);
    if (init == -1) {
        return -1;
    }
    init = server_configure();
    if (init == -1) {
        return -1;
    }

    init = server_init_databases();
    if (init == -1) {
        server_free();
        return -1;
    }

    server.sfd = vnet_tcp_server(vstr_data(&server.bind_addr), server.port, server.tcp_backlog);
    if (server.sfd == -1) {
        server_free();
        return -1;
    }

    server.ev = ev_new(server.max_clients);
    if (server.ev == NULL) {
        server_free();
        return -1;
    }

    ev_await(server.ev);

    server_free();
    return 0;
}

static void server_free(void) {
    free(server.config_file);
    free(server.executable);
    vstr_free(&server.bind_addr);
    vec_free(server.users, free_user_in_vec);
    vec_free(server.clients, NULL);
    if (server.logfile) {
        free(server.logfile);
    }
    if (server.ev) {
        ev_free(server.ev);
    }
    if (server.databases) {
        uint64_t i;
        for (i = 0; i < server.num_databases; ++i) {
            vmap_free(server.databases[i].keys);
        }
        free(server.databases);
    }
    if (server.sfd != -1) {
        close(server.sfd);
    }
}

static void free_user_in_vec(void* ptr) {
    user* user = ptr;
    vec_free(user->commands, NULL);
    vec_free(user->categories, NULL);
    vec_free(user->passwords, free_vstr_in_vec);
    vstr_free(&user->username);
}

static void sigint_handler(int mode) {
    UNUSED(mode);
    server.shutdown = 1;
}
