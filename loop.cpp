#include <aio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <unordered_map>

#include "handlers.hpp"

#define FIFO_TEMPLATE "/tmp/fifo"

// n is the number of children
#define n 3

// seconds
#define SLEEP_FOR 2

struct msg {
  int value;
};

struct ans {
  int value;
};

/**
 * currenly not used - for setting tty raw mode
 *
struct termios orig_termios;
void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

  // TODO set the terminal to raw mode - so that
  // I can read single characters
  // for now, read one character from tty,
  // attached to the current process

*/

int main() {
  // signal mask for SIGCHLD
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGCHLD);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  // this holds file descriptors for FIFOs
  int my_fds[n];

  // this holds process ids ofchildren processes
  int children_pids[n];

  // signal handler for catching zombies
  signal(SIGCHLD, HReapZombies);

  // TODO: maybe no sense in calling this?
  umask(0);

  // slef-pipe trick
  int pfd[2];
  pipe(pfd);
  int dummy_var;

  for (int i = 0; i < n; ++i) {
    
    // TODO: do it in current directory instead
    std::string str = FIFO_TEMPLATE + std::to_string(i);

    // TODO: use other permissions, we just create
    // regular file in current diirectory (we will do so)
    mkfifo(str.c_str(), S_IRUSR | S_IWUSR | S_IWGRP);

    // open fifo to read from it later
    int rfd = open(str.c_str(), O_RDONLY | O_NONBLOCK);
    my_fds[i] = rfd;

    // send a messge to a child
    int wfd = open(str.c_str(), O_WRONLY);
    msg yo_msg = {1};
    write(wfd, &yo_msg, sizeof(msg));

    int success = fork();
    if (success == 0) {
      
      close(pfd[0]);

      // read data from parent
      int fd = open(str.c_str(), O_RDONLY);
      msg my_msg;
      read(fd, &my_msg, sizeof(msg));

      // slef-pipe trick - after this we can read
      // the results from parent process
      close(pfd[1]);

      // useful work here
      sleep(2 * i + 2);

      // reponse
      ans a;
      a.value = i + 1;

      int wdf = open(str.c_str(), O_WRONLY);
      write(wdf, &a, sizeof(ans));

      _exit(0);
    }
    
    children_pids[i] = success;
  }

  // self-pipe trick - synh yourself with child processes
  close(pfd[1]);

  // wait for children to finish reading
  if (read(pfd[0], &dummy_var, sizeof(int)) < 0) {
    std::cout << "=== error reading in from sync pipe === " << std::endl;
  }

  // return values from children
  int results[n];

  // how many children have reported
  int ready_cnt = 0;

  // ready to break the loop or not?
  int ready_flag = 0;

  // sigset we will use to wait for input
  // from pipes
  fd_set reads;

  // optimization parameter for select
  int nfd = 0;
  for (int i = 0; i < n; ++i) {
    nfd = (nfd > my_fds[i] ? nfd : my_fds[i]);
  }
  ++nfd;

  // TODO remove this? 
  for (int i = 0; i < n; ++i) {
    results[i] = -1;
  }

  // period for sleeping in between processing and user prompt
  timespec period;
  period.tv_sec = SLEEP_FOR;
  period.tv_nsec = 0;

  bool prompt_flag = true;

  using std::cout;
  using std::endl;
  
  while (true)
  {
      
    // prepare file descriptors of interest
    FD_ZERO(&reads);
    for (int i = 0; i < n; ++i) {

      // have already read response -> leave it alone
      if (results[i] >= 0) {
        continue;
      }

      FD_SET(my_fds[i], &reads);
    }

    if (prompt_flag) // in this case it is desirable to
                     // preserve periodicity
    {
      // TODO something else? - nanosleep sleeps for more than needed. why????
      timespec started, finished, elapsed;
      clock_gettime(CLOCK_REALTIME, &started);
      nanosleep(&period, NULL);
      clock_gettime(CLOCK_REALTIME, &finished);
      elapsed.tv_sec = finished.tv_sec - started.tv_sec;
      elapsed.tv_nsec = finished.tv_nsec - started.tv_nsec;
      if (elapsed.tv_nsec < 0)
      {
        --elapsed.tv_sec;
        elapsed.tv_nsec = 999999999 + elapsed.tv_nsec;
      }
      cout << elapsed.tv_sec << ' ' << elapsed.tv_nsec << endl;
    }
    
    pselect(nfd, &reads, NULL, NULL, NULL, NULL);
    
    for (int i = 0; i < n; i++) {

      // already remembered or not ready to read
      if ((results[i] >= 0) || !(FD_ISSET(my_fds[i], &reads)))
        continue;

      // safe to read here - no blocking
      ans a;
      int ret_val = read(my_fds[i], &a, sizeof(ans));

      if (ret_val > 0) {
        
        if (a.value == 0) {
          std::cout << "NULL" << std::endl;
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

    if (prompt_flag)
    {
      char control_char;
      while (true)
      {
        std::cout
            << "Please tell me what you want to do."
            << std::endl
            << "(c - continue)"
            << std::endl
            << "(w - continue w/o prompt)"
            << std::endl
            << "(q - quit)"
            << std::endl;
                                                             
        std::cin >> control_char; // blocking here
        
        if (control_char == 'c')
        {
         break;
        }
        else if (control_char == 'q')
        {
          // special case
          // if possible, show results (if function is ready
          // to be calculated immediately); if not, report which
          // one is not ready yet !!!!! TODO

          // refresh info about children processes

          cout << "I" << endl;
          FD_ZERO(&reads);
          for (int i = 0; i < n; ++i) {

            // have already read response -> leave it alone
            if (results[i] >= 0) {
              continue;
            }

            FD_SET(my_fds[i], &reads);
          }

          cout << "II" << endl;

          /**
           * WHY DO I NEED TO PROVIDE THIS TIMESPEC ARGUMENT? 
           * I THOUGH IT WILL EXECUTE AND RETURN IMMEDIATELY,
           * BUT IT HANGS
          */
          
          timespec dummy_ts;
          dummy_ts.tv_sec = 0;
          dummy_ts.tv_nsec = 0;
          
          pselect(nfd, &reads, NULL, NULL, &dummy_ts, NULL);

          cout << "III" << endl;
          
          for (int i = 0; i < n; i++) {

            // already remembered or not ready to read
            if ((results[i] >= 0) ||
                !(FD_ISSET(my_fds[i], &reads)))
              continue;

            // safe to read here - no blocking
            ans a;
            int ret_val = read(my_fds[i], &a, sizeof(ans));

            if (ret_val > 0) {
        
              if (a.value == 0) {
                std::cout << "NULL" << std::endl;
                for (int j = 0; j < n; ++j) {
                  kill(children_pids[j], SIGTERM);
                }
                exit(1);
              }

              results[i] = a.value;
              ++ready_cnt;
            }
          }

          cout << "IV" << endl;

          bool can_report_before_quitting = true;
          
          for (int i = 0; i < n && can_report_before_quitting; ++i)
          {
            can_report_before_quitting =
                can_report_before_quitting && (results[i] >= 0);
          }

          cout << "V" << endl;

          if (can_report_before_quitting)
          {
            for (int j = 0; j < n; ++j) {
              kill(children_pids[j], SIGTERM);
            }

            for (int i = 0; i < n; ++i) {
              std::cout << results[i] << std::endl;
            }
            exit(1);
            
          }
          cout << "VI" << endl;
          

          cout << "System was not able to calculate the result."
               << endl
               << "The following values are not yet known:"
               << endl;
          for (int j = 0; j < n; ++j)
          {
            if (results[j] == -1)
            {
              cout << "Function #"
                   << j << endl;
            }
          }
          
          for (int j = 0; j < n; ++j) {
            kill(children_pids[j], SIGTERM);
          }
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

  for (int i = 0; i < n; ++i) {
    std::cout << results[i] << std::endl;
  }

  sigprocmask(SIG_UNBLOCK, &sigset, NULL);

  // TODO remove signal handlers - you block
  // every SIGCHLD on initialization! no need to handle
  // these signals, as you wait for every child here
  // check that and if valid, remove hadlers!!!
  int pid;
  while ((pid = wait(NULL)) > 0) {
    std::cout << "main waited for " << pid << std::endl;
    continue;
  }
  
  return 0;
}

