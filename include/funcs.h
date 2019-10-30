#pragma once

#include <sys/select.h>
#include <stdbool.h>

void report(const int *results);

void stop_child_processes(const int *children_pids);

void refresh_read_fds(fd_set *reads, const int *my_fds, const int *results);

void restore_terminal_settings();

void prepare_terminal();

// TODO prettify this mess and TODO rename parameters
bool process_data_quickly(int nfd, int *results, fd_set *reads, const int *my_fds,
                        int *children_pids, int *ready_cnt);
