#include "handlers.h"

#include <stdio.h>
#include <sys/wait.h>

void HReapZombies(int dummy)
{
  int atom;
  while ((atom = wait(NULL)) > 0)
    {
      printf("unsafe hit %u\n", atom);
      continue;
    }
}
