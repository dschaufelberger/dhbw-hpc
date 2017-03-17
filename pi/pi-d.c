#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <omp.h>

#define TRYS 5000000

static int throw() {
  double x, y;
  x = (double)rand() / (double)RAND_MAX;
  y = (double)rand() / (double)RAND_MAX;
  if ((x*x + y*y) <= 1.0) return 1;
    
  return 0;
}

/*
* Aufgabe 3 e)
* Die Anzahl der Threads kann durch den Benutzer beim Starten des Programmes in der Kommandozeile
* mittels "OMP_NUM_THREADS=<number> ./pi-d" gesteuert werden. Für <number> kann die zu nutzende
* Anzahl Threads angegeben werden.
* Unterbinden lässt es sich durch die Angabe von "num_threads(<number>)" in der äußeren "omp parallel"-Direktive.
*/
int main(int argc, char **argv) {
  int globalCount = 0, globalSamples=TRYS;

  #pragma omp parallel reduction( +: globalCount)
  {

  #pragma omp for
  for(int i = 0; i < globalSamples; ++i) {
    globalCount += throw();
  }

  printf("Thread %d: treffer %d\n", omp_get_thread_num(), globalCount);

  }

  double pi = 4.0 * (double)globalCount / (double)(globalSamples);
 
  printf("pi is %.9lf\n", pi);
  
  return 0;
}
