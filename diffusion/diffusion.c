#include <stdio.h>
#include <stdlib.h>

#define OS WIN32

#define WIN32 1
#define LINUX 2

#if (OS == WIN32)
#include <time.h>
#elif (OS == LINUX)
#include <sys/time.h>
#endif

#define DISPLAY_STEP (1)
#define NX 8192
#define NY 8192

float data[2][NY][NX];

#if (OS == WIN32)
/* in microseconds (us) */
double get_elapsed_time(clock_t *begin, clock_t *end)
{
  return ((double)(*end - *begin)) / CLOCKS_PER_SEC * 1000000;
}
#elif (OS == LINUX)
/* in microseconds (us) */
double get_elapsed_time(struct timeval *begin, struct timeval *end)
{
  return (end->tv_sec - begin->tv_sec) * 1000000 + (end->tv_usec - begin->tv_usec);
}
#endif

void init()
{
  int x, y;
  int cx = NX / 2, cy = 0; /* center of ink */
  int rad = (NX + NY) / 8; /* radius of ink */

  for (y = 0; y < NY; y++)
  {
    for (x = 0; x < NX; x++)
    {
      float v = 0.0;
      if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < rad * rad)
      {
        v = 1.0;  // they represent the ink's region
      }
      data[0][y][x] = v;
      data[1][y][x] = v;
    }
  }
  return;
}

/* Calculate for one time step */
/* Input: data[t%2], Output: data[(t+1)%2] */
void calc(int nt)
{
  int t, x, y;

  for (t = 0; t < nt; t++)
  {
    // An array for “even” steps, An array for “odd” steps. (i.e., “current” array and “next” array)
    int from = t % 2;
    int to = (t + 1) % 2;

#if DISPLAY_STEP
    printf("step %d\n", t);
    fflush(0);
#endif

    // boundaries are skipped.
    for (y = 1; y < NY - 1; y++)
    {
      for (x = 1; x < NX - 1; x++)
      {
        data[to][y][x] = 0.2 * (data[from][y][x] + data[from][y][x - 1] + data[from][y][x + 1] + data[from][y - 1][x] + data[from][y + 1][x]);
      }
    }
  }

  return;
}

int main(int argc, char *argv[])
{
  int nt = 20; /* number of time steps */

  if (argc >= 2)
  { /* if an argument is specified */
    nt = atoi(argv[1]);
  }

  init();

#if (OS == WIN32)
  clock_t start, stop;
  start = clock();

  calc(nt);

  stop = clock();

  {
    double us;
    double gflops;
    int op_per_point = 5; // 4 add & 1 multiply per point

    us = get_elapsed_time(&start, &stop);
    printf("Elapsed time: %.3lf sec\n", us / 1000000.0);
    gflops = ((double)NX * NY * nt * op_per_point) / us / 1000.0;
    printf("Speed: %.3lf GFlops\n", gflops);
  }

#elif (OS == LINUX)
  struct timeval t1, t2;
  gettimeofday(&t1, NULL);

  calc(nt);

  gettimeofday(&t2, NULL);

  {
    double us;
    double gflops;
    int op_per_point = 5; // 4 add & 1 multiply per point

    us = get_elapsed_time(&t1, &t2);
    printf("Elapsed time: %.3lf sec\n", us / 1000000.0);
    gflops = ((double)NX * NY * nt * op_per_point) / us / 1000.0;
    printf("Speed: %.3lf GFlops\n", gflops);
  }
#endif
  
  int result = nt % 2;
  int block = NX * NY;
  FILE *fd1 = fopen("Data", "wb");
  fwrite(data + result * block, sizeof(float), block, fd1);
  fclose(fd1);

  return 0;
}
