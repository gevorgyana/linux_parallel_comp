#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "demofuncs.h"
#include "funcs.h"
#include "handlers.h"

/**
 * original terminal settings,
 * will be restored when the application
 * exits normally
 */

void run_test_case(int test_case_id) {
  prepare_terminal();

  /**
   * signal mask for SIGCHLD - need to prevent
   * this signal from interrupting function calls
   * like sleeping or waiting for file descriptors
   * to become available with select()
   */
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGCHLD);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  // holds file descriptors for FIFOs
  int my_fds[n];

  // holds process ids of children processes
  int children_pids[n];

  // signal handler for catching zombies
  signal(SIGCHLD, HReapZombies);

  /**
   * processes in the group need write permissions;
   * default umask value prevents giving write
   * permissions to group
   */
  umask(0);

  /**
   * Synchronization pipe works as a kind of barrier:
   * until every process closes their write end,
   * the parent is blocked trying to read from the pipe
   */
  int pfd[2];
  pipe(pfd);
  int dummy_var;

  for (int i = 0; i < n; ++i) {

    // prepare filepath of FIFO
    char fifo_filepath[sizeof(FIFO_TEMPLATE) + 10];
    snprintf(fifo_filepath, sizeof(FIFO_TEMPLATE) + 10, FIFO_TEMPLATE,
             (unsigned int)i);

    mkfifo(fifo_filepath, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);

    // open FIFO to read from it later
    int rfd = open(fifo_filepath, O_RDONLY | O_NONBLOCK);
    my_fds[i] = rfd;

    // send a messge to a child
    int wfd = open(fifo_filepath, O_WRONLY);

    struct message_to_child msg_ = {test_case_id};
    write(wfd, &msg_, sizeof(struct message_to_child));

    int success = fork();

    if (success == 0) {

      close(pfd[0]);

      // read data from parent
      int fd = open(fifo_filepath, O_RDONLY);
      struct message_to_child my_msg;
      read(fd, &my_msg, sizeof(struct message_to_child));

      // synchronization pipe - after this we can read
      // the results from parent process
      close(pfd[1]);

      // reponse
      struct message_from_child response;

      switch (i) {
      case 0:
        /* can also access the variable directly
        response.value = f_func_imin(test_case_id);
        */
        response.value = f_func_imin(my_msg.value);
        break;
      case 1:
        response.value = g_func_imin(my_msg.value);
        break;
      }

      int wdf = open(fifo_filepath, O_WRONLY);
      write(wdf, &response, sizeof(struct message_from_child));

      // avoid calling atexit()-registered functions here
      _exit(0);
    }

    // remember pid of a child
    children_pids[i] = success;
  }

  // close write end of synchronization pipe
  close(pfd[1]);

  // wait for children to finish reading - when
  // they finish, this call returns
  if (read(pfd[0], &dummy_var, sizeof(int)) < 0) {
    printf("=== error reading in from sync pipe === \n\r");
  }

  // return values from child processes
  int results[n];

  // how many child processes have reported
  int ready_cnt = 0;

  // sigset used to wait for input from pipes
  fd_set reads;

  // optimization parameter for select
  int nfd = 0;
  for (int i = 0; i < n; ++i) {
    nfd = (nfd > my_fds[i] ? nfd : my_fds[i]);
  }
  ++nfd;

  for (int i = 0; i < n; ++i) {
    results[i] = -1;
  }

  // this timespec specifies how much manager should sleep
  // in between processing and showing user prompt
  struct timespec period;
  period.tv_sec = SLEEP_FOR_SEC;
  period.tv_nsec = SLEEP_FOR_NSEC;

  // if true, manager should enable prompting
  bool prompt_flag = true;

  while (true) {

    refresh_read_fds(&reads, my_fds, results);

    if (fetch_results(nfd, results, &reads, my_fds, children_pids, &ready_cnt)) // accepted a null operand
    {
      stop_child_processes(children_pids);
      restore_terminal_settings();
      return;
    }


    if (ready_cnt == n) // break from the main loop, as
      // the manager has completed its task
      break;

    if (prompt_flag) {
      char control_char;
      while (true) {
        printf("Please tell me what you want to do.\n\r");
        printf("(c - continue)\n\r");
        printf("(w - continue w/o prompt)\n\r");
        printf("(q - quit)\n\r");

        scanf(" %c",
              &control_char); // blocking here -> manager does not change its
        // status and waits for user to tell what to do

        if (control_char == 'c') {
          /*we may have taken too long waiting for user input; if there is already enough
           information to calculate result, do it*/
          refresh_read_fds(&reads, my_fds, results);

          if (fetch_results(nfd, results, &reads, my_fds, children_pids, &ready_cnt))
          {
            stop_child_processes(children_pids);
            restore_terminal_settings();
            return;
          }

          break;
        } else if (control_char == 'q') {
          refresh_read_fds(&reads, my_fds, results);

          fetch_quick(nfd, results, &reads, my_fds, children_pids,
                             &ready_cnt);

          bool can_report_before_quitting = true;

          for (int i = 0; i < n && can_report_before_quitting; ++i) {
            can_report_before_quitting =
                can_report_before_quitting && (results[i] >= 0);
          }

          if (can_report_before_quitting) {
            stop_child_processes(children_pids);
            report(results);
            restore_terminal_settings();
            return;
          }

          printf("System was not able to calculate the result.\n\r");
          printf("The following values are not yet known:\n\r");
          for (int j = 0; j < n; ++j) {

            if (results[j] >= 0)
              continue;

            char func_code;

            switch (j)
            {
              case 0:
                func_code = 'f';
                break;
              case 1:
                func_code = 'g';
                break;
            }

            printf("Function %c\n\r", func_code);
          }

          stop_child_processes(children_pids);
          restore_terminal_settings();
          return;

        } else if (control_char == 'w') {
          restore_terminal_settings();
          prompt_flag = false;
          break;
        }
      }
    }
  }

  report(results);

  sigprocmask(SIG_UNBLOCK, &sigset, NULL);

  int pid;
  while ((pid = wait(NULL)) > 0) {
    printf("main waiter for %u", pid);
    continue;
  }

  restore_terminal_settings();
}

int main() {

  int opcode;
  char msg[64] = {
    "Enter test number (-1 to exit)\n"
  };

  while (true)
  {
    printf("%s", msg);
    scanf(" %d", &opcode);

    if (opcode == -1)
    {
      break;
    }

    printf("running test case #%d\n", opcode);
    run_test_case(opcode);
    printf("\n");
  }

  return 0;
}
