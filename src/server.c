#include "server.h"
#include "cmd.h"
#include "ev.h"
#include "parser.h"
#include "util.h"
#include "vnet.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* global server */
lexi_server server = {0};

static void server_accept(ev* ev, int fd, void* client_data, int mask);
static void server_free(void);
static int find_default_user_in_vec(const void* cmp, const void* el);
static void free_user_in_vec(void* ptr);
static void free_client_in_vec(void* ptr);
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
    server.clients = vec_new(sizeof(client*));
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
    config_file_contents =
        read_file(server.config_file, &config_file_contents_len);
    if (config_file_contents == NULL) {
        return -1;
    }
    config_res = configure_server(&server, config_file_contents,
                                  config_file_contents_len, &error);
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

static int server_create(int argc, char** argv) {
    int res;

    res = server_init(argc, argv);
    if (res == -1) {
        return -1;
    }

    res = server_configure();
    if (res == -1) {
        return -1;
    }

    res = server_init_databases();
    if (res == -1) {
        return -1;
    }

    server.sfd = vnet_tcp_server(vstr_data(&server.bind_addr), server.port,
                                 server.tcp_backlog);
    if (server.sfd == -1) {
        return -1;
    }

    server.ev = ev_new(server.max_clients);
    if (server.ev == NULL) {
        return -1;
    }

    return 0;
}

void server_log(log_level level, const char* buf, size_t buf_len) {
    static int file;
    static int opened_file = 0;
    if (level > server.loglevel) {
        return;
    }
    if (!opened_file) {
        if (server.logfile != NULL) {
            file = open(server.logfile, O_CREAT | O_RDWR);
            if (file == -1) {
            }
        } else {
            file = STDOUT_FILENO;
        }
        opened_file = 1;
    }
    if (buf == NULL) {
        close(file);
        return;
    }
    write(file, buf, buf_len);
}

int server_run(int argc, char** argv) {
    int init = server_create(argc, argv);
    vstr log_msg;
    if (init == -1) {
        server_free();
        return -1;
    }

    if (ev_add_event(server.ev, server.sfd, EV_READ, server_accept, NULL) ==
        -1) {
        server_free();
        return -1;
    }

    ev_await(server.ev);

    log_msg = vstr_format("%s received. shutting down\n",
                          server.signal_received == SIGINT ? "sigint"
                                                           : "unknown signal");
    server_log(LL_Info, vstr_data(&log_msg), vstr_len(&log_msg));
    vstr_free(&log_msg);

    server_free();
    return 0;
}

static int client_can_execute_command(client* client, cmd_type type,
                                      category cat) {
    vec_iter category_iter, cmd_type_iter;
    user* user = client->user;
    category_iter = vec_iter_new(user->categories);
    while (category_iter.cur) {
        const category* cur_cat = category_iter.cur;
        if (*cur_cat == cat || *cur_cat == C_All) {
            return 1;
        }
        vec_iter_next(&category_iter);
    }
    cmd_type_iter = vec_iter_new(user->commands);
    while (cmd_type_iter.cur) {
        const cmd_type* cur_type = cmd_type_iter.cur;
        if (*cur_type == type) {
            return 1;
        }
        vec_iter_next(&cmd_type_iter);
    }
    return 0;
}

static void execute_command(client* client, cmd* cmd) {
    int proc_res;
    if (!client_can_execute_command(client, cmd->type, cmd->cat) &&
        server.protected_mode) {
        client_add_reply_no_access(client);
        cmd_free_full(cmd);
        return;
    }
    proc_res = cmd->proc(client, cmd);
    if (proc_res == -1) {
        cmd_free_full(cmd);
        return;
    }
    cmd_free(cmd);
}

static void handle_client_req(client* client) {
    parser p = parser_new(client->read_buf, client->read_buf_pos,
                          client->protocol_version);
    cmd cmd = parse_cmd(&p);
    if (parser_has_error(&p)) {
        vstr err = parser_get_error(&p);
        if (vstr_has_retcar_or_newline(&err)) {
            client_add_reply_bulk_error(client, &err);
        } else {
            client_add_reply_simple_error(client, &err);
        }
        vstr_free(&err);
        return;
    }
    execute_command(client, &cmd);
}

static void write_to_client(ev* ev, int fd, void* client_data, int mask) {
    client* client = client_data;
    int write_res = client_write(client);
    if (write_res == -1) {
        vstr err = format_err("failed to write to client %s (errno %d) %s\n",
                              client->ip, client->conn->last_errno,
                              strerror(client->conn->last_errno));
        server_log(LL_Info, vstr_data(&err), vstr_len(&err));
        vstr_free(&err);
    }
    ev_delete_event(ev, fd, EV_WRITE);
}

static void read_from_client(ev* ev, int fd, void* client_data, int mask) {
    client* client = client_data;
    int read_res = client_read(client);
    vstr log_msg;

    if (read_res == -1) {
        vstr err = format_err("failed to read from client %s (errno %d) %s\n",
                              client->ip, client->conn->last_errno,
                              strerror(client->conn->last_errno));
        server_log(LL_Info, vstr_data(&err), vstr_len(&err));
        vstr_free(&err);
        client_close(client);
        return;
    }

    if (client->conn->state == CS_Closed) {
        log_msg = vstr_format("closing %s\n", client->ip);
        server_log(LL_Info, vstr_data(&log_msg), vstr_len(&log_msg));
        vstr_free(&log_msg);
        client_close(client);
        return;
    }

    server_log(LL_Info, (const char*)client->read_buf, client->read_buf_pos);

    handle_client_req(client);

    memset(client->read_buf, 0, client->read_buf_pos);
    client->read_buf_pos = 0;

    ev_add_event(ev, fd, EV_WRITE, write_to_client, client);
    UNUSED(mask);
}

static void server_accept(ev* ev, int fd, void* client_data, int mask) {
    int cfd;
    int port = 0;
    char addr[BIND_ADDR_MAX] = {0};
    vstr log_msg;
    connection* conn;
    client* client;
    user* default_user;
    int push_res;

    cfd = vnet_accept(fd, addr, sizeof addr, &port);
    if (cfd == -1) {
        vstr err = format_err("failed to accept (errno: %d) %s\n", errno,
                              strerror(errno));
        server_log(LL_Info, vstr_data(&err), vstr_len(&err));
        vstr_free(&err);
        return;
    }

    conn = connection_new(cfd);
    if (conn == NULL) {
        vstr err = format_err("oom creating connection for %s\n", addr);
        server_log(LL_Info, vstr_data(&err), vstr_len(&err));
        vstr_free(&err);
        abort();
    }

    client = client_new(conn);
    if (conn == NULL) {
        vstr err = format_err("oom creating client for %s\n", addr);
        server_log(LL_Info, vstr_data(&err), vstr_len(&err));
        vstr_free(&err);
        abort();
    }

    memcpy(client->ip, addr, sizeof addr);
    client->port = port;

    default_user =
        (user*)vec_find(server.users, NULL, find_default_user_in_vec);
    if (default_user == NULL) {
        server_log(LL_Info, "failed to find default user in user vec\n", 40);
        abort();
    }

    client->user = default_user;

    push_res = vec_push(&server.clients, &client);
    if (push_res == -1) {
        vstr err = format_err("unable to push client %s to client vec\n", addr);
        server_log(LL_Info, vstr_data(&err), vstr_len(&err));
        vstr_free(&err);
        abort();
    }

    client->conn->state = CS_Connected;

    log_msg = vstr_format("accepted connection fd: %d ip: %s port: %d\n", cfd,
                          addr, port);
    server_log(LL_Info, vstr_data(&log_msg), vstr_len(&log_msg));
    vstr_free(&log_msg);

    ev_add_event(ev, cfd, EV_READ, read_from_client, client);
    UNUSED(mask);
    UNUSED(client_data);
}

static void server_free(void) {
    free(server.config_file);
    free(server.executable);
    vstr_free(&server.bind_addr);
    vec_free(server.users, free_user_in_vec);
    vec_free(server.clients, free_client_in_vec);
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
    // close the logfile
    server_log(LL_Nothing, NULL, 0);
}

static int find_default_user_in_vec(const void* cmp, const void* el) {
    const user* user = el;
    UNUSED(cmp);
    if (user->flags & USER_DEFAULT) {
        return 0;
    }
    return 1;
}

static void free_user_in_vec(void* ptr) {
    user* user = ptr;
    vec_free(user->commands, NULL);
    vec_free(user->categories, NULL);
    vec_free(user->passwords, free_vstr_in_vec);
    vstr_free(&user->username);
}

static void free_client_in_vec(void* ptr) {
    client* c = *((client**)ptr);
    free(c->read_buf);
    free(c->conn);
    free(c);
}

static void sigint_handler(int mode) {
    UNUSED(mode);
    server.shutdown = 1;
    server.signal_received = SIGINT;
}
