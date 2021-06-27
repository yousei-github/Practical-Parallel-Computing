#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <limits.h>

#define OS WIN32

#define WIN32 1
#define LINUX 2

#if (OS == WIN32)
#include <time.h>
#elif (OS == LINUX)
#include <sys/time.h>
#endif

#define DISPLAY_STEP (1)
#define SELECT_WAY (3) // 1/2/3
#define EN_VERIFY (0)  // 0/1
#define NX 8192
#define NY 8192

float *data = NULL;

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

void init(int nx, int ny, float *data)
{
  int x, y;
  int cx = nx / 2, cy = 0; /* center of ink */
  int rad = (nx + ny) / 8; /* radius of ink */
  int block = nx * ny;

  for (y = 0; y < ny; y++)
  {
    for (x = 0; x < nx; x++)
    {
      float v = 0.0;
      if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < rad * rad)
      {
        v = 1.0; // They represent the ink's region.
      }
      data[0 + y * nx + x] = v;
      data[1 * block + y * nx + x] = v;
    }
  }
  return;
}

/* Calculate for one time step */
/* Input: data[t%2], Output: data[(t+1)%2] */
void calc(int nt, int nx, int ny, float *data)
{
  int t;
  int block = nx * ny;

  for (t = 0; t < nt; t++)
  {
    // An array for “even” steps, An array for “odd” steps. (i.e., “current” array and “next” array)
    int from = t % 2;
    int to = (t + 1) % 2;

#if DISPLAY_STEP
    printf("step %d\n", t);
    fflush(0);
#endif

// Note the boundaries are skipped.
#if (SELECT_WAY == 1)
    int x, y;
#pragma omp parallel for private(x)
    for (y = 1; y < ny - 1; y++)
    {
      for (x = 1; x < nx - 1; x++)
      {
        data[to * block + y * nx + x] = 0.2 * (data[from * block + y * nx + x] + data[from * block + y * nx + x - 1] \
        + data[from * block + y * nx + x + 1] + data[from * block + (y - 1) * nx + x] + data[from * block + (y + 1) * nx + x]);
      }
    }
#elif (SELECT_WAY == 2)
    int y;
#pragma omp parallel
    {
#pragma omp for
      for (y = 1; y < ny - 1; y++)
      {
        int x;
        for (x = 1; x < nx - 1; x++)
        {
          data[to * block + y * nx + x] = 0.2 * (data[from * block + y * nx + x] + data[from * block + y * nx + x - 1] \
          + data[from * block + y * nx + x + 1] + data[from * block + (y - 1) * nx + x] + data[from * block + (y + 1) * nx + x]);
        }
      }
    }
#else

#pragma omp parallel
    {
      int num_thread = omp_get_num_threads();
      int span = (ny - 2) / num_thread;
      if (span == 0)
        span = 1;
      int thread_num = omp_get_thread_num();  // Get its own thread number.
      int end = (thread_num + 1) * span;  // Find its end in y-axis.

      if (thread_num == num_thread - 1) // The last thread needs to finish the rest of work (Note the -2 is need for the loop below).
        end = ny - 2;

      int y;
      for (y = 1 + thread_num * span; y <= end; y++)  // Note here we use y <= end.
      {
        int x;
        for (x = 1; x < nx - 1; x++)
        {
          data[to * block + y * nx + x] = 0.2 * (data[from * block + y * nx + x] + data[from * block + y * nx + x - 1] \
          + data[from * block + y * nx + x + 1] + data[from * block + (y - 1) * nx + x] + data[from * block + (y + 1) * nx + x]);
        }
      }
    }
#endif

  }

  return;
}

int main(int argc, char *argv[])
{
  int nt = 20; /* number of time steps */
  int nx = NX, ny = NY;

  printf("Function is %s.\n", argv[0]);
  if (argc >= 2)
  { /* if an argument is specified */
    nt = atoi(argv[1]);
    printf("nt is %d, ", nt);
    if (argc >= 4)
    {
      nx = atoi(argv[2]); // < 65536
      ny = atoi(argv[3]);
      printf("nx is %d, ny is %d\n", nx, ny);
      if ((nx > USHRT_MAX) || (ny > USHRT_MAX))
      {
        printf("Error: nx or ny is too big!\n");
        exit(1);
      }
    }
  }

  data = (float *)malloc(sizeof(float) * nx * ny * 2);  // Double buffers.

  if (data == NULL)
  {
    printf("Error: Memory allocation failed.\n");
    exit(1);
  }

  init(nx, ny, data);

#if (OS == WIN32)
  clock_t start, stop;
  start = clock();

  calc(nt, nx, ny, data);

  stop = clock();

  {
    double us;
    double gflops;
    int op_per_point = 5; // 4 add & 1 multiply per point and some integer operation

    us = get_elapsed_time(&start, &stop);
    printf("Elapsed time: %.3lf sec\n", us / 1000000.0);
    gflops = ((double)nx * ny * nt * op_per_point) / us / 1000.0;
    printf("Speed: %.3lf GFlops\n", gflops);
  }

#elif (OS == LINUX)
  struct timeval t1, t2;
  gettimeofday(&t1, NULL);

  calc(nt, nx, ny, data);

  gettimeofday(&t2, NULL);

  {
    double us;
    double gflops;
    int op_per_point = 5; // 4 add & 1 multiply per point

    us = get_elapsed_time(&t1, &t2);
    printf("Elapsed time: %.3lf sec\n", us / 1000000.0);
    gflops = ((double)nx * ny * nt * op_per_point) / us / 1000.0;
    printf("Speed: %.3lf GFlops\n", gflops);
  }
#endif

#if (EN_VERIFY == 1)
  int result = nt % 2;
  int block = nx * ny;

  /* Store the result generated by parallelism  . */
#if (SELECT_WAY == 1)
  FILE *fd1 = fopen("Data_w1", "wb");
  size_t num1 = fwrite(data + result * block, sizeof(float), block, fd1);
  printf("VERIFY: fd1 writes [%ld] elements.\n", num1);
  fclose(fd1);
#elif (SELECT_WAY == 2)
  FILE *fd1 = fopen("Data_w2", "wb");
  size_t num1 = fwrite(data + result * block, sizeof(float), block, fd1);
  printf("VERIFY: fd1 writes [%ld] elements.\n", num1);
  fclose(fd1);
#else
  FILE *fd1 = fopen("Data_w3", "wb");
  size_t num1 = fwrite(data + result * block, sizeof(float), block, fd1);
  printf("VERIFY: fd1 writes [%ld] elements.\n", num1);
  fclose(fd1);
#endif

  /* Read the result that is created by “diffusion” sample program */
  FILE *fd2 = fopen("Data", "rb");
  //FILE *fd2 = fopen("Data_w1", "rb");
  if(fd2 == NULL)
  {
    printf("VERIFY Error: Open file Data.txt failed.\n");
    exit(1);
  }
  int test = (nt + 1) % 2;
  size_t num2 = fread(data + test * block, sizeof(float), block, fd2);
  printf("VERIFY: fd2 reads [%ld] elements.\n", num2);

  /* Below we compare the results. */
  unsigned char error_flag = 0;
  int y;
  for (y = 1; y < ny - 1; y++)
  {
    int x;
    for (x = 1; x < nx - 1; x++)
    {
      if (data[test * block + y * nx + x] != data[result * block + y * nx + x])
      {
        error_flag = 1;
        printf("VERIFY: We got error flag at y = [%d] and x = [%d]!\n", x, y);
        break;
      }
    }
  }

  if (!error_flag)
      printf("VERIFY: Succeed! We have no error flag!\n");
#endif

  return 0;
}
