#include <aio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>

#include <iostream>
#include <unordered_map>

#include "handlers.hpp"

// used to generate fifos in /tmp directory
#define FIFO_TEMPLATE "/tmp/fifo"

// number of children
#define n 3

// seconds
#define SLEEP_FOR 2

struct msg {
  int value;
};

struct ans {
  int value;
};


// original terminal settings,
// will be restored when the application
// exits normally
struct termios original_settings;

void RestoreTerminalSettings()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &original_settings);
}

void PrepapreTerminal()
{
  // store previous settings
  tcgetattr(STDIN_FILENO, &original_settings);

  // disable echoing and enable char-by-char reading
  termios temp_termios;
  temp_termios.c_lflag &= (~ICANON);
  temp_termios.c_cc[VMIN] = 1;
  temp_termios.c_cc[VTIME] = 0;
  temp_termios.c_lflag &= (~ECHO);
  temp_termios.c_oflag &= (~OPOST);

  // apply new settings immediately
  tcsetattr(STDIN_FILENO, TCSANOW, &temp_termios);

  // just in case we press C-C while running
  atexit(RestoreTerminalSettings);
}

int main() {
  PrepapreTerminal();
  
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

  // processes in the group need write permissions
  // in case the process umask forbids it TODO ????
  umask(0);

  // slef-pipe trick
  int pfd[2];
  pipe(pfd);
  int dummy_var;

  for (int i = 0; i < n; ++i) {
    
    // TODO: do it in current directory instead
    std::string str = FIFO_TEMPLATE + std::to_string(i);

    mkfifo(str.c_str(), S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);

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
    std::cout << "=== error reading in from sync pipe === " << "\n\r";
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
      cout << elapsed.tv_sec << ' ' << elapsed.tv_nsec << "\n\r";
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
          std::cout << "NULL" << "\n\r";
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
            << "\n\r"
            << "(c - continue)"
            << "\n\r"
            << "(w - continue w/o prompt)"
            << "\n\r"
            << "(q - quit)"
            << "\n\r";
                                                             
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

          cout << "I" << "\n\r";
          FD_ZERO(&reads);
          for (int i = 0; i < n; ++i) {

            // have already read response -> leave it alone
            if (results[i] >= 0) {
              continue;
            }

            FD_SET(my_fds[i], &reads);
          }

          cout << "II" << "\n\r";

          /**
           * WHY DO I NEED TO PROVIDE THIS TIMESPEC ARGUMENT? 
           * I THOUGH IT WILL EXECUTE AND RETURN IMMEDIATELY,
           * BUT IT HANGS
          */
          
          timespec dummy_ts;
          dummy_ts.tv_sec = 0;
          dummy_ts.tv_nsec = 0;
          
          pselect(nfd, &reads, NULL, NULL, &dummy_ts, NULL);

          cout << "III" << "\n\r";
          
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
                std::cout << "NULL" << "\n\r";
                for (int j = 0; j < n; ++j) {
                  kill(children_pids[j], SIGTERM);
                }
                exit(1);
              }

              results[i] = a.value;
              ++ready_cnt;
            }
          }

          cout << "IV" << "\n\r";

          bool can_report_before_quitting = true;
          
          for (int i = 0; i < n && can_report_before_quitting; ++i)
          {
            can_report_before_quitting =
                can_report_before_quitting && (results[i] >= 0);
          }

          cout << "V" << "\n\r";

          if (can_report_before_quitting)
          {
            for (int j = 0; j < n; ++j) {
              kill(children_pids[j], SIGTERM);
            }

            for (int i = 0; i < n; ++i) {
              std::cout << results[i] << "\n\r";
            }
            exit(1);
            
          }
          cout << "VI" << "\n\r";
          

          cout << "System was not able to calculate the result."
               << "\n\r"
               << "The following values are not yet known:"
               << "\n\r";
          for (int j = 0; j < n; ++j)
          {
            if (results[j] == -1)
            {
              cout << "Function #"
                   << j << "\n\r";
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
    std::cout << results[i] << "\n\r";
  }

  sigprocmask(SIG_UNBLOCK, &sigset, NULL);

  // TODO remove signal handlers - you block
  // every SIGCHLD on initialization! no need to handle
  // these signals, as you wait for every child here
  // check that and if valid, remove hadlers!!!
  int pid;
  while ((pid = wait(NULL)) > 0) {
    std::cout << "main waited for " << pid << "\n\r";
    continue;
  }
  
  return 0;
}

