// c headers
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <aio.h>
#include <sys/select.h>

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
      int rfd = open(str.c_str(), O_RDONLY | O_NONBLOCK);
      my_fds[i] = rfd;
      
      int wfd = open(str.c_str(), O_WRONLY);
      msg yo_msg = {1};
      write(wfd, &yo_msg, sizeof(msg));
      
      int success = fork();
      if (success == 0)
	{

	  // close read end
	  close(pfd[0]);

	  // READ DATA FROM PARENT
	  int fd = open(str.c_str(), O_RDONLY);
	  msg my_msg;
	  read(fd, &my_msg, sizeof(msg));

	  // ready to report that we have finished
	  close(pfd[1]);
	  
	  // useful work here

	  // RESPOND
	  ans a = {i};
	  int wdf = open(str.c_str(), O_WRONLY);
	  write(wdf, &a, sizeof(ans));
	  
	  _exit(0);
	}
      std::cout << "AFTER FORK " << std::endl;
      children_pids[i] = success;
    }

  // MAIN BLOCK
  // close fd to sync
  close(pfd[1]);

  // todo - wait for children later; and do not block
  // wait for children
  read(pfd[0], &dummy_var, sizeof(int));
  
  int results[n];
  int ready_cnt = 0;
  int ready_flag = 0;

  /*
    the only thing i need to be sure about is that 
    all the children have read their message
    this is what unnamed pipe is for
    then use select to watch for fifos
   */

  // test - try to read from stdin
  // it may get interrupted by a signal

  // for select system call
  fd_set reads;
  struct timespec out;
  out.tv_sec = 1;
  out.tv_nsec = 0;
  int nfd = 0;
  for (int i = 0; i < n; ++i)
    {
      nfd = (nfd > my_fds[i] ? nfd : my_fds[i]);
    }
  ++nfd;
  std::cout << "NFD VALUE: " << nfd<< std::endl;
  
  for (int i = 0; i < n; ++i)
    {
      results[i] = 0;
    }

  /*
    todo for async input case 
    use select()
    inside the main block
    to be able to fetch input from n + 1 decriptors
    where n is the amnt of children
    the other one is for stdin (do in in raw mode, btw)
    you want to wait on the sync pipe (usual pipe; named)! 
    otherwise race condition
   */
  
  while (!ready_flag)
    {
      
      // prepare signal mask
      sigset_t sigset;
      sigemptyset(&sigset);
      sigaddset(&sigset, SIGCHLD);

      // prepare file descriptors of interest
      FD_ZERO(&reads);
      for (int i = 0; i < n; ++i)
	{
	  FD_SET(my_fds[i], &reads);
	}
      FD_SET(STDIN_FILENO, &reads);

      int ret_ = pselect(nfd, &reads, NULL, NULL, &out, &sigset);
      std::cout << "--RETVAL" << ret_ << std::endl;
      
      for (int i = 0; i < n; ++i)
	{
	  std::cout << "IS_SET (T/F): " << FD_ISSET(my_fds[i], &reads) << std::endl;
	}
      sleep(2);

      FD_ZERO(&reads);
      for (int i = 0; i < n; ++i)
	{
	  FD_SET(my_fds[i], &reads);
	}
      FD_SET(STDIN_FILENO, &reads);
      
      int ret__ = pselect(nfd, &reads, NULL, NULL, &out, &sigset);
      std::cout << "--RETVAL" << ret__ << std::endl;
      
      for (int i = 0; i < n; ++i)
	{
	  std::cout << "IS_SET (T/F): " << FD_ISSET(my_fds[i], &reads) << std::endl;
	}

      // use select to watch fds (fifos and stdin)
      // if there are some,
      // check if it is user input
      
      // if it is input, terminate nicely
      // but if you were able to perform the computation,
      // report it

      // if not, process
      // if SCE, process and terminate
      // if not, just read and remember 
      
      // deprecates soooooon
      
      for (int i = 0; i < n; i++)
	{
	  if (results[i])
	    continue;
	  ans a;
	  int ret_val = read(my_fds[i], &a, sizeof(ans));
	  if (ret_val > 0)
	    {
	      results[i] = 1;
	      ++ready_cnt;
	    }
	}
      if (ready_cnt == n)
	ready_flag = 1;
      std::cout << "score for next iter " << ready_cnt << std::endl;
      
    }

  for (int i = 0; i < n; ++i)
    {
      std::cout << results[i] << std::endl;
    }

  int pid;
  while ((pid = wait(NULL)) > 0)
    {
      std::cout << "main waited for " << pid << std::endl;
      continue;
    }
  return 0;
}
