#pragma once

#include <sys/select.h>

void report(const int *results);

void stop_child_processes(const int *children_pids);

void restore_terminal_settings();

void prepare_terminal();
