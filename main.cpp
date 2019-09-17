#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

// cpp headers
#include <iostream>
#include <unordered_map>

// append a number to this string and you get the filepath
#define SERVER_FIFO "/home/i516739/fifo"

// handler for reaping zombies
void c_handler(int dummy)
{
  int atom;
  while ((atom = wait(NULL)) > 0)
    {
      std::cout << "unsafe hit " << atom << std::endl;
      continue;
    }
}

// handle the case when blocking operation is interrupted by a signal! - use sigaction and
// specify the option that you want the operation to be tried again once it is interrupted

// we can allow reading to be opened as non-blocking operation
// but opening for writing is not allowed in this way!

// if we have one fifo (shared across every child process)
// then we need to make sure that data is understood correctly
// (consider this case: proc A writes x to B and C; B then writes its
// response just before C read its x value; we just got things mixed u// p. You need to synchronise this stuff)

// n is the number of children
#define n 3

// struct that a client reveives
struct msg
{
  int value;
};

// struct that a server sends
struct ans
{
  int value;
};

// contiainer that holds the results from children
std::unordered_map<int,int> answer_container;

// make it atomic volatile? FLAG
// user has cancelled operation?
static int cancel_flag = 0;

// make it atomic volatile? FLAG
static int ready_flag = 0;

// atomic volatile? PLAIN COUNTER
static int ready_counter = 0;

// sig handler for cancel: just sets a global variable
// to true so that when then can check it inside of the
// main loop
void UserWantsToCancel(int dummy)
{
  cancel_flag = 1;
}

int main()
{
  // open fifos
  int my_fds[n];
  // array that holds file decriptors:
  int children_pids[n];

  // signal handler for catching zombies
  signal(SIGCHLD, c_handler);

  // todo: maybe no sense in calling this?
  umask(0);
  
  for (int i = 0; i < n; ++i)
    {
      // do it in the current directory, no hardcoding
      std::string str = SERVER_FIFO + std::to_string(i);
      
      // todo: use other permissions, we just create regular file in
      // current diirectory
      mkfifo(str.c_str(), S_IRUSR | S_IWUSR | S_IWGRP);

      int rfd = open(str.c_str(), O_RDONLY | O_NONBLOCK);
      my_fds[i] = rfd;
      
      int success = fork();
      if (success == 0)
	{
	  // it is safe to use non-blocking read here
	  int fd = open(str.c_str(), O_RDONLY | O_NONBLOCK);
	  msg my_msg;
	  read(fd, &my_msg, sizeof(msg));


	  // useful work here
	  sleep(1);

	  // safe to write, parent already has 1 open fd
	  ans a = {i};
	  int wdf = open(str.c_str(), O_WRONLY);
	  write(wdf, &a, sizeof(ans));
	   
	  // exit with this call _exit(0), to avoid closing the parent's descriptors??
	  _exit(0);
	}

      children_pids[i] = success;
      int wfd = open(str.c_str(), O_WRONLY);
      msg yo_msg = {1};
      write(wfd, &yo_msg, sizeof(msg));
    }

  int pid;
  while ((pid = wait(NULL)) > 0)
    {
      std::cout << "main waited for " << pid << std::endl;
      continue;
    }
  return 0;
}
