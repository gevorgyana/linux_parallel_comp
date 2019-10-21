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

//struct termios original_settings;

int main() {
  
  PrepareTerminal();
  
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
          response.value = f_func_imin(0);
          break;
        case 1:
          response.value = g_func_imin(0);
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

  // this timespec specifies how much manager should sleep
  // in between processing and showing user prompt
  struct timespec period;
  period.tv_sec = SLEEP_FOR_SEC;
  period.tv_nsec = SLEEP_FOR_NSEC;

  // if true, manager should enable prompting
  bool prompt_flag = true;
  
  while (true)
  {

    RefreshReadFds(&reads, my_fds, results);

    if (prompt_flag) // in this case it is desirable to
                     // preserve periodicity
    {
      // TODO why does nanosleep sleeps for more than needed?
      nanosleep(&period, NULL);
    }
    
    ProcessDataQuickly(nfd, results, &reads, my_fds, children_pids, &ready_cnt);

    if (ready_cnt == n) // break from the main loop, as
                        // the manager has completed its task
      break;

    if (prompt_flag)
    {
      char control_char;
      while (true)
      {
        printf("Please tell me what you want to do.\n\r");
        printf("(c - continue)\n\r");
        printf("(w - continue w/o prompt)\n\r");
        printf("(q - quit)\n\r");
        
        scanf("%c", &control_char); // blocking here -> manager does not change its
                                    // status and waits for user to tell what to do

        if (control_char == 'c')
        {
          break;
        }
        else if (control_char == 'q')
        {
          RefreshReadFds(&reads, my_fds, results);

          ProcessDataQuickly(nfd, results, &reads, my_fds, children_pids, &ready_cnt);
          
          bool can_report_before_quitting = true;
          
          for (int i = 0; i < n && can_report_before_quitting; ++i)
          {
            can_report_before_quitting =
                can_report_before_quitting && (results[i] >= 0);
          }

          if (can_report_before_quitting)
          {
            StopChildProcesses(children_pids);
            Report(results);
            exit(1);
          }

          printf("System was not able to calculate the result.\n\r");
          printf("The following values are not yet known:\n\r");
          for (int j = 0; j < n; ++j)
          {
            if (results[j] == -1)
            {
              printf("Function #%u\n\r", j);
            }
          }
          
          StopChildProcesses(children_pids);
          exit(1);
          
        }
        else if (control_char == 'w')
        {
          prompt_flag = false;
          break;
        }
      }      
    }
  }

  Report(results);

  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
  
  int pid;
  while ((pid = wait(NULL)) > 0) {
    printf("main waiter for %u", pid);
    continue;
  }
  
  return 0;
}

