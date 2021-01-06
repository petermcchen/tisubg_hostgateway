#include <time.h>
#include <stdio.h>

int main()
{
  struct timespec tt;
  clock_getres(CLOCK_REALTIME, &tt);
  printf("resolution: %ld\n", tt.tv_nsec);

  struct timespec tv;
  unsigned long long r;

  clock_gettime(CLOCK_REALTIME, &tv);

  /* convert nano seconds to milliseconds */
  r = tv.tv_nsec / (1000 * 1000);
  /* convert secs to mSecs and add */
  r = r + (tv.tv_sec * 1000);
  printf("abs time: %llu\n", r);

  time_t timep;
  time (&timep);
  printf("time: %ld\n", timep);

  return 0;
}
