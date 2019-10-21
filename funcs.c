#include "common.h"
#include "funcs.h"

#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

void report(const int* results)
{
  // calculate minimum of the return
  // values that we have
  unsigned int imin_result;
  for (int i = 0; i < n; ++i) {
    if (i == 0)
    {
      imin_result = results[i];
      continue;
    }
    imin_result = (imin_result < results[i] ? imin_result : results[i]);
  }

  printf("The answer: %u\n\r", imin_result);
}

void stop_child_processes(const int* children_pids)
{
  for (int j = 0; j < n; ++j) {
    kill(children_pids[j], SIGTERM);
  }
}

void restore_terminal_settings()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &original_settings);
}

void prepare_terminal()
{
  // store previous settings
  tcgetattr(STDIN_FILENO, &original_settings);

  // disable echoing and line-by-line reading
  struct termios temp_termios;
  temp_termios.c_lflag &= (~ICANON);
  temp_termios.c_cc[VMIN] = 1;
  temp_termios.c_cc[VTIME] = 0;
  temp_termios.c_lflag &= (~ECHO);

  /**
   * By default, newline character is converted to '\n\r'
   * In non-canonical mode, this is no longer desirable
   */
  temp_termios.c_oflag &= (~OPOST);

  // apply new settings immediately
  tcsetattr(STDIN_FILENO, TCSANOW, &temp_termios);

  // on exit, restore original settings
  atexit(restore_terminal_settings);
}
