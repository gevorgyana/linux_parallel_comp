#pragma once

#include <sys/select.h>
#include <stdbool.h>

void report(const int *results);

void stop_child_processes(const int *children_pids);

void refresh_read_fds(fd_set *reads, const int *child_pipes_fds, const int *results);

void restore_terminal_settings();

void prepare_terminal();

bool fetch(int nfd, int *results, fd_set *reads, const int *child_pipes_fds,
                   int *children_pids, int *ready_cnt);

bool fetch_quick(int nfd, int *results, fd_set *reads, const int *child_pipes_fds,
                 int *children_pids, int *ready_cnt);
