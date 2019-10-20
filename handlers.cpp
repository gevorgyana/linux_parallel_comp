#include "handlers.hpp"

#include <iostream>
#include <sys/wait.h>

void HReapZombies(int dummy)
{
  int atom;
  while ((atom = wait(NULL)) > 0)
    {
      std::cout << "unsafe hit " << atom << "\n\r";
      continue;
    }
}
