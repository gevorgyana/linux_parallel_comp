// c headers
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <aio.h>

// cpp headers
#include <iostream>
#include <unordered_map>

// relative headers
#include "handlers.hpp"

// =========================

// append a number to this string and you get the filepath
#define FIFO_TEMPLATE "/home/i516739/fifo"
// n is the number of children
#define n 3

// struct that a child recieves
struct msg
{
  int value;
};

// struct that a parent sends
struct ans
{
  int value;
};

// GLOBAL VARS
// ========================

volatile sig_atomic_t cancel_requested;

int main()
{
  // TODO set the terminal to raw mode - so that I can read single characters
  // for now, just create a separate process
  // that will read from console
  
  // open fifos
  int my_fds[n];
  
  // array that holds file decriptors:
  int children_pids[n];

  // signal handler for catching zombies
  signal(SIGCHLD, HReapZombies);

  // todo: maybe no sense in calling this?
  umask(0);

  // pipe for synchronisation
  int pfd[2];
  pipe(pfd);
  int dummy_var;
  std::cout << "CREATED PIPE" << std::endl;
  
  for (int i = 0; i < n; ++i)
    {
      // SETUP
      // do it in the current directory
      std::string str = FIFO_TEMPLATE + std::to_string(i);
      // todo: use other permissions, we just create regular file in
      // current diirectory
      mkfifo(str.c_str(), S_IRUSR | S_IWUSR | S_IWGRP);

      // OPEN FIFO TO READ LATER
      std::cout << getpid() << "-- open read fifo parent" << std::endl;
      int rfd = open(str.c_str(), O_RDONLY | O_NONBLOCK);
      std::cout << getpid() << "-- opened read fifo parent" << std::endl;
      my_fds[i] = rfd;

      std::cout << "==parent about to open for writing" << std::endl;
      int wfd = open(str.c_str(), O_WRONLY);
      std::cout << "==parent opened for writing " << std::endl;
      msg yo_msg = {1};
      std::cout << "= = started writing " << i << std::endl;
      write(wfd, &yo_msg, sizeof(msg));
      std::cout << "= = = finished writing" << i << std::endl;
      
      int success = fork();
      if (success == 0)
	{

	  // close read end
	  close(pfd[0]);

	  // READ DATA FROM PARENT
	  std::cout << getpid() << "child started open read" << std::endl;
	  int fd = open(str.c_str(), O_RDONLY);
	  std::cout << getpid() << "child finished open read" << std::endl;
	  msg my_msg;
	  std::cout << getpid() << "child started read" << std::endl;
	  // sometimes hangs here
	  // the data has been "stolen" by the parent process
	  read(fd, &my_msg, sizeof(msg));
	  std::cout << getpid() << "child finished read" << std::endl;

	  // useful work here

	  // RESPOND
	  ans a = {i};
	  std::cout << getpid() << "child started open write" << std::endl;
	  int wdf = open(str.c_str(), O_WRONLY);
	  std::cout << getpid() << "child finished open write" << std::endl;
	  std::cout << getpid() << "child started write" << std::endl;
	  write(wdf, &a, sizeof(ans));
	  std::cout << getpid() << "child finished write" << std::endl;

	  // ready to report that we have finished
	  close(pfd[1]);
	  
	  _exit(0);
	}
      std::cout << "AFTER FORK " << std::endl;
      children_pids[i] = success;
    }

  std::cout << "MAIN BLOCK" << std::endl;

  // MAIN BLOCK
  // close fd to sync
  close(pfd[1]);
  // wait for children
  std::cout << "parent has started waiting..." << std::endl;
  read(pfd[0], &dummy_var, sizeof(int));
  std::cout << "parent has waited for all children" << std::endl;
  
  int results[n];
  int ready_cnt = 0;
  int ready_flag = 0;

  /*
  for (int i = 0; i < n; ++i)
    {
      results[i] = 0;
    }
  */

  /*
    race condition 
    we have "stolen" the x value from a child
    if has not yet read it, but we already expect to read 
    from the fifo -> by the time the child opens its read end,
    it does not see anything and hangs

    => decided to use unnamed pipes for synchronization
   */

  /*
  while (!ready_flag)
    {
      for (int i = 0; i < n; i++)
	{
	  if (results[i])
	    continue;
	  ans a;
	  //std::cout << "<" << std::endl;
	  int ret_val = read(my_fds[i], &a, sizeof(ans));
	  //std::cout << ">" << std::endl;
	  if (ret_val > 0)
	    {
	      results[i] = 1;
	      ++ready_cnt;
	    }
	  //std::cout << "currently counter is" << ready_cnt << std::endl;
	}
      if (ready_cnt == n)
	ready_flag = 1;
      std::cout << "next iter" << ready_cnt << std::endl;
    }
  
  
  // if we survived untill this point, report results
  for (int i = 0; i < n; ++i)
    {
      std::cout << results[i] << std::endl;
    }
  */

  int pid;
  while ((pid = wait(NULL)) > 0)
    {
      std::cout << "main waited for " << pid << std::endl;
      continue;
    }
  return 0;
}
