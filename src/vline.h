#ifndef __VLINE_H__

#define __VLINE_H__

#include <stddef.h>

typedef struct {
    size_t len;
    size_t cap;
    char** vec;
} vline_completions;

typedef struct {
    char* buf;
    size_t buf_len;
    int stdin_fd;
    int stdout_fd;
    const char* prompt;
    size_t prompt_len;
    size_t num_cols;
    size_t pos;
    size_t len;
    int history_idx;
    int in_completion;
    size_t completion_idx;
} vline;

extern char* vline_more;

int vline_init(vline* vl, int stdin_fd, int stdout_fd, char* buf,
               size_t buf_len, const char* prompt);
char* vline_edit(vline* vl);
void vline_hide(vline* vl);
void vline_show(vline* vl);
void vline_stop(vline* vl);

int vline_history_add(const char* line);
int vline_history_save(const char* file);
void vline_history_free(void);

typedef void vline_completion_callback(const char* line, vline_completions* completions);
typedef char* vline_hints_callback(const char* line, int* color, int* bold);
typedef void vline_free_hint_callback(void* hint);

void vline_set_completion_callback(vline_completion_callback* callback);
void vline_set_hints_callback(vline_hints_callback* callback);
void vline_set_free_hint_callback(vline_free_hint_callback* callback);
int vline_add_completion(vline_completions* completions, const char* str);

#endif /* __VLINE_H__ */
