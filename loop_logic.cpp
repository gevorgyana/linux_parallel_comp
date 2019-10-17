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

// TODO check how nanosleep and oftware clock are related
// what is that thing about rounding errors???

void do_loop()
{
  while (true)
  {
    // record current time
    timespec started, finished, elapsed;
    clock_gettime(CLOCK_REALTIME, &started);

    cout << started.tv_sec << endl;
    
    /** 
     * prepare for select() - some of this code can be put 
     * in larger scope, e.g. timespec, as pselect does not
     * modify it
     **/

    // set timout
    timespec ts;
    ts.tv_sec = 2;
    ts.tv_nsec = 0;
    
    // prepare fd_set
    fd_set read_from;
    FD_SET(STDIN_FILENO, &read_from);

    // prepare nfd
    int nfd = STDIN_FILENO + 1;

    // sigmask
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGCHLD);
    
    // do select - just to take time in this case
    pselect(nfd, &read_from, NULL, NULL, &ts, &sigset);

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
      // TODO does nanosleep reinvokes itself on being interrupted?
      clock_gettime(CLOCK_REALTIME, &finished);
      elapsed.tv_sec = finished.tv_sec - started.tv_sec;
      elapsed.tv_nsec = finished.tv_nsec - started.tv_nsec;
      nanosleep(&elapsed, NULL);
      
      continue;
    }
    if (c == 'w')
    {
      // if is safe frok the logial point of view to just break
      // from the loop and do what we would if there were no control
      // of the program by the user
      break;
    }
    if (c == 'q')
    {
      // repreat same logic as in the async branch
      ;
    }
  }
}
