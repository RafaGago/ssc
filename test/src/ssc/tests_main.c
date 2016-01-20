#include <stdio.h>
#include <ssc/basic_test.h>
#include <ssc/two_fiber_test.h>
#include <ssc/ahead_of_time_test.h>

int main (void)
{
  int failed = 0;
  if (basic_tests() != 0)     { ++failed; }
  if (two_fiber_tests() != 0) { ++failed; }
  if (ahead_of_time_tests() != 0) { ++failed; }
  printf ("\n[SUITE ERR ] %d suite(s)\n", failed);
  return failed;
}
