#include <sys/select.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

// relative headers
#include "handlers.h"

// used to generate fifos in /tmp directory
#define FIFO_TEMPLATE "/tmp/fifo%d"

// n is the number of children
#define n 2

// struct that a child receives
struct msg {
  int value;
};
// struct that a parent sends
struct ans {
  int value;
};

int main() {
  // signal mask for SIGCHLD
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGCHLD);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  // TODO set the terminal to raw mode - so that
  // I can read single characters
  // for now, read one character from tty,
  // attached to the current process

  // these hold file descriptors for FIFOs
  int my_fds[n];

  // this holds process ids ofchildren processes
  int children_pids[n];

  // signal handler for catching zombies
  signal(SIGCHLD, HReapZombies);

  umask(0);

  int pfd[2];
  pipe(pfd);
  int dummy_var;

  for (int i = 0; i < n; ++i) {
    // prepare filepath of FIFO
    char fifo_filepath[sizeof(FIFO_TEMPLATE) + 10];
    snprintf(fifo_filepath, sizeof(FIFO_TEMPLATE) + 10, FIFO_TEMPLATE, (unsigned int) i);

    mkfifo(fifo_filepath, S_IRUSR | S_IWUSR | S_IWGRP);

    // open fifo to read from it later
    int rfd = open(fifo_filepath, O_RDONLY | O_NONBLOCK);
    my_fds[i] = rfd;

    // send a messge to a child
    int wfd = open(fifo_filepath, O_WRONLY);
    struct msg yo_msg = {1};
    write(wfd, &yo_msg, sizeof(struct msg));

    int success = fork();
    if (success == 0) {
      // close read end - see comment
      // above about synchronization pipe
      close(pfd[0]);

      // read data from parent
      int fd = open(fifo_filepath, O_RDONLY);
      struct msg my_msg;
      read(fd, &my_msg, sizeof(struct msg));

      // ready to report that we have finished - see comment
      // above about synchronization pipe
      close(pfd[1]);

      // useful work here
      sleep(2 * i + 2);

      // reponse
      struct ans a;
      a.value = i + 1;

      /**
       * UNCOMMENT IF YOU WANT TO
       * EXPERIMENT WITH SHORT-CIRCUIT
       * EVALUATION

       if (i == 1) {
         a.value = 0;
       }
       */
      
      int wdf = open(fifo_filepath, O_WRONLY);
      write(wdf, &a, sizeof(struct ans));

      _exit(0);
    }
    
    // remember the pid, we will need it
    children_pids[i] = success;
  }

  // MAIN BLOCK

  // close fd to synchronise yourself with children -
  // see comment above about synchronization pipe

  close(pfd[1]);

  // wait for children to finish reading
  // what they have to read
  if (read(pfd[0], &dummy_var, sizeof(int)) < 0) {
    printf("=== error reading in from sync pipe === \n");
  }

  // return values from children
  int results[n];

  // how many children have reported
  int ready_cnt = 0;

  // ready or not to break the loop?
  int ready_flag = 0;

  // for select system call:

  // sigset we will watch
  fd_set reads;

  // optimization parameter for select
  // set to max value of all file descriptors
  // that we are watching + 1
  int nfd = 0;
  for (int i = 0; i < n; ++i) {
    nfd = (nfd > my_fds[i] ? nfd : my_fds[i]);
  }
  ++nfd;

  for (int i = 0; i < n; ++i) {
    results[i] = -1;
  }

  while (!ready_flag) {

    // prepare file descriptors of interest
    FD_ZERO(&reads);
    for (int i = 0; i < n; ++i) {

      // have already read response
      if (results[i] >= 0) {
        continue;
      }

      FD_SET(my_fds[i], &reads);
    }

    // init file descriptors to watch them
    FD_SET(STDIN_FILENO, &reads);

    // do select
    int ret_ = pselect(nfd, &reads, NULL, NULL, NULL, &sigset);

    // someone wants to report
    if (FD_ISSET(STDIN_FILENO, &reads)) {

      char c;
      scanf("%c", &c);

      if (c == 'q') {

        // the user wanted to quit
        // but maybe we can repotr ot him now?
        // let's check that

        bool can_report_before_quitting = true;

        for (int i = 0; i < n && can_report_before_quitting; ++i) {
          can_report_before_quitting =
              can_report_before_quitting && (results[i] >= 0);
        }

        if (can_report_before_quitting) {
          for (int j = 0; j < n; ++j) {
            kill(children_pids[j], SIGTERM);
          }

          for (int i = 0; i < n; ++i) {
            printf("%d\n", results[i]);
          }

          // exit with closing all
          // fds that were open automatically
          exit(1);
        }

        for (int j = 0; j < n; ++j) {
          kill(children_pids[j], SIGTERM);
        }
        exit(1);
      }
    }

    for (int i = 0; i < n; i++) {

      // already remembered or not ready to read
      if ((results[i] >= 0) || !(FD_ISSET(my_fds[i], &reads)))
        continue;

      // safe to read here - no blocking
      struct ans a;
      int ret_val = read(my_fds[i], &a, sizeof(struct ans));

      // app logic
      if (ret_val > 0) {
        if (a.value == 0) {
          printf("NULL\n");

          for (int j = 0; j < n; ++j) {
            kill(children_pids[j], SIGTERM);
          }
          exit(1);
        }

        results[i] = a.value;
        ++ready_cnt;
      }
    }

    if (ready_cnt == n)
      ready_flag = 1;

  }

  for (int i = 0; i < n; ++i) {
    printf("%d\n", results[i]);
  }

  // unblock signals from children
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);

  int pid;
  while ((pid = wait(NULL)) > 0) {

    printf("main waited for %d\n", pid);
    continue;
  }
  return 0;
}
