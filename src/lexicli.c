#include "vline.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

void completion(const char* buf, vline_completions* comps) {
    if (buf[0] == 'h') {
        vline_add_completion(comps, "hello");
        vline_add_completion(comps, "hi mom");
    }
}

char* hints(const char* buf, int* color, int* bold) {
    if (strlen(buf) == 2 && strncmp(buf, "hi", 2) == 0) {
        *color = 35;
        *bold = 0;
        return " mom";
    }
    return NULL;
}

char* read_line(void) {
    char* line = NULL;
    vline vl = {0};
    char buf[1024] = {0};
    if (vline_init(&vl, -1, -1, buf, sizeof buf, "lexi> ") == -1) {
        fprintf(stderr, "vline_init\n");
        exit(1);
    }
    for (;;) {
        fd_set read_fds;
        int rv;
        FD_ZERO(&read_fds);
        FD_SET(vl.stdin_fd, &read_fds);

        rv = select(vl.stdin_fd + 1, &read_fds, NULL, NULL, NULL);
        if (rv == -1) {
            fprintf(stderr, "select (errno: %d) %s\n", errno, strerror(errno));
            exit(1);
        } else {
            line = vline_edit(&vl);
            if (line != vline_more) {
                break;
            }
        }
    }
    if (line) {
        vline_history_add(line);
    }
    vline_stop(&vl);
    return line;
}

int main(void) {
    vline_set_completion_callback(completion);
    vline_set_hints_callback(hints);
    for (;;) {
        char* line = read_line();
        if (line == NULL) {
            break;
        }
        if (strlen(line) == 4 && strcmp(line, "exit") == 0) {
            free(line);
            break;
        }
        printf("%s\n", line);
        free(line);
    }
    vline_history_free();
    return 0;
}
