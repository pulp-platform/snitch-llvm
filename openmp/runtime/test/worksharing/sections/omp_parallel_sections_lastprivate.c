// RUN: %libomp-compile-and-run
#include <stdio.h>
#include "omp_testsuite.h"

int test_omp_parallel_sections_lastprivate()
{
  int sum;
  int sum0;
  int i;
  int i0;
  int known_sum;
  sum =0;
  sum0 = 0;
  i0 = -1;

  #pragma omp parallel sections private(i,sum0) lastprivate(i0)
  {
    #pragma omp section
    {
      sum0=0;
      for (i=1;i<400;i++) {
        sum0=sum0+i;
        i0=i;
      }
      #pragma omp critical
      {
        sum= sum+sum0;
      }
    }
    #pragma omp section
    {
      sum0=0;
      for(i=400;i<700;i++) {
        sum0=sum0+i;
        i0=i;
      }
      #pragma omp critical
      {
        sum= sum+sum0;
      }
    }
    #pragma omp section
    {
      sum0=0;
      for(i=700;i<1000;i++) {
        sum0=sum0+i;
        i0=i;
      }
      #pragma omp critical
      {
        sum= sum+sum0;
      }
    }
  }

  known_sum=(999*1000)/2;
  return ((known_sum==sum) && (i0==999) );
}

#ifndef NO_MAIN
int main()
{
  int i;
  int num_failed=0;

  for(i = 0; i < REPETITIONS; i++) {
    if(!test_omp_parallel_sections_lastprivate()) {
      num_failed++;
    }
  }
  return num_failed;
}
#endif
