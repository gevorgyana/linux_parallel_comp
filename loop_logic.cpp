#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <iostream>

using std::cout;
using std::cin;
using std::endl;

#define MAX_NSEC_IN_TIMEVAL 999999999

void do_loop();

void preserve_period(const timespec* period, const timespec* elapsed)
{
  timespec sleep_for;

  cout << period->tv_sec << ' ' << period->tv_nsec << endl;
  cout << elapsed->tv_sec << ' ' << elapsed->tv_nsec << endl;

  // should not happen, but still there is no sense in
  // waiting even more, when we are already late
  if (period->tv_sec < elapsed->tv_sec
      ||
      (
          period->tv_sec == elapsed->tv_sec
          &&
          period->tv_nsec < elapsed->tv_nsec
      )
     )
  {
    cout << "HI" << endl;
    return;
  }
  
  sleep_for.tv_sec = period->tv_sec - elapsed->tv_sec;
  sleep_for.tv_nsec = period->tv_nsec - elapsed->tv_nsec;

  if (sleep_for.tv_nsec < 0)
  {
    --sleep_for.tv_sec; 
    sleep_for.tv_nsec = MAX_NSEC_IN_TIMEVAL + sleep_for.tv_nsec;
  }
  
  cout << sleep_for.tv_sec << ' ' << sleep_for.tv_nsec << endl;
  nanosleep(&sleep_for, NULL);
}

int main()
{
  do_loop();
}

// TODO check how nanosleep and software clock are related
// what is that thing about rounding errors???

void do_loop()
{
  timespec period;
  period.tv_sec = 2;
  period.tv_nsec = 0;
  
  while (true)
  {
    // record current time
    timespec started, finished;

    // here it does not matter if the standard input is ready,
    // if not, we will block
    cout << "Please enter what you want to do..." << endl;
    char c;
    cin >> c;

    if (c == 'c')
    {
      clock_gettime(CLOCK_REALTIME, &started);

      // do regular processing (quick) - copy from async branch

      clock_gettime(CLOCK_REALTIME, &finished);

      // wait for some time (if needed), to preserve periodicity

      timespec elapsed;
      elapsed.tv_sec = finished.tv_sec - started.tv_sec;
      elapsed.tv_nsec = finished.tv_nsec - started.tv_nsec;
      cout << "SLEEPING" << endl;
      preserve_period(&period, &elapsed);
      
      // TODO does nanosleep reinvoke itself on
      // being interrupted?
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
