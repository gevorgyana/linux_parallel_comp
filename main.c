#include <sys/select.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "common.h"
#include "funcs.h"
#include "handlers.h"
#include "demofuncs.h"

// TODO create table with test cases as the task says to do

/** 
 * original terminal settings,
 * will be restored when the application
 * exits normally
 */


void run_test_case(int test_case_id)
{
  
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

    umask(0);

    /**
     * processes in the group need write permissions;
     * default umask value prevents giving write
     * permissions to group
     */
    umask(0);

    /**
     * Synchronization pipe works as a kind of barrier:
     * when every process closes their write end,
     * the parent stops blocking on the pipe and
     * in this way synchronizes itself with children
     */

    int pfd[2];
    pipe(pfd);
    int dummy_var;

    for (int i = 0; i < n; ++i) {

      // prepare filepath of FIFO
      char fifo_filepath[sizeof(FIFO_TEMPLATE) + 10];
      snprintf(fifo_filepath, sizeof(FIFO_TEMPLATE) + 10, FIFO_TEMPLATE, (unsigned int) i);
    
      mkfifo(fifo_filepath, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);

      // open FIFO to read from it later
      int rfd = open(fifo_filepath, O_RDONLY | O_NONBLOCK);
      my_fds[i] = rfd;

      // send a messge to a child
      int wfd = open(fifo_filepath, O_WRONLY);

      struct message_to_child msg_ = {1};
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
      
        switch(i)
        {
          case 0:
            response.value = f_func_imin(test_case_id);
            break;
          case 1:
            response.value = g_func_imin(test_case_id);
            break;
        }

        int wdf = open(fifo_filepath, O_WRONLY);
        write(wdf, &response, sizeof(struct message_from_child));

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

    while (true) {

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
        struct message_from_child a;
        int ret_val = read(my_fds[i], &a, sizeof(struct message_from_child));

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

      if (ready_cnt == n) // break from the main loop, as
        // the manager has completed its task
        break;
    }

    report(results);

    // unblock signals from children
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);

    int pid;
    while ((pid = wait(NULL)) > 0) {

      printf("main waited for %d\n", pid);
      continue;
    }

  
}

int main()
{
  run_test_case(0);
  return 0;
}

