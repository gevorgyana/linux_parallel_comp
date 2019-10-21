#pragma once

#include <termios.h>

// used to generate fifos in /tmp directory
#define FIFO_TEMPLATE "/tmp/fifo%d"

// number of children
#define n 2

// sleep seconds
#define SLEEP_FOR_SEC 2
#define SLEEP_FOR_NSEC 0

/*
// message passed to children
struct message_to_child;

// return value retrieved from children
// is stored in this struct
struct message_from_child;
*/

struct message_to_child
{
  int value;
};

struct message_from_child
{
  int value;
};

/** 
 * original terminal settings,
 * will be restored when the application
 * exits normally
 */
struct termios original_settings;
