#pragma once

#include <sys/select.h>

void Report(const int* results);

void StopChildProcesses(const int* children_pids);

void RefreshReadFds(fd_set* reads,
                         const int* my_fds,
                    const int* results);


void RestoreTerminalSettings();

void PrepareTerminal();

// TODO prettify this mess and TODO rename parameters
void ProcessDataQuickly(int nfd, int* results,
                        fd_set* reads, const int* my_fds,
                        int* children_pids, int* ready_cnt);
