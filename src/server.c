#include "server.h"
#include "builder.h"
#include "clap.h"
#include "cmd.h"
#include "cmd_help.c"
#include "config.h"
#include "config_parser.h"
#include "ev.h"
#include "ht.h"
#include "log.h"
#include "networking.h"
#include "object.h"
#include "parser.h"
#include "result.h"
#include "set.h"
#include "util.h"
#include "vec.h"
#include "vstr.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_ADDRESS "127.0.0.1"
#define DEFAULT_PORT 6969
#define SERVER_BACKLOG 10
#define CLIENT_READ_BUF_CAP 4096

typedef client* client_ptr;

result_t(server, vstr);
result_t(client_ptr, vstr);
result_t(object, void*);
result_t(log_level, vstr);

typedef struct {
    const char* str;
    log_level ll;
} log_level_lookup;

static result(server) server_new(const char* addr, uint16_t port, log_level ll,
                                 vstr* conf_file_path);
static int add_users(vec** users_vec, vec* users_from_config);
static lexidb* lexidb_new(void);
static void server_free(server* s);
static void lexidb_free(lexidb* db);

static void server_accept(ev* ev, int fd, void* client_data, int mask);
static void read_from_client(ev* ev, int fd, void* client_data, int mask);
static void write_to_client(ev* ev, int fd, void* client_data, int mask);

static result(client_ptr) create_client(int fd, uint32_t addr, uint16_t port);

static void execute_cmd(server* s, client* c);
static int execute_auth_command(server* s, client* client, auth_cmd* auth);
static ht_result execute_set_command(server* s, set_cmd* set);
static object* execute_get_command(server* s, get_cmd* get);
static ht_result execute_del_command(server* s, del_cmd* del);
static int execute_push_command(server* s, push_cmd* push);
static result(object) execute_pop_command(server* s);
static int execute_enque_command(server* s, enque_cmd* enque);
static result(object) execute_deque_command(server* s);
static set_result execute_zset_command(server* s, zset_cmd* zset);
static bool execute_zhas_command(server* s, zhas_cmd* zhas);
static set_result execute_zdel_command(server* s, zdel_cmd* zdel);

static result(log_level) determine_loglevel(vstr* loglevel_s);

static int realloc_client_read_buf(client* c);

static int server_compare_objects(void* a, void* b);
static int client_compare(void* fdp, void* clientp);
static int user_compare(void* a, void* b);
static int user_compare_by_username(void* a, void* b);

static void cmd_free(cmd* cmd);
static void client_free(client* client);
static void server_free_object(void* ptr);
static void client_in_vec_free(void* ptr);
static void user_in_vec_free(void* ptr);

const log_level_lookup log_level_lookups[] = {
    {"none", None},
    {"info", Info},
    {"debug", Debug},
    {"verbose", Verbose},
};

int server_run(int argc, char* argv[]) {
    result(server) rs;
    server s;
    args args;
    cla cla;
    object* addr_obj;
    object* port_obj;
    object* log_level_obj;
    object* conf_file_path_obj;
    const char* addr;
    uint16_t port;
    log_level ll;
    vstr conf_file_path;

    if (create_sigint_handler() == -1) {
        error("failed to create sigint handler (errno: %d) %s\n", errno,
              strerror(errno));
        return 1;
    }

    args = args_new();
    args_add(&args, "config file path", "-c", "--config",
             "the path to the configuration file to configure lexidb", String);
    args_add(&args, "address", "-a", "--address",
             "the address the server should listen on", String);
    args_add(&args, "port", "-p", "--port",
             "the port the server should listen on", Int);
    args_add(&args, "loglevel", "-ll", "--loglevel",
             "the amount to log (none, info, debug, verbose)", String);
    cla = parse_cmd_line_args(args, argc, argv);

    if (clap_has_error(&cla)) {
        const char* err = clap_error(&cla);
        error("clap error: %s\n", err);
        args_free(args);
        clap_free(&cla);
        return 0;
    }

    if (clap_version_requested(&cla)) {
        printf("v%d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
        args_free(args);
        clap_free(&cla);
        return 0;
    }

    if (clap_help_requested(&cla)) {
        args_free(args);
        clap_free(&cla);
        return 0;
    }

    conf_file_path_obj = args_get(&cla, "config file path");
    if (conf_file_path_obj != NULL) {
        const char* path = vstr_data(&conf_file_path_obj->data.string);
        char* real_path = realpath(path, NULL);
        size_t real_path_len;
        if (real_path == NULL) {
            error("configuration file path (%s) is invalid (errno: %d) %s\n",
                  path, errno, strerror(errno));
            args_free(args);
            clap_free(&cla);
            return 1;
        }
        real_path_len = strlen(real_path);
        conf_file_path = vstr_from_len(real_path, real_path_len);
        free(real_path);
    } else {
        char* real_path = realpath("../lexi.conf", NULL);
        size_t real_path_len = strlen(real_path);
        if (real_path == NULL) {
            error("configuration file (%s) path is invalid (errno: %d) %s\n",
                  "../lexi.conf", errno, strerror(errno));
            args_free(args);
            clap_free(&cla);
            return 1;
        }
        conf_file_path = vstr_from_len(real_path, real_path_len);
        free(real_path);
    }

    addr_obj = args_get(&cla, "address");
    if (addr_obj != NULL) {
        addr = vstr_data(&addr_obj->data.string);
    } else {
        addr = DEFAULT_ADDRESS;
    }

    port_obj = args_get(&cla, "port");
    if (port_obj != NULL) {
        port = port_obj->data.num;
    } else {
        port = DEFAULT_PORT;
    }

    log_level_obj = args_get(&cla, "loglevel");
    if (log_level_obj != NULL) {
        result(log_level) ll_res =
            determine_loglevel(&(log_level_obj->data.string));
        if (ll_res.type == Err) {
            error("%s %s\n", vstr_data(&ll_res.data.err),
                  vstr_data(&log_level_obj->data.string));
            args_free(args);
            clap_free(&cla);
            return 1;
        }
        ll = ll_res.data.ok;
    } else {
        ll = Info;
    }

    rs = server_new(addr, port, ll, &conf_file_path);

    args_free(args);
    clap_free(&cla);

    if (rs.type == Err) {
        error("%s\n", vstr_data(&(rs.data.err)));
        vstr_free(&conf_file_path);
        vstr_free(&(rs.data.err));
        return 1;
    }

    s = rs.data.ok;

    if (s.log_level >= Info) {
        info("listening on %s:%u\n", vstr_data(&s.addr), s.port);
    }

    ev_await(s.ev);

    if (s.log_level >= Info) {
        info("server shutting down\n");
    }

    server_free(&s);

    return 0;
}

static result(server) server_new(const char* addr, uint16_t port, log_level ll,
                                 vstr* conf_file_path) {
    result(server) result = {0};
    server s = {0};
    int sfd = create_tcp_socket(1);
    ev* ev;
    vstr addr_str;
    result(vstr) config_file_contents_res;
    result(ht) config_from_file_res;
    object* users_from_config;
    line_data_type user_type = User;
    int add_users_res;

    if (sfd < 0) {
        result.type = Err;
        result.data.err = vstr_format("failed to create socket (errno: %d) %s",
                                      errno, strerror(errno));
        return result;
    }

    if (tcp_bind(sfd, addr, port) < 0) {
        result.type = Err;
        result.data.err = vstr_format("failed to bind socket (errno: %d) %s",
                                      errno, strerror(errno));
        close(sfd);
        return result;
    }

    if (tcp_listen(sfd, SERVER_BACKLOG) < 0) {
        result.type = Err;
        result.data.err =
            vstr_format("failed to listen on socket (errno: %d) %s", errno,
                        strerror(errno));
        close(sfd);
        return result;
    }

    s.conf_file_path = *conf_file_path;

    config_file_contents_res = read_file(vstr_data(&s.conf_file_path));
    if (config_file_contents_res.type == Err) {
        result.type = Err;
        result.data.err = config_file_contents_res.data.err;
        close(sfd);
        return result;
    }

    config_from_file_res =
        parse_config(vstr_data(&config_file_contents_res.data.ok),
                     vstr_len(&config_file_contents_res.data.ok));
    if (config_from_file_res.type == Err) {
        close(sfd);
        result.type = Err;
        result.data.err = config_from_file_res.data.err;
        vstr_free(&config_file_contents_res.data.ok);
        return result;
    }

    vstr_free(&config_file_contents_res.data.ok);

    s.users = vec_new(sizeof(user));
    if (s.users == NULL) {
        result.type = Err;
        result.data.err =
            vstr_from("failed to allocate memory for users vector");
        close(sfd);
        return result;
    }

    users_from_config =
        ht_get(&config_from_file_res.data.ok, &user_type, sizeof user_type);
    assert(users_from_config != NULL);
    assert(users_from_config->type == Array);

    add_users_res = add_users(&s.users, users_from_config->data.vec);
    if (add_users_res == -1) {
        config_free(&config_from_file_res.data.ok);
        vec_free(s.users, user_in_vec_free);
        close(sfd);
        result.type = Err;
        result.data.err = vstr_from("failed to add users to user array");
        return result;
    }

    config_free(&config_from_file_res.data.ok);

    s.executable_path = get_execuable_path();
    if (s.executable_path == NULL) {
        result.type = Err;
        result.data.err =
            vstr_format("failed to get executable path (errno: %d) %s\n", errno,
                        strerror(errno));
        vstr_free(&s.conf_file_path);
        vec_free(s.users, user_in_vec_free);
        close(sfd);
        return result;
    }

    s.os_name = get_os_name();

    addr_str = vstr_from(addr);
    s.addr = addr_str;
    s.port = port;

    s.clients = vec_new(sizeof(client*));
    if (s.clients == NULL) {
        result.type = Err;
        result.data.err = vstr_from("failed to allocate for client vec");
        vstr_free(&s.conf_file_path);
        free(s.executable_path);
        vec_free(s.users, user_in_vec_free);
        close(sfd);
        return result;
    }

    ev = ev_new(SERVER_BACKLOG);
    if (ev == NULL) {
        result.type = Err;
        result.data.err = vstr_from("failed to allocate memory for ev");
        vec_free(s.clients, client_in_vec_free);
        free(s.executable_path);
        vstr_free(&s.conf_file_path);
        vec_free(s.users, user_in_vec_free);
        close(sfd);
        return result;
    }

    if (ev_add_event(ev, sfd, EV_READ, server_accept, &s) == -1) {
        result.type = Err;
        result.data.err = vstr_format(
            "failed to add server accept as event_fn to ev (errno: %d) %s",
            errno, strerror(errno));
        vstr_free(&s.conf_file_path);
        vec_free(s.clients, client_in_vec_free);
        ev_free(ev);
        vec_free(s.users, user_in_vec_free);
        free(s.executable_path);
        close(sfd);
        return result;
    }

    s.help_cmds = init_all_cmd_helps();
    s.help_cmds_len = get_all_cmd_helps_len();

    s.start_time = get_time();
    s.pid = getpid();
    s.sfd = sfd;
    s.db = lexidb_new();
    s.ev = ev;
    s.log_level = ll;
    result.type = Ok;
    result.data.ok = s;
    return result;
}

static lexidb* lexidb_new(void) {
    lexidb* res = calloc(1, sizeof *res);
    assert(res != NULL);
    res->dict = ht_new(sizeof(object), server_compare_objects);
    res->vec = vec_new(sizeof(object));
    assert(res->vec != NULL);
    res->queue = queue_new(sizeof(object));
    res->set = set_new(sizeof(object), server_compare_objects);
    return res;
}

static int add_users(vec** users_vec, vec* users_from_config) {
    vec_iter iter = vec_iter_new(users_from_config);
    while (iter.cur) {
        object* cur = iter.cur;
        object* name_obj;
        object* password_obj;
        vstr name;
        vstr password;
        user user = {0};
        int push_res;
        vstr hash;

        assert(cur->type == Array);

        name_obj = vec_get_at(cur->data.vec, 0);
        password_obj = vec_get_at(cur->data.vec, 1);

        assert(name_obj->type == String);
        assert(password_obj->type == String);

        name = name_obj->data.string;
        password = password_obj->data.string;

        hash = hash_password(vstr_data(&password), vstr_len(&password));

        user.name = vstr_from(vstr_data(&name));
        user.password = hash;

        if (vec_find(*users_vec, &user, NULL, user_compare) != -1) {
            vstr_free(&user.name);
            vstr_free(&user.password);
            return -1;
        }

        push_res = vec_push(users_vec, &user);
        if (push_res == -1) {
            vstr_free(&user.name);
            vstr_free(&user.password);
            return -1;
        }

        vec_iter_next(&iter);
    }
    return 0;
}

static void server_free(server* s) {
    free(s->executable_path);
    ev_free(s->ev);
    lexidb_free(s->db);
    vec_free(s->clients, client_in_vec_free);
    vstr_free(&s->addr);
    vstr_free(&s->conf_file_path);
    vstr_free(&s->os_name);
    vec_free(s->users, user_in_vec_free);
    free(s->help_cmds);
    close(s->sfd);
}

static void lexidb_free(lexidb* db) {
    ht_free(&(db->dict), server_free_object, server_free_object);
    vec_free(db->vec, server_free_object);
    queue_free(&(db->queue), server_free_object);
    set_free(&(db->set), server_free_object);
    free(db);
}

static void server_accept(ev* ev, int fd, void* client_data, int mask) {
    server* s = client_data;
    int cfd;
    struct sockaddr_in addr = {0};
    socklen_t len = 0;
    result(client_ptr) r_client;
    client* client;
    int add;

    cfd = tcp_accept(fd, &addr, &len);
    if (cfd == -1) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            warn("accept blocked\n");
            return;
        } else {
            error("accept failed (errno: %d) %s\n", errno, strerror(errno));
            return;
        }
    }

    if (make_socket_nonblocking(cfd) == -1) {
        error("failed to make %d nonblocking\n", cfd);
        return;
    }

    r_client = create_client(cfd, addr.sin_addr.s_addr, addr.sin_port);
    if (r_client.type == Err) {
        error("%s\n", vstr_data(&(r_client.data.err)));
        vstr_free(&(r_client.data.err));
        return;
    }

    client = r_client.data.ok;

    add = vec_push(&(s->clients), &client);
    if (add == -1) {
        error("failed to add client to client vector (errno: %d) %s\n", errno,
              strerror(errno));
        return;
    }

    add = ev_add_event(s->ev, cfd, EV_READ, read_from_client, s);
    if (add == -1) {
        error("failed to add read event for client (errno: %d) %s\n", errno,
              strerror(errno));
        client_free(client);
        return;
    }

    if (s->log_level >= Info) {
        info("new connection: %d\n", cfd);
    }
}

static void read_from_client(ev* ev, int fd, void* client_data, int mask) {
    ssize_t read_amt;
    server* s = client_data;
    client* c;
    ssize_t found;
    int add;

    found = vec_find(s->clients, &fd, &c, client_compare);
    if (found == -1) {
        error("failed to find client (fd %d) in client vec\n", fd);
        ev_delete_event(ev, fd, EV_READ);
        close(fd);
        return;
    }

    for (;;) {
        /* it should never be greater than, but just in case */
        if (c->read_pos >= c->read_cap) {
            if (realloc_client_read_buf(c) == -1) {
                error("failed to realloce client buffer, closing connection\n");
                vec_remove_at(s->clients, found, NULL);
                ev_delete_event(ev, fd, mask);
                client_free(c);
                return;
            }
        }

        read_amt =
            read(fd, c->read_buf + c->read_pos, c->read_cap - c->read_pos);

        if (read_amt == 0) {
            if (s->log_level >= Info) {
                info("closing %d\n", fd);
            }
            vec_remove_at(s->clients, found, NULL);
            ev_delete_event(ev, fd, mask);
            client_free(c);
            return;
        }

        if (read_amt == -1) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                break;
            } else {
                // todo
                error("failed to read from client (errno: %d) %s\n", errno,
                      strerror(errno));
                return;
            }
        }
        c->read_pos += read_amt;
    }

    if (s->log_level >= Debug) {
        debug("received: %s\n", c->read_buf);
    }

    execute_cmd(s, c);

    c->write_buf = (uint8_t*)builder_out(&(c->builder));
    c->write_size = builder_len(&(c->builder));

    add = ev_add_event(ev, fd, EV_WRITE, write_to_client, s);
    if (add == -1) {
        error("failed to add write event for %d\n", fd);
        return;
    }

    memset(c->read_buf, 0, c->read_pos);
    c->read_pos = 0;

    return;
}

static void write_to_client(ev* ev, int fd, void* client_data, int mask) {
    server* s = client_data;
    size_t bytes_sent = 0;
    size_t to_send;
    client* c;
    ssize_t found = vec_find(s->clients, &fd, &c, client_compare);

    if (found == -1) {
        error("failed to find client %d\n", fd);
        ev_delete_event(ev, fd, mask);
        close(fd);
        return;
    }

    to_send = c->write_size;

    while (bytes_sent < to_send) {
        ssize_t amt_sent = write(fd, c->write_buf, c->write_size);
        if (amt_sent == -1) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                error("write blocked\n");
                return;
            } else {
                error("failed to write to client (errno: %d) %s\n", errno,
                      strerror(errno));
                return;
            }
        }
        bytes_sent += amt_sent;
    }

    ev_delete_event(ev, fd, EV_WRITE);
    builder_reset(&(c->builder));
}

static result(client_ptr) create_client(int fd, uint32_t addr, uint16_t port) {
    result(client_ptr) res = {0};
    client* c;
    c = calloc(1, sizeof *c);
    if (c == NULL) {
        res.type = Err;
        res.data.err = vstr_from("failed to allocate memory for client");
        return res;
    }
    c->fd = fd;
    c->time_connected = get_time();
    c->addr = addr;
    c->port = port;
    c->read_buf = calloc(CLIENT_READ_BUF_CAP, sizeof(uint8_t));
    if (c->read_buf == NULL) {
        free(c);
        res.type = Err;
        res.data.err =
            vstr_from("failed to allocate memory for client read buf");
        return res;
    }
    c->read_cap = CLIENT_READ_BUF_CAP;
    c->read_pos = 0;
    c->builder = builder_new();
    res.type = Ok;
    res.data.ok = c;
    return res;
}

static void execute_cmd(server* s, client* c) {
    cmd cmd = parse(c->read_buf, c->read_pos);
    if (cmd.type == Illegal) {
        builder_add_err(&(c->builder), err_invalid_command.str,
                        err_invalid_command.str_len);
        return;
    }
    if (!(c->flags & AUTHENTICATED) && cmd.type != Auth) {
        cmd_free(&cmd);
        builder_add_err(&c->builder, err_unauthed.str, err_unauthed.str_len);
        return;
    }
    switch (cmd.type) {
    case Okc:
        break;
    case Ping:
        builder_add_pong(&(c->builder));
        s->cmd_executed++;
        break;
    case Auth: {
        auth_cmd auth = cmd.data.auth;
        int auth_res = execute_auth_command(s, c, &auth);
        if (auth_res != 0) {
            builder_add_err(&c->builder, "failed to authenticate", 22);
            break;
        }
        builder_add_ok(&c->builder);
    } break;
    case Infoc: {
        vstr time_secs;
        struct timespec cur_time;
        uint64_t uptime_secs;
        builder_add_ht(&c->builder, 10);

        builder_add_string(&c->builder, "process id", 10);
        builder_add_int(&c->builder, s->pid);

        builder_add_string(&c->builder, "executable", 10);
        builder_add_string(&c->builder, s->executable_path,
                           strlen(s->executable_path));

        builder_add_string(&c->builder, "config file", 11);
        builder_add_string(&c->builder, vstr_data(&s->conf_file_path),
                           vstr_len(&s->conf_file_path));

        builder_add_string(&c->builder, "OS", 2);
        builder_add_string(&c->builder, vstr_data(&s->os_name),
                           vstr_len(&s->os_name));

        builder_add_string(&c->builder, "multiplexing api", 16);
        builder_add_string(&c->builder, ev_api_name(), strlen(ev_api_name()));

        builder_add_string(&c->builder, "host", 4);
        builder_add_string(&c->builder, vstr_data(&s->addr),
                           vstr_len(&s->addr));

        builder_add_string(&c->builder, "port", 4);
        builder_add_int(&c->builder, s->port);

        builder_add_string(&c->builder, "commands processes", 18);
        builder_add_int(&c->builder, s->cmd_executed);

        builder_add_string(&c->builder, "num connections", 15);
        builder_add_int(&c->builder, s->clients->len);

        cur_time = get_time();
        uptime_secs = cur_time.tv_sec - s->start_time.tv_sec;
        time_secs = vstr_format("%lu secs", uptime_secs);
        builder_add_string(&c->builder, "uptime", 6);
        builder_add_string(&c->builder, vstr_data(&time_secs),
                           vstr_len(&time_secs));
        vstr_free(&time_secs);

        s->cmd_executed++;
    } break;
    case Keys: {
        size_t len = s->db->dict.num_entries;
        ht_iter iter = ht_iter_new(&s->db->dict);
        builder_add_array(&c->builder, len);
        while (iter.cur) {
            ht_entry* cur = iter.cur;
            const object* key = ht_entry_get_key(cur);
            builder_add_object(&c->builder, key);
            ht_iter_next(&iter);
        }
        s->cmd_executed++;
    } break;
    case Set: {
        ht_result set_res = execute_set_command(s, &(cmd.data.set));
        if (set_res != HT_OK) {
            builder_add_err(&(c->builder), err_oom.str, err_oom.str_len);
            break;
        }
        builder_add_ok(&(c->builder));
        s->cmd_executed++;
    } break;
    case Get: {
        object* obj = execute_get_command(s, &(cmd.data.get));
        if (obj == NULL) {
            builder_add_none(&(c->builder));
            break;
        }
        builder_add_object(&(c->builder), obj);
        s->cmd_executed++;
    } break;
    case Del: {
        int del_res = execute_del_command(s, &(cmd.data.del));
        if (del_res != HT_OK) {
            builder_add_err(&(c->builder), err_invalid_key.str,
                            err_invalid_key.str_len);
            break;
        }
        builder_add_ok(&(c->builder));
        s->cmd_executed++;
    } break;
    case Push: {
        int push_res = execute_push_command(s, &(cmd.data.push));
        if (push_res == -1) {
            builder_add_err(&(c->builder), "unable to push", 14);
            break;
        }
        builder_add_ok(&(c->builder));
        s->cmd_executed++;
    } break;
    case Pop: {
        result(object) ro = execute_pop_command(s);
        object obj;
        if (ro.type == Err) {
            builder_add_none(&(c->builder));
            break;
        }
        obj = ro.data.ok;
        builder_add_object(&(c->builder), &obj);
        object_free(&obj);
        s->cmd_executed++;
    } break;
    case Enque: {
        int res = execute_enque_command(s, &cmd.data.enque);
        if (res == -1) {
            builder_add_err(&c->builder, "failed to enque data", 20);
            break;
        }
        builder_add_ok(&c->builder);
        s->cmd_executed++;
    } break;
    case Deque: {
        result(object) ro = execute_deque_command(s);
        object obj;
        if (ro.type == Err) {
            builder_add_none(&c->builder);
            break;
        }
        obj = ro.data.ok;
        builder_add_object(&c->builder, &obj);
        object_free(&obj);
        s->cmd_executed++;
    } break;
    case ZSet: {
        set_result res = execute_zset_command(s, &cmd.data.zset);
        if (res != SET_OK) {
            if (res == SET_OOM) {
                builder_add_err(&c->builder, err_oom.str, err_oom.str_len);
                break;
            }
        }
        builder_add_ok(&c->builder);
        s->cmd_executed++;
    } break;
    case ZHas: {
        bool res = execute_zhas_command(s, &cmd.data.zhas);
        if (res == false) {
            builder_add_none(&c->builder);
            break;
        }
        builder_add_int(&c->builder, 1);
        s->cmd_executed++;
    } break;
    case ZDel: {
        set_result res = execute_zdel_command(s, &cmd.data.zdel);
        if (res != SET_OK) {
            builder_add_err(&c->builder, err_invalid_key.str,
                            err_invalid_key.str_len);
            break;
        }
        builder_add_ok(&c->builder);
        s->cmd_executed++;
    } break;
    default:
        builder_add_err(&(c->builder), err_invalid_command.str,
                        err_invalid_command.str_len);
        break;
    }
}

static int execute_auth_command(server* s, client* client, auth_cmd* auth) {
    user user = {0};
    vstr username;
    vstr password;
    vstr hashed_password;
    ssize_t found;
    int cmp;
    object username_obj = auth->username;
    object password_obj = auth->password;
    if (username_obj.type != String) {
        object_free(&auth->username);
        object_free(&auth->password);
        return -1;
    }
    if (password_obj.type != String) {
        object_free(&auth->username);
        object_free(&auth->password);
        return -1;
    }

    username = username_obj.data.string;
    password = password_obj.data.string;
    found = vec_find(s->users, &username, &user, user_compare_by_username);
    if (found == -1) {
        vstr_free(&username);
        vstr_free(&password);
        return -1;
    }

    hashed_password = hash_password(vstr_data(&password), vstr_len(&password));

    cmp = time_safe_compare(vstr_data(&hashed_password),
                            vstr_data(&user.password),
                            vstr_len(&hashed_password));

    if (cmp == 0) {
        client->user = user;
        client->flags |= AUTHENTICATED;
    }

    vstr_free(&hashed_password);
    vstr_free(&username);
    vstr_free(&password);
    return cmp;
}

static ht_result execute_set_command(server* s, set_cmd* set) {
    object key = set->key;
    object value = set->value;
    ht_result res = ht_insert(&(s->db->dict), &key, sizeof(object), &value,
                              server_free_object, server_free_object);
    return res;
}

static object* execute_get_command(server* s, get_cmd* get) {
    object key = get->key;
    object* res = ht_get(&(s->db->dict), &key, sizeof(object));
    object_free(&key);
    return res;
}

static ht_result execute_del_command(server* s, del_cmd* del) {
    object key = del->key;
    ht_result res = ht_delete(&(s->db->dict), &key, sizeof(object),
                              server_free_object, server_free_object);
    object_free(&key);
    return res;
}

static int execute_push_command(server* s, push_cmd* push) {
    object value = push->value;
    int res = vec_push(&(s->db->vec), &value);
    return res;
}

static result(object) execute_pop_command(server* s) {
    result(object) ro = {0};
    object out;
    int pop_res = vec_pop(s->db->vec, &out);
    if (pop_res == -1) {
        ro.type = Err;
        return ro;
    }
    ro.type = Ok;
    ro.data.ok = out;
    return ro;
}

static int execute_enque_command(server* s, enque_cmd* enque) {
    object to_enque = enque->value;
    return queue_enque(&s->db->queue, &to_enque);
}

static result(object) execute_deque_command(server* s) {
    result(object) ro = {0};
    object obj;
    int deque_res = queue_deque(&s->db->queue, &obj);
    if (deque_res == -1) {
        ro.type = Err;
        return ro;
    }
    ro.type = Ok;
    ro.data.ok = obj;
    return ro;
}

static set_result execute_zset_command(server* s, zset_cmd* zset) {
    return set_insert(&s->db->set, &zset->value, server_free_object);
}

static bool execute_zhas_command(server* s, zhas_cmd* zhas) {
    bool res = set_has(&s->db->set, &zhas->value);
    object_free(&zhas->value);
    return res;
}

static set_result execute_zdel_command(server* s, zdel_cmd* zdel) {
    set_result res = set_delete(&s->db->set, &zdel->value, server_free_object);
    object_free(&zdel->value);
    return res;
}

static result(log_level) determine_loglevel(vstr* loglevel_s) {
    result(log_level) res = {0};
    size_t i, len = sizeof log_level_lookups / sizeof log_level_lookups[0];
    const char* s = vstr_data(loglevel_s);
    size_t s_len = vstr_len(loglevel_s);
    for (i = 0; i < len; ++i) {
        log_level_lookup lll = log_level_lookups[i];
        size_t lookup_len = strlen(lll.str);
        if (lookup_len != s_len) {
            continue;
        }
        if (strncmp(s, lll.str, s_len) == 0) {
            res.type = Ok;
            res.data.ok = lll.ll;
            return res;
        }
    }
    res.type = Err;
    res.data.err = vstr_from("invalid log level");
    return res;
}

static int realloc_client_read_buf(client* c) {
    void* tmp;
    size_t new_cap = c->read_cap << 1;
    tmp = realloc(c->read_buf, new_cap);
    if (tmp == NULL) {
        return -1;
    }
    c->read_buf = tmp;
    memset(c->read_buf + c->read_pos, 0, new_cap - c->read_pos);
    c->read_cap = new_cap;
    return 0;
}

static void cmd_free(cmd* cmd) {
    switch (cmd->type) {
    case Illegal:
        break;
    case Okc:
        break;
    case Help:
        break;
    case Auth:
        object_free(&cmd->data.auth.username);
        object_free(&cmd->data.auth.password);
        break;
    case Ping:
        break;
    case Infoc:
        break;
    case Keys:
        break;
    case Set:
        object_free(&cmd->data.set.key);
        object_free(&cmd->data.set.value);
        break;
    case Get:
        object_free(&cmd->data.get.key);
        break;
    case Del:
        object_free(&cmd->data.del.key);
        break;
    case Push:
        object_free(&cmd->data.push.value);
        break;
    case Pop:
        break;
    case Enque:
        object_free(&cmd->data.enque.value);
        break;
    case Deque:
        break;
    case ZSet:
        object_free(&cmd->data.zset.value);
        break;
    case ZHas:
        object_free(&cmd->data.zhas.value);
        break;
    case ZDel:
        object_free(&cmd->data.zdel.value);
        break;
    }
}

static int server_compare_objects(void* a, void* b) {
    object* ao = a;
    object* bo = b;
    return object_cmp(ao, bo);
}

static int client_compare(void* fdp, void* clientp) {
    int fd = *((int*)fdp);
    client* c = *((client**)clientp);
    return fd - c->fd;
}

static int user_compare(void* a, void* b) {
    user* ua = a;
    user* ub = b;
    return vstr_cmp(&ua->name, &ub->name);
}

static int user_compare_by_username(void* a, void* b) {
    vstr* username = a;
    user* user = b;
    return vstr_cmp(username, &user->name);
}

static void server_free_object(void* ptr) {
    object* o = ptr;
    object_free(o);
}

static void client_free(client* client) {
    close(client->fd);
    builder_free(&(client->builder));
    free(client->read_buf);
    free(client);
}

static void client_in_vec_free(void* ptr) {
    client* c = *((client**)ptr);
    client_free(c);
}

static void user_in_vec_free(void* ptr) {
    user* u = ptr;
    vstr_free(&u->name);
    vstr_free(&u->password);
}
