#include "server.h"
#include <assert.h>
#include <stdint.h>
#include <sys/resource.h>
#include <x86intrin.h>

ServerStats stats_new(void) {
    ServerStats stats = { 0 };
    stats.cycles = vec_new(32, sizeof(uint64_t));
    assert(stats.cycles != NULL);
    stats.times = vec_new(32, sizeof(CmdTime));
    assert(stats.times != NULL);
    return stats;
}

uint64_t rdtsc(void) {
    return __rdtsc();
}

struct rusage get_cur_usage(void) {
    struct rusage res = { 0 };
    getrusage(RUSAGE_SELF, &res);
    return res;
}

CmdTime cmd_time(struct timeval start, struct timeval end) {
    CmdTime time = { 0 };
    time.seconds = end.tv_sec - start.tv_sec;
    time.microseconds = end.tv_usec - start.tv_usec;
    return time;
}

struct timeval add_user_and_sys_time(struct timeval usr, struct timeval sys) {
    struct timeval res = { 0 };
    res.tv_sec = usr.tv_sec + sys.tv_sec;
    res.tv_usec = usr.tv_usec + sys.tv_usec;
    return res;
}

int update_stats(ServerStats* stats, CmdTime* last_time, uint64_t cycles) {
    int push_time, push_cycles;

    push_cycles = vec_push(&(stats->cycles), &cycles);
    if (push_cycles == -1) {
        return -1;
    }
    push_time = vec_push(&(stats->times), last_time);
    if (push_time == -1) {
        return -1;
    }

    stats->num_requests++;

    return 0;
}

