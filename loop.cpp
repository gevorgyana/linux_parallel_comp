// c headers
#include <aio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
// cpp headers
#include <iostream>
#include <unordered_map>
// relative headers
#include "handlers.hpp"
// append a number to this string and you get the filepath
#define FIFO_TEMPLATE "/home/i516739/fifo"
// n is the number of children
#define n 3
// struct that a child recieves
struct msg {
  int value;
};
// struct that a parent sends
struct ans {
  int value;
};

#define LOG

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
*/

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

  // todo: maybe no sense in calling this?
  // umask(0);

  // pipe for synchronisation - when
  // all children close it, we
  // can read EOF in the main process => works as
  // a kind of a barrier
  int pfd[2];
  pipe(pfd);

  // dummy variable to read from synchronisation pipe
  int dummy_var;
#ifdef LOG
  std::cout << "CREATED PIPE" << std::endl;
#endif

  for (int i = 0; i < n; ++i) {
    // SETUP PART

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
      // close read end - see comment
      // above about synchronization pipe
      close(pfd[0]);

      // read data from parent
      int fd = open(str.c_str(), O_RDONLY);
      msg my_msg;
      read(fd, &my_msg, sizeof(msg));

      // ready to report that we have finished - see comment
      // above about synchronization pipe
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
#ifdef LOG
    std::cout << "AFTER FORK " << std::endl;
#endif
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
    std::cout << "=== error reading in from sync pipe === " << std::endl;
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
#ifdef LOG
  std::cout << "NFD VALUE: " << nfd << std::endl;
#endif

  // TODO remove this? what is it for?
  for (int i = 0; i < n; ++i) {
    results[i] = -1;
  }

  timespec period;
  period.tv_sec = 2;
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

    int ret_;
    if (prompt_flag) // in this case it is desirable to
                     // preserve periodicity
    {
      // TODO something else?
      nanosleep(&period, NULL);
    }
    
    ret_ = pselect(nfd, &reads, NULL, NULL, NULL, &sigset);
    

    // DO APPLICATION LOGIC
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

    if (ready_cnt == n)
      break;

    if (prompt_flag)
    {
      char control_char;
      while (true)
      {
        std::cout
            << "Please tell me what you want to do."
            << std::endl
            << "(c - contirnue)"
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
          // one is not ready yet
          std::cout << "QUIT" << std::endl;

          // exit here - no need to break
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

  // unblock signals from children
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);

  int pid;
  while ((pid = wait(NULL)) > 0) {
    std::cout << "main waited for " << pid << std::endl;
    continue;
  }
  return 0;
}

