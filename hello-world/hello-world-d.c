#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <omp.h>

int main(int argc, char **argv) {
  #pragma omp parallel sections num_threads(4)
  {
    {
    sleep(1);
    printf("Hallo Welt from thread %d of %d\n", omp_get_thread_num(), omp_get_num_threads());
    }
    #pragma omp section
    {
    sleep(1);
    printf("Hola mundo from thread %d of %d\n", omp_get_thread_num(), omp_get_num_threads());
    }
    #pragma omp section
    {
    sleep(1);
    printf("Hej varlden from thread %d of %d\n", omp_get_thread_num(), omp_get_num_threads());
    }
    #pragma omp section
    {
    sleep(1);
    printf("Bonjour tout le monde from thread %d of %d\n", omp_get_thread_num(), omp_get_num_threads());
    }
    #pragma omp section
    {
    sleep(1);
    printf("Hello World from thread %d of %d\n", omp_get_thread_num(), omp_get_num_threads());
    }
  }

  return 0;
}
