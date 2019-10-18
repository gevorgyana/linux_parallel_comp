#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <iostream>

using std::cout;
using std::cin;
using std::endl;

void do_loop();

int main()
{
  do_loop();
}

// TODO check how nanosleep and software clock are related
// what is that thing about rounding errors???

void do_loop()
{
  while (true)
  {
    // record current time
    timespec started, finished;
    clock_gettime(CLOCK_REALTIME, &started);
    
    /** 
     * prepare for select() - some of this code can be put 
     * in larger scope, e.g. timespec, as pselect does not
     * modify it
     **/

    // here it does not matter if the standard input is ready,
    // if not, we will block
    cout << "Please enter what you want to do..." << endl;
    char c;
    cin >> c;

    if (c == 'c')
    {
      // do regular processing (quick) - copy from async branch

      // check how much time we have taken - pselect() call
      // and also processing above are in a way sporadic events
      clock_gettime(CLOCK_REALTIME, &finished);

      // wait for some time (if needed), to preserve periodicity
      
      cout << "SLEEPING" << endl;
      timespec sleep_for;
      sleep_for.tv_sec = 2;
      sleep_for.tv_nsec = 0;
      nanosleep(&sleep_for, NULL);
      
      // TODO does nanosleep reinvokes itself on being interrupted?
      continue;
    }
    if (c == 'w')
    {
      // if is safe frok the logial point of view to just break
      // from the loop and do what we would if there were no control
      // of the program by the user
      cout << "BREAKING" << endl;
      break;
    }
    if (c == 'q')
    {
      // repreat same logic as in the async branch
      cout << "QUIT - TERMINATE" << endl;
      break;
    }
  }
}
