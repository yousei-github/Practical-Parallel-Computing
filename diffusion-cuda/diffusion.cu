#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <cuda.h>
#include <cuda_runtime.h>

#define OS WIN32

#define WIN32 1
#define LINUX 2

#if (OS == WIN32)
#include <time.h>
#elif (OS == LINUX)
#include <sys/time.h>
#endif

#define DISPLAY_STEP (1)
#define SELECT_WAY (1) // 1
#define EN_VERIFY (0)  // 0/1
#define NX 8192
#define NY 8192
#define BS (32) //(32,32,1)/(16,16,1)/(8,8,1)/(4,4,1)

float* data = NULL;
float* gpu_data = NULL;

// For precise time measurement
#if (OS == WIN32)
clock_t st, st2, et2, et;
#elif (OS == LINUX)
struct timeval st, st2, et2, et;
#endif

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

__global__ void calculation_onestep_kernel(int t, int nx, int ny, int block, float* gpu_data)
{
  // An array for “even” steps, An array for “odd” steps. (i.e., “current” array and “next” array)
  int from = t % 2;
  int to = (t + 1) % 2;
  int i, j;

  j = blockIdx.y * blockDim.y + threadIdx.y;
  i = blockIdx.x * blockDim.x + threadIdx.x;

  // Ignore boundary data
  if ((i == 0) || (j == 0) || (i >= nx - 1) || (j >= ny - 1))
  {
    return;
  }

#define x i
#define y j
  gpu_data[to * block + y * nx + x] = 0.2 * (gpu_data[from * block + y * nx + x] + gpu_data[from * block + y * nx + x - 1] \
  + gpu_data[from * block + y * nx + x + 1] + gpu_data[from * block + (y - 1) * nx + x] + gpu_data[from * block + (y + 1) * nx + x]);
#undef x
#undef y
}

/* Calculate for one time step */
/* Input: data[t%2], Output: data[(t+1)%2] */
void calc(int nt, int nx, int ny, float *data)
{
  int t;
  int block = nx * ny;
  cudaError_t rc;
  int data_size = nx * ny * 2;
  int result = nt % 2;

  int temp = 0;
  temp = (int) ceil(sqrt(block / (BS * BS)));
  if (temp > USHRT_MAX)
  {
    printf("Error: block is too big!\n");
    exit(1);
  }
  else if (!temp) // One grid is enough.
    temp = 1;
  printf("temp is[%d].\n", temp);
  dim3 gpu_grid = dim3(temp, temp, 1);
  dim3 gpu_block = dim3(BS, BS, 1);

  /* allocate device memory */
  rc = cudaMalloc((void **)&gpu_data, sizeof(float) * data_size); // Double buffers.
  if (rc != cudaSuccess)
  {
    printf("Error: cudaMalloc failed.\n");
    exit(1);
  }

#if (OS == WIN32)
  st = clock();
#elif (OS == LINUX)
  gettimeofday(&st, NULL);
#endif
  /* copy input data from host to device */
  cudaMemcpy(gpu_data, data, sizeof(float) * data_size, cudaMemcpyHostToDevice);
  cudaDeviceSynchronize(); /* for precise time measurement */

#if (OS == WIN32)
  st2 = clock();
#elif (OS == LINUX)
  gettimeofday(&st2, NULL);
#endif

  /* computation */
  for (t = 0; t < nt; t++)
  {
#if DISPLAY_STEP
    printf("step %d\n", t);
    //fflush(0);
#endif

    calculation_onestep_kernel<<<gpu_grid, gpu_block>>>(t, nx, ny, block, gpu_data);
  }
  cudaDeviceSynchronize(); /* for precise time measurement */

#if (OS == WIN32)
  et2 = clock();
#elif (OS == LINUX)
  gettimeofday(&et2, NULL);
#endif

  /* copy output data (only result data) from device to host */
  cudaMemcpy(&(data[result * block]), &(gpu_data[result * block]), sizeof(float) * data_size / 2, cudaMemcpyDeviceToHost);
  cudaDeviceSynchronize(); /* for precise time measurement */

#if (OS == WIN32)
  et = clock();
#elif (OS == LINUX)
  gettimeofday(&et, NULL);
#endif

  cudaFree(gpu_data);
  return;
}

int main(int argc, char *argv[])
{
  int nt = 20; /* number of time steps */
  int nx = NX, ny = NY;

  printf("Function is %s, BS=[%d].\n", argv[0], BS);
  if (argc >= 2)
  { /* if an argument is specified */
    nt = atoi(argv[1]);
    printf("nt is %d, ", nt);
    if (argc >= 4)
    {
      nx = atoi(argv[2]); // < 65536
      ny = atoi(argv[3]);
      printf("nx is %d, ny is %d. (Only accept equal numbers: nx=ny)\n", nx, ny);
      if ((nx > USHRT_MAX) || (ny > USHRT_MAX))
      {
        printf("Error: nx or ny is too big!\n");
        exit(1);
      }
      if (nx != ny)
      {
        printf("Error: nx is not equal to ny!\n");
        exit(1);
      }
    }
  }

  data = (float *)malloc(sizeof(float) * nx * ny * 2); // Double buffers.

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

    double us2;
    us = get_elapsed_time(&st, &et);
    us2 = get_elapsed_time(&st2, &et2);
    gflops = ((double)nx * ny * nt * op_per_point) / us / 1000.0;
    printf("Calculation took %.3lf sec --> %.3lf GFlops  (with data transfer)\n",
           us / 1000000.0, gflops);
    gflops = ((double)nx * ny * nt * op_per_point) / us2 / 1000.0;
    printf("                 %.3lf sec --> %.3lf GFlops  (without data transfer)\n",
           us2 / 1000000.0, gflops);
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

    double us2;
    us = get_elapsed_time(&st, &et);
    us2 = get_elapsed_time(&st2, &et2);
    gflops = ((double)nx * ny * nt * op_per_point) / us / 1000.0;
    printf("Calculation took %.3lf sec --> %.3lf GFlops  (with data transfer)\n",
           us / 1000000.0, gflops);
    gflops = ((double)nx * ny * nt * op_per_point) / us2 / 1000.0;
    printf("                 %.3lf sec --> %.3lf GFlops  (without data transfer)\n",
           us2 / 1000000.0, gflops);
  }
#endif

#if (EN_VERIFY == 1)
  int result = nt % 2;
  int block = nx * ny;

  /* Store the result generated by parallelism  . */
#if (SELECT_WAY == 1)
  FILE *fd1 = fopen("Data_w1", "wb");
  size_t num1 = fwrite(data + result * block, sizeof(float), block, fd1);
  printf("VERIFY: fd1 writes [%ld] elements.\n", (long)num1);
  fclose(fd1);

#endif

  /* Read the result that is created by “diffusion” sample program */
  FILE *fd2 = fopen("Data", "rb");
  //FILE *fd2 = fopen("Data_w1", "rb");
  if (fd2 == NULL)
  {
    printf("VERIFY Error: Open file Data.txt failed.\n");
    exit(1);
  }
  int test = (nt + 1) % 2;
  size_t num2 = fread(data + test * block, sizeof(float), block, fd2);
  printf("VERIFY: fd2 reads [%ld] elements.\n", (long)num2);

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

  free(data);
  return 0;
}
