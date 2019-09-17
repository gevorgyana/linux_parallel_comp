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
  
  for (int i = 0; i < n; ++i)
    {
      // do it in the current directory, no hardcoding
      std::string str = FIFO_TEMPLATE + std::to_string(i);
      
      // todo: use other permissions, we just create regular file in
      // current diirectory
      mkfifo(str.c_str(), S_IRUSR | S_IWUSR | S_IWGRP);

      int rfd = open(str.c_str(), O_RDONLY | O_NONBLOCK);
      my_fds[i] = rfd;
      
      int success = fork();
      if (success == 0)
	{
	  int fd = open(str.c_str(), O_RDONLY);
	  msg my_msg;
	  read(fd, &my_msg, sizeof(msg));

	  // useful work here
	 

	  // safe to write, parent already has 1 open fd
	  ans a = {i};
	  int wdf = open(str.c_str(), O_WRONLY);
	  write(wdf, &a, sizeof(ans));
	   
	  exit(0);
	}

      children_pids[i] = success;
      int wfd = open(str.c_str(), O_WRONLY);
      msg yo_msg = {1};
      write(wfd, &yo_msg, sizeof(msg));
    }

  int results[n];
  int ready_cnt = 0;
  int ready_flag = 0;

  for (int i = 0; i < n; ++i)
    {
      results[i] = 0;
    }
  
  while (!ready_flag)
    {
      for (int i = 0; i < n; i++)
	{
	  if (!results[i])
	    continue;
	  ans a;
	  std::cout << "<" << std::endl;
	  int ret_val = read(my_fds[i], &a, sizeof(ans));
	  std::cout << ">" << std::endl;
	  if (ret_val > 0)
	    {
	      results[i] = 1;
	      ++ready_cnt;
	    }
	}
      if (!ready_flag && (ready_cnt == n - 1))
	ready_flag = 1;
      std::cout << "next iter" << std::endl;
    }
  
  // if we survived untill this point, report results
  for (int i = 0; i < n; ++i)
    {
      std::cout << results[i] << std::endl;
    }
  
  std::cout << "debug" << std::endl;
  int pid;
  while ((pid = wait(NULL)) > 0)
    {
      std::cout << "main waited for " << pid << std::endl;
      continue;
    }
  return 0;
}
