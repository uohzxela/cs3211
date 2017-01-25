#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define LOOPS 1
#define SIZE 8192

/* Return 1 if the difference is negative, otherwise 0.  */
int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1)
{
  long int diff = (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec + 1000000 * t1->tv_sec);
  result->tv_sec = diff / 1000000;
  result->tv_usec = diff % 1000000;

  return (diff<0);
}

void timeval_print(struct timeval *tv)
{
  char buffer[30];
  time_t curtime;

  printf("%ld.%06ld", tv->tv_sec, (long int)tv->tv_usec);
  curtime = tv->tv_sec;
  strftime(buffer, 30, "%m-%d-%Y  %T", localtime(&curtime));
  printf(" = %s.%06ld\n", buffer, (long int) tv->tv_usec);
}


int arr[64 * 1024 * 1024];

int main() {
   struct timeval tvBegin, tvEnd, tvDiff;
   int steps = 64 * 1024 * 1024; // Arbitrary number of steps
   int i,j,len = 1024;
   for (j=1; j<17;j++)
     {
        int lengthMod = len-1;
        gettimeofday(&tvBegin, NULL);
        for (i = 0; i < steps; i++)
          {
             arr[(i * 16) & lengthMod]++; // (x & lengthMod) is equal to (x % arr.Length)
          }
        gettimeofday(&tvEnd, NULL);
        timeval_subtract(&tvDiff, &tvEnd, &tvBegin);
        printf( "Array size: %d kB ",4*len/1024 );
        printf("and time taken is %ld.%06ld secs\n", tvDiff.tv_sec, (long int)tvDiff.tv_usec);
        len = len+len;
     }
}
