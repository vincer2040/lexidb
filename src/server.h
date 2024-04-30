#ifndef __SERVER_H__

#define __SERVER_H__

#include "builder.h"
#include "ev.h"
#include "vec.h"
#include "vmap.h"
#include "vstr.h"
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BIND_ADDR_MAX 16

#define UNUSED(v) ((void)v)

#define format_err(fmt, ...) vstr_format(fmt, __VA_ARGS__)

typedef struct lexi_server lexi_server;
typedef struct client client;
typedef struct connection connection;
typedef struct user user;

typedef enum {
    LL_Nothing = 0,
    LL_Info = 1,
    LL_Warning = 2,
    LL_Verbose = 3,
    LL_Debug = 4,
} log_level;

typedef struct {
    uint64_t id;
    vmap* keys;
} lexi_db;

typedef struct {
    const char* ok;
    const char* pong;
    const char* none;
    const char* denied_cmd;
    const char* null;
    const char* zero;
    const char* one;
    const char* invalid_auth;
} shared_reply;

struct lexi_server {
    pid_t pid;              /* pid of the process */
    pthread_t thread_id;    /* main thread id */
    int sfd;                /* the socket the server is listening on */
    char* config_file;      /* absolute path to config file */
    char* executable;       /* absolute path to this executable */
    int argc;               /* the number of args passed to executable */
    char** argv;            /* the args passsed to executable */
    ev* ev;                 /* event loop */
    uint16_t port;          /* the tcp port */
    vstr bind_addr;         /* the address to bind to */
    uint64_t max_clients;   /* the maximum number of concurrent connections */
    vec* users;             /* list of configured users */
    vec* clients;           /* list of active clients */
    log_level loglevel;     /* the amount to log */
    char* logfile;          /* path to log to. if null, logs sent to stdout */
    int protected_mode;     /* are we in protected mode? */
    int tcp_backlog;        /* backlog passed to listen() */
    uint64_t num_databases; /* the number of databases */
    lexi_db* databases;     /* array of databases, len = num_databases */
    volatile sig_atomic_t shutdown; /* 1 when should shutdown */
    int signal_received;            /* the signal received */
    uint64_t next_client_id;        /* the next client id to use */
};

struct client {
    uint64_t id;              /* autoincremented unique id */
    connection* conn;         /* connection */
    vstr name;                /* name */
    lexi_db* db;              /* pointer to selected db */
    user* user;               /* user associated with this connection */
    time_t creation_time;     /* time this client was created */
    char ip[BIND_ADDR_MAX];   /* bind addr */
    int port;                 /* port */
    int authenticated;        /* is this client authenticated */
    int protocol_version;     /* the protocol version to use */
    size_t read_buf_pos;      /* position in buf */
    size_t read_buf_cap;      /* buf capacity */
    unsigned char* read_buf;  /* read buf */
    builder builder;          /* protocol builder */
    size_t write_len;         /* the amount to write */
    unsigned char* write_buf; /* the buffer to write */
};

#define USER_ON (1 << 0)
#define USER_NO_PASS (1 << 1)
#define USER_DEFAULT (1 << 2)

struct user {
    vstr username;   /* the username */
    uint32_t flags;  /* flags. see USER_ */
    vec* passwords;  /* vector of allowed passwords */
    vec* categories; /* vector of allowed categories */
    vec* commands;   /* vector of allowed commands */
};

typedef enum {
    CS_None,
    CS_Accepting,
    CS_Connected,
    CS_Closed,
    CS_Error,
} connection_state;

struct connection {
    int fd;
    connection_state state;
    int last_errno;
};

extern lexi_server server;
extern shared_reply shared_replies;

int server_run(int argc, char** argv);
int configure_server(lexi_server* s, const char* input, size_t input_len,
                     vstr* error);
void server_log(log_level level, const char* buf, size_t buf_len);

/* vmap_type.c */
void init_vmap_type(void);
vmap_type* get_vmap_type(void);

/* networking.c */
client* client_new(connection* conn);
void client_select_db(client* client, uint64_t db);
int client_read(client* client);
int client_write(client* client);
void client_close(client* client);
void client_add_reply_ok(client* client);
void client_add_reply_pong(client* client);
void client_add_reply_none(client* client);
void client_add_reply_no_access(client* client);
void client_add_reply_null(client* client);
void client_add_reply_zero(client* client);
void client_add_reply_one(client* client);
void client_add_reply_invalid_auth(client* client);
int client_add_reply_simple_error(client* client, const vstr* error);
int client_add_reply_bulk_error(client* client, const vstr* error);
int client_add_reply_object(client* client, const object* obj);
int client_add_reply_array(client* client, size_t len);
int client_add_reply_bulk_string(client* client, const vstr* string);

/* connection.c */
connection* connection_new(int fd);
void connection_close(connection* conn);

/* db.c */
int db_insert_key(lexi_db* db, vstr* key, object* value);
const object* db_get_key(const lexi_db* db, const vstr* key);
int db_delete_key(lexi_db* db, const vstr* key);
uint64_t db_get_num_keys(const lexi_db* db);

#endif /* __SERVER_H__ */
