#include "vline.h"
#include <errno.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define HISTORY_MAX_LEN 100

#define REFRESH_WRITE (1 << 0)
#define REFRESH_CLEAN (1 << 1)
#define REFRESH_ALL (REFRESH_CLEAN | REFRESH_WRITE)

typedef enum {
    CTRL_C = 3,
    ENTER = 13,
    ESC = 27,
    BACKSPACE = 127,
} key;

typedef struct {
    size_t len;
    size_t cap;
    char* buf;
} buffer;

static char* history[HISTORY_MAX_LEN] = {0};
static size_t history_len = 0;

static struct termios original;
static int at_exit_registerd = 0;

vline_completion_callback* completion_callback = NULL;
vline_hints_callback* hints_callback = NULL;
vline_free_hint_callback* free_hint_callback = NULL;

static void vline_at_exit(void);

static void refresh_line_with_flags(vline* vl, int flags);
static void completions_free(vline_completions* vlc);

static buffer buffer_new(void);
static int buffer_push(buffer* b, const char* s, size_t slen);
static void buffer_free(buffer* b);

static void disable(int fd);

static char* vline_strdup(const char* str);

char* vline_more = "the user is still editing";

static int enable(int fd) {
    struct termios new;
    if (!isatty(STDIN_FILENO)) {
        return -1;
    }

    if (!at_exit_registerd) {
        atexit(vline_at_exit);
        at_exit_registerd = 1;
    }

    if (tcgetattr(fd, &original) == -1) {
        return -1;
    }
    new = original;
    /* no CR to NL */
    new.c_iflag &= ~ICRNL;
    /* 8 bit chars */
    new.c_cflag |= CS8;
    /* disable echoing, non-canonical, no signals */
    new.c_lflag &= ~(ECHO | ICANON | ISIG);
    /* read return every byte */
    new.c_cc[VMIN] = 1;
    /* no timer */
    new.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSAFLUSH, &new) == -1) {
        return -1;
    }
    return 0;
}

static int get_cursor_position(int stdin_fd, int stdout_fd) {
    char buf[32] = {0};
    int cols, rows;
    uint32_t i = 0;

    if (write(stdout_fd, "\x1b]6n", 4) != 4) {
        return -1;
    }
    while (i < sizeof buf - 1) {
        if (read(stdin_fd, buf + i, 1) != 1) {
            break;
        }
        if (buf[i] == 'R') {
            break;
        }
        i++;
    }
    if (buf[0] != ESC || buf[1] != '[') {
        return -1;
    }
    if (sscanf(buf + 2, "%d;%d", &rows, &cols) != 2) {
        return -1;
    }
    return cols;
}

static int get_columns(int stdin_fd, int stdout_fd) {
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        int cols, start = get_cursor_position(stdin_fd, stdout_fd);
        if (start == -1) {
            goto fail;
        }
        if (write(stdout_fd, "\x1b[999C", 6) != 6) {
            goto fail;
        }
        cols = get_cursor_position(stdin_fd, stdout_fd);
        if (cols == -1) {
            goto fail;
        }

        if (cols > start) {
            char seq[32];
            snprintf(seq, 32, "\x1b[%dD", cols - start);
            write(stdout_fd, seq, strlen(seq));
        }
        return cols;
    } else {
        return ws.ws_col;
    }
fail:
    return 80;
}

int vline_init(vline* vl, int stdin_fd, int stdout_fd, char* buf,
               size_t buf_len, const char* prompt) {
    vl->stdin_fd = stdin_fd != -1 ? stdin_fd : STDIN_FILENO;
    vl->stdout_fd = stdout_fd != -1 ? stdout_fd : STDOUT_FILENO;
    vl->buf = buf;
    vl->buf_len = buf_len;
    vl->prompt = prompt;
    vl->prompt_len = strlen(prompt);
    vl->pos = 0;
    vl->len = 0;
    vl->history_idx = 0;
    vl->in_completion = 0;
    vl->completion_idx = 0;

    if (enable(vl->stdin_fd) == -1) {
        return -1;
    }

    vl->num_cols = get_columns(stdin_fd, stdout_fd);

    /* space for null term */
    vl->buf_len--;

    vline_history_add("");

    if (write(vl->stdout_fd, vl->prompt, vl->prompt_len) == -1) {
        return -1;
    }
    return 0;
}

static void refresh_line_with_completions(vline* vl, vline_completions* vlc,
                                          int flags) {
    vline_completions table = {0};
    if (vlc == NULL) {
        completion_callback(vl->buf, &table);
        vlc = &table;
    }
    if (vl->completion_idx < vlc->len) {
        vline saved = *vl;
        vl->len = vl->pos = strlen(vlc->vec[vl->completion_idx]);
        vl->buf = vlc->vec[vl->completion_idx];
        refresh_line_with_flags(vl, flags);
        vl->len = saved.len;
        vl->pos = saved.pos;
        vl->buf = saved.buf;
    } else {
        refresh_line_with_flags(vl, flags);
    }

    if (vlc != &table) {
        completions_free(&table);
    }
}

static void refresh_show_hints(buffer* b, vline* vl, size_t prompt_len) {
    char seq[64] = {0};
    if (hints_callback && prompt_len + vl->len < vl->num_cols) {
        int color = -1, bold = 0;
        char* hint = hints_callback(vl->buf, &color, &bold);
        size_t hint_len, hint_max_len;
        if (hint == NULL) {
            return;
        }
        hint_len = strlen(hint);
        hint_max_len = vl->num_cols - (prompt_len + vl->len);
        if (hint_len > hint_max_len) {
            hint_len = hint_max_len;
        }
        if (bold == 1 && color == -1) {
            color = 37;
        }
        if (color != -1 || bold != 0) {
            snprintf(seq, 64, "\033[%d;%d;49m", bold, color);
        } else {
            seq[0] = '\0';
        }
        buffer_push(b, seq, strlen(seq));
        buffer_push(b, hint, hint_len);
        if (color != -1 || bold != 0) {
            buffer_push(b, "\033[0m", 4);
        }
        if (free_hint_callback) {
            free_hint_callback(hint);
        }
    }
}

static void refresh_single_line(vline* vl, int flags) {
    char seq[64] = {0};
    size_t prompt_len = vl->prompt_len;
    int fd = vl->stdout_fd;
    char* buf = vl->buf;
    size_t len = vl->len;
    size_t pos = vl->pos;
    buffer buffer = buffer_new();

    while ((prompt_len + pos) >= vl->num_cols) {
        buf++;
        len--;
        pos--;
    }
    while ((prompt_len + pos) > vl->num_cols) {
        len--;
    }

    snprintf(seq, sizeof seq, "\r");
    buffer_push(&buffer, seq, strlen(seq));

    if (flags & REFRESH_WRITE) {
        buffer_push(&buffer, vl->prompt, prompt_len);
        buffer_push(&buffer, buf, len);
        refresh_show_hints(&buffer, vl, prompt_len);
    }

    /* erase right */
    snprintf(seq, sizeof(seq), "\x1b[0K");
    buffer_push(&buffer, seq, strlen(seq));

    if (flags & REFRESH_WRITE) {
        /* move cursor to original position */
        snprintf(seq, sizeof(seq), "\r\x1b[%dC", (int)(pos + prompt_len));
        buffer_push(&buffer, seq, strlen(seq));
    }

    write(fd, buffer.buf, buffer.len);
    buffer_free(&buffer);
}

static void refresh_line_with_flags(vline* vl, int flags) {
    refresh_single_line(vl, flags);
}

static void refresh_line(vline* vl) {
    refresh_line_with_flags(vl, REFRESH_ALL);
}

void vline_hide(vline* vl) { refresh_single_line(vl, REFRESH_CLEAN); }

void vline_show(vline* vl) { refresh_single_line(vl, REFRESH_WRITE); }

static void vline_backspace(vline* vl) {
    if (vl->pos > 0 && vl->len > 0) {
        memmove(vl->buf + vl->pos - 1, vl->buf + vl->pos, vl->len - vl->pos);
        vl->pos--;
        vl->len--;
        vl->buf[vl->len] = '\0';
        refresh_line(vl);
    }
}

static void vline_delete(vline* vl) {
    if (vl->len > 0 && vl->pos < vl->len) {
        memmove(vl->buf + vl->pos, vl->buf + vl->pos + 1,
                vl->len - vl->pos - 1);
        vl->len--;
        vl->buf[vl->len] = '\0';
        refresh_line(vl);
    }
}

static void vline_move_right(vline* vl) {
    if (vl->pos != vl->len) {
        vl->pos++;
        refresh_line(vl);
    }
}

static void vline_move_left(vline* vl) {
    if (vl->pos != 0) {
        vl->pos--;
        refresh_line(vl);
    }
}

static void vline_move_home(vline* vl) {
    if (vl->pos != 0) {
        vl->pos = 0;
        refresh_line(vl);
    }
}

static void vline_move_end(vline* vl) {
    if (vl->pos != vl->len) {
        vl->pos = vl->len;
        refresh_line(vl);
    }
}

#define VLINE_HISTORY_PREV 0
#define VLINE_HISTORY_NEXT 1
static void vline_history_get(vline* vl, int dir) {
    if (history_len <= 1) {
        return;
    }
    free(history[history_len - 1 - vl->history_idx]);
    history[history_len - 1 - vl->history_idx] = vline_strdup(vl->buf);
    vl->history_idx += (dir == VLINE_HISTORY_PREV) ? 1 : -1;
    if (vl->history_idx < 0) {
        vl->history_idx = 0;
        return;
    } else if (vl->history_idx >= history_len) {
        vl->history_idx = history_len - 1;
        return;
    }
    memcpy(vl->buf, history[history_len - 1 - vl->history_idx], vl->buf_len);
    vl->buf[vl->buf_len - 1] = '\0';
    vl->len = vl->pos = strlen(vl->buf);
    refresh_line(vl);
}

static int vline_insert(vline* vl, char ch) {
    if (vl->len >= vl->buf_len) {
        return 0;
    }
    if (vl->len == vl->pos) {
        vl->buf[vl->pos] = ch;
        vl->pos++;
        vl->len++;
        vl->buf[vl->len] = '\0';
        if (vl->prompt_len + vl->len < vl->num_cols && !hints_callback) {
            if (write(vl->stdout_fd, &ch, 1) == -1) {
                return -1;
            }
        } else {
            refresh_line(vl);
        }
    } else {
        memmove(vl->buf + vl->pos + 1, vl->buf + vl->pos, vl->len - vl->pos);
        vl->buf[vl->len] = ch;
        vl->len++;
        vl->pos++;
        vl->buf[vl->len] = '\0';
        refresh_line(vl);
    }
    return 0;
}

static char complete_line(vline* vl, char ch) {
    vline_completions vlc = {0, 0, NULL};
    int rv;
    completion_callback(vl->buf, &vlc);
    if (vlc.len == 0) {
        vl->in_completion = 0;
        completions_free(&vlc);
        return ch;
    }
    switch (ch) {
    case 9: /* tab */
        if (vl->in_completion == 0) {
            vl->in_completion = 1;
            vl->completion_idx = 0;
        } else {
            vl->completion_idx = (vl->completion_idx + 1) % (vlc.len + 1);
        }
        ch = 0;
        break;
    case 27: /* escape */
        if (vl->completion_idx < vlc.len) {
            refresh_line(vl);
        }
        vl->in_completion = 0;
        ch = 0;
        break;
    default:
        if (vl->completion_idx < vlc.len) {
            rv = snprintf(vl->buf, vl->buf_len, "%s",
                          vlc.vec[vl->completion_idx]);
            vl->len = vl->pos = rv;
        }
        vl->in_completion = 0;
        break;
    }
    if (vl->in_completion && vl->completion_idx < vlc.len) {
        refresh_line_with_completions(vl, &vlc, REFRESH_ALL);
    } else {
        refresh_line(vl);
    }
    completions_free(&vlc);
    return ch;
}

char* vline_edit(vline* vl) {
    char c;
    int rv;
    char seq[3] = {0};
    rv = read(vl->stdin_fd, &c, 1);
    if (rv <= 0) {
        return NULL;
    }

    if ((vl->in_completion || c == 9) && completion_callback != NULL) {
        c = complete_line(vl, c);
        if (c < 0) {
            return NULL;
        }
        if (c == 0) {
            return vline_more;
        }
    }

    switch (c) {
    case ENTER:
        history_len--;
        free(history[history_len]);
        if (hints_callback) {
            vline_hints_callback* cb = hints_callback;
            hints_callback = NULL;
            refresh_line(vl);
            hints_callback = cb;
        }
        return vline_strdup(vl->buf);
    case CTRL_C:
        errno = EAGAIN;
        return NULL;
    case BACKSPACE:
        vline_backspace(vl);
        break;
    case ESC:
        if (read(vl->stdin_fd, seq, 1) == -1) {
            break;
        }
        if (read(vl->stdin_fd, seq + 1, 1) == -1) {
            break;
        }
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(vl->stdin_fd, seq + 2, 1) == -1) {
                    break;
                }
                if (seq[2] == '~') {
                    switch (seq[1]) {
                    case '3': /* delete */
                        vline_delete(vl);
                        break;
                    }
                }
            } else {
                switch (seq[1]) {
                case 'A': /* up */
                    vline_history_get(vl, VLINE_HISTORY_PREV);
                    break;
                case 'B': /* down */
                    vline_history_get(vl, VLINE_HISTORY_NEXT);
                    break;
                case 'C': /* right */
                    vline_move_right(vl);
                    break;
                case 'D': /* left */
                    vline_move_left(vl);
                    break;
                case 'H': /* home */
                    vline_move_home(vl);
                    break;
                case 'F': /* end */
                    vline_move_end(vl);
                    break;
                }
            }
        } else if (seq[0] == '0') {
            switch (seq[1]) {
            case 'H': /* home */
                vline_move_home(vl);
                break;
            case 'F': /* end */
                vline_move_end(vl);
                break;
            }
        }
        break;
    default:
        if (vline_insert(vl, c) == -1) {
            return NULL;
        }
        break;
    }
    return vline_more;
}

void vline_stop(vline* vl) {
    disable(vl->stdin_fd);
    printf("\n");
}

int vline_history_add(const char* line) {
    char* dup;
    dup = vline_strdup(line);
    if (dup == NULL) {
        return -1;
    }
    if (history_len == HISTORY_MAX_LEN) {
        free(history[0]);
        memmove(history, &history[1], (history_len - 1) * sizeof(char*));
        history[history_len - 1] = dup;
        return 0;
    }
    history[history_len] = dup;
    history_len++;
    return 0;
}

int vline_history_save(const char* file) {
    FILE* fp;
    buffer b;
    size_t i;
    ssize_t rv;

    fp = fopen(file, "w");
    if (fp == NULL) {
        return -1;
    }

    b = buffer_new();

    for (i = 0; i < history_len; ++i) {
        char* hist = history[i];
        buffer_push(&b, hist, strlen(hist));
        buffer_push(&b, "\n", 1);
    }

    rv = fwrite(b.buf, b.len, sizeof(char), fp);
    if (rv == -1) {
        buffer_free(&b);
        fclose(fp);
        return -1;
    }

    buffer_free(&b);
    fclose(fp);
    return 0;
}

void vline_history_free(void) {
    size_t i;
    for (i = 0; i < history_len; ++i) {
        free(history[i]);
    }
}

void vline_set_completion_callback(vline_completion_callback* callback) {
    completion_callback = callback;
}

void vline_set_hints_callback(vline_hints_callback* callback) {
    hints_callback = callback;
}

void vline_set_free_hint_callback(vline_free_hint_callback* callback) {
    free_hint_callback = callback;
}

int vline_add_completion(vline_completions* vlc, const char* str) {
    char* dup = vline_strdup(str);
    if (dup == NULL) {
        return -1;
    }
    if (vlc->vec == NULL) {
        vlc->cap = 32;
        vlc->vec = calloc(32, sizeof(char*));
        if (vlc->vec == NULL) {
            return -1;
        }
    }
    if (vlc->len >= vlc->cap) {
        void* tmp;
        vlc->cap <<= 1;
        tmp = realloc(vlc->vec, vlc->cap * (sizeof(char*)));
        if (tmp == NULL) {
            return -1;
        }
        vlc->vec = tmp;
        memset(&vlc->vec[vlc->len], 0, (vlc->cap - vlc->len) * sizeof(char*));
    }
    vlc->vec[vlc->len] = dup;
    vlc->len++;
    return 0;
}

static void completions_free(vline_completions* vlc) {
    size_t i, len = vlc->len;
    for (i = 0; i < len; ++i) {
        char* str = vlc->vec[i];
        free(str);
    }
    if (vlc->vec) {
        free(vlc->vec);
    }
}

static buffer buffer_new(void) {
    buffer b = {0};
    b.cap = 32;
    b.len = 0;
    b.buf = calloc(b.cap, sizeof(char));
    return b;
}

static int buffer_push(buffer* b, const char* s, size_t slen) {
    size_t len = b->len, cap = b->cap;
    if (len + slen >= cap - 1) {
        void* tmp;
        cap = (cap << 1) + slen;
        tmp = realloc(b->buf, cap);
        if (tmp == NULL) {
            return -1;
        }
        b->buf = tmp;
        b->cap = cap;
        memset(b->buf + len, 0, cap - len);
    }
    memcpy(b->buf + len, s, slen);
    b->len += slen;
    return 0;
}

static void buffer_free(buffer* b) { free(b->buf); }

static void disable(int fd) { tcsetattr(fd, TCSAFLUSH, &original); }

static void vline_at_exit(void) { disable(STDIN_FILENO); }

static char* vline_strdup(const char* str) {
    size_t len = strlen(str);
    char* res = malloc(len + 1);
    if (res == NULL) {
        return NULL;
    }
    memset(res, 0, len + 1);
    memcpy(res, str, len);
    return res;
}
