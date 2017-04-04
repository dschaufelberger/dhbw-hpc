#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/time.h>

#include "mpi.h"

#define calcIndex(width, x,y)  ((y)*(width) + (x))

long TimeSteps = 100;

void writeVTK2Piece(long timestep, double *data, char prefix[1024], int w, int h, int overallWidth, int processRank) {
  char filename[2048];  
  int x,y; 
  
  long offsetX = w * processRank;
  long  nxy = w * h * sizeof(float);  

  snprintf(filename, sizeof(filename), "%s-%05ld-%02d%s", prefix, timestep, processRank, ".vti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d 0 0\" Origin=\"0 0 0\" Spacing=\"1 1 0\">\n", 0, overallWidth, 0, h);
  fprintf(fp, "<Piece Extent=\"%d %d %d %d 0 0\">\n", offsetX, offsetX + w, 0, h);
  fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
  fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp, "</CellData>\n");
  fprintf(fp, "</Piece>\n");
  fprintf(fp, "</ImageData>\n");
  fprintf(fp, "<AppendedData encoding=\"raw\">\n");
  fprintf(fp, "_");
  fwrite((unsigned char*)&nxy, sizeof(long), 1, fp);

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      float value = data[calcIndex(w,x,y)];
      fwrite((unsigned char*)&value, sizeof(float), 1, fp);
    }
  }
  
  fprintf(fp, "\n</AppendedData>\n");
  fprintf(fp, "</VTKFile>\n");
  fclose(fp);
}

void writeVTK2Container(long timestep, char prefix[1024], long w, long h, int partialWidth, int num_processes) {
  char filename[2048];

  snprintf(filename, sizeof(filename), "%s-%05ld%s", prefix, timestep, ".pvti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp,"<?xml version=\"1.0\"?>\n");
  fprintf(fp,"<VTKFile type=\"PImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp,"<PImageData WholeExtent=\"%d %d %d %d 0 0\" Origin=\"0 0 0\" Spacing =\"1 1 0\" GhostLevel=\"1\">\n", 0, w, 0, h);
  fprintf(fp,"<PCellData Scalars=\"%s\">\n", prefix);
  fprintf(fp,"<PDataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp,"</PCellData>\n");

  for(int i = 0; i < num_processes; i++) {
    fprintf(fp, "<Piece Extent=\"%d %d %d %d 0 0\" Source=\"%s-%05ld-%02d%s\"/>\n", i * partialWidth, (i + 1) * partialWidth, 0, h, prefix, timestep, i, ".vti");
  }

  fprintf(fp,"</PImageData>\n");
  fprintf(fp, "</VTKFile>\n");
  fclose(fp);
}

void printToFile(double* field,char prefix[1024], int w, int h, int rank) {
  char filename[2048]; 
  snprintf(filename, sizeof(filename), "%s-%d%s", prefix, rank, ".txt");
  FILE* fp = fopen(filename, "w");

  int x,y;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) fprintf(fp, field[calcIndex(w, x,y)] ? "X" : "_");
    fprintf(fp, "\n");
  }

  fclose(fp);
}

void show(double* currentfield, int w, int h) {
  // printf("\033[H");
  int x,y;
  for (y = 0; y < h; y++) {
    // for (x = 0; x < w; x++) printf(currentfield[calcIndex(w, x,y)] ? "\033[07m  \033[m" : "  ");
    for (x = 0; x < w; x++) printf(currentfield[calcIndex(w, x,y)] ? "X" : "_");
    printf("\n");
  }
  fflush(stdout);
}


int evolve(double* currentfield, double* newfield, double* ghostLeft, double* ghostRight, int w, int h) {
  int x,y;
  int itLives = 0;

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      
      int n = countNeighbours(currentfield, ghostLeft, ghostRight, x, y, w, h);
      int index = calcIndex(w, x, y);

      // dead or alive and 3 neighbours => come alive or stay alive
      if (n == 3) {
        newfield[index] = 1;
      }
      // dead or alive and 2 neighbours => stay dead or alive
      else if (n == 2) {
        newfield[index] = currentfield[index];
      }
      // less than 2 or more than 3 neighbours => DIE (even if you're already dead)!
      else {
        newfield[index] = 0;
      }
      
      itLives += (currentfield[index] != newfield[index]);
    }
  }

  return itLives;
}

int countNeighbours(double* currentfield, double* ghostLeft, double* ghostRight, int x, int y, int w, int h) {
  int n = 0;

  for (int stencilX = (x-1); stencilX <= (x+1); stencilX++) {
    for (int stencilY = (y-1); stencilY <= (y+1); stencilY++) {
      if (stencilX == -1) {
          n += ghostLeft[calcIndex(1, 0, stencilY % h)];
      }
      else if (stencilX == w) {
          n += ghostRight[calcIndex(1, 0, stencilY % h)];
      }
      else {
        // the modulo operations makes the field be tested in a periodic way
        n += currentfield[calcIndex(w, (stencilX + w) % w, (stencilY + h) % h)];
      }
    }
  }

  // the center of the stencil should not be taken into account
  n -= currentfield[calcIndex(w, x, y)];

  return n;
}

double* readFromASCIIFile(char filename[256], int* w, int* h) {
    FILE* file = fopen(filename, "r"); /* should check the result */

    int size = 22*13;
    int character;
    size_t len = 0;
    size_t width = 0;
    size_t height = 0;
    double* field = calloc(size, sizeof(double));

    while ((character = fgetc(file)) != EOF){
      if (character == '\n') {
        if (!width) width = len;
        height++;
        continue;
      }
      if (character == 'o') field[len++] = 1;
      if (character == '_') field[len++] = 0;
      // resize 
      if(len==size){
          field = realloc(field, sizeof(double) * (size += 10));
      }
    }
    height++;

    field = realloc(field, sizeof(*field) * len);
    *w = width;
    *h = height;

    fclose(file);
    return field;
}
/*
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~
* ~ Buffer vs. MPI_Datatype ~
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Buffer:
*   + kein Overhead zum erstellen der Datentypen
*   + Auf gleichen Systemen schneller durch nicht benötigte Konvertierungen
*   - Nicht portierbar, interoperabel
*
* MPI_Datatype:
*   + Interoperabel, unabhängig vom System
*   - mehr Aufwand*
*
*/
void game(int overallWidth, int overallHeight, double initialfield[], MPI_Comm communicator, int rank, int num_processes) {
  int w = (overallWidth / num_processes);
  int h = overallHeight;
  double* currentfield = (double*)calloc(w * h, sizeof(double));
  memcpy(currentfield, initialfield, w * h * sizeof(double));
  double* newfield = (double*)calloc(w * h, sizeof(double));
  double* ghostLeft = (double*)calloc(h, sizeof(double));
  double* ghostRight = (double*)calloc(h, sizeof(double));
  int *aliveBuffer = (int*)calloc(1, sizeof(int));;
  int itLives = 1;

  // printf("Rank = %d, width x height = %d x %d\n", rank, w, h);
  // printToFile(currentfield, "field", w, h, rank);

  for (int t = 0; t < TimeSteps && itLives; t++) {
    if (rank == 0) printf("Timestep %d\n", t);
    for (int i = 0; i < h; i++) {
      int index = calcIndex(w, 0, i);
      ghostLeft[i] = currentfield[index];
      index = calcIndex(w, (w - 1), i);
      ghostRight[i] = currentfield[index];
    }
    // printToFile(ghostLeft, "ghostLeft", 1,h, rank);
    // printToFile(ghostRight, "ghostRight", 1,h,rank);

    shareGhostlayers(ghostLeft, ghostRight, h, rank, num_processes, communicator);
    itLives = evolve(currentfield, newfield, ghostLeft, ghostRight, w, h);
    writeVTK2Piece(t, currentfield, "gol", w, h, overallWidth, rank);

    if (rank == 0) {
      writeVTK2Container(t, "gol", overallWidth, overallHeight, w, num_processes);
    }

    // printToFile(ghostLeft, "sharedLeft", 1,h, rank);
    // printToFile(ghostRight, "sharedRight", 1,h,rank);
    
    //SWAP
    double *temp = currentfield;
    currentfield = newfield;
    newfield = temp;

    *aliveBuffer = itLives;
    MPI_Allreduce(MPI_IN_PLACE, aliveBuffer, 1, MPI_INT, MPI_SUM, communicator);
    itLives = *aliveBuffer;
  }

  free(currentfield);
  free(newfield);
  free(ghostLeft);
  free(ghostRight);
  free(aliveBuffer);
}

void shareGhostlayers(double* ghostLeft, double* ghostRight, int sendCount, int rank, int num_processes, MPI_Comm communicator) {
  double* sendbuffer = (double*)calloc(sendCount, sizeof(double));
  double* receivebuffer = (double*)calloc(sendCount, sizeof(double));;
  if (rank % 2 == 0) {
    if (rank < num_processes-1) {
      fillGhostIntoBuffer(ghostRight, sendbuffer, sendCount);

      MPI_Send(sendbuffer, sendCount, MPI_DOUBLE, (rank+1), 1, communicator);
      MPI_Recv(receivebuffer, sendCount, MPI_DOUBLE, (rank+1), 2, communicator, MPI_STATUS_IGNORE);

      fillBufferIntoGhost(receivebuffer, ghostRight, sendCount);
    }
    if (rank > 0) {
      fillGhostIntoBuffer(ghostLeft, sendbuffer, sendCount);

      MPI_Send(sendbuffer, sendCount, MPI_DOUBLE, (rank-1), 3, communicator);      
      MPI_Recv(receivebuffer, sendCount, MPI_DOUBLE, (rank-1), 4, communicator, MPI_STATUS_IGNORE);

      fillBufferIntoGhost(receivebuffer, ghostLeft, sendCount);
    }
  } else {
    if (rank > 0) {
      fillGhostIntoBuffer(ghostLeft, sendbuffer, sendCount);

      MPI_Recv(receivebuffer, sendCount, MPI_DOUBLE, (rank-1), 1, communicator, MPI_STATUS_IGNORE);

      fillBufferIntoGhost(receivebuffer, ghostLeft, sendCount);

      MPI_Send(sendbuffer, sendCount, MPI_DOUBLE, (rank-1), 2, communicator);
    }
    if (rank < num_processes-1) {
      fillGhostIntoBuffer(ghostRight, sendbuffer, sendCount);
      
      MPI_Recv(receivebuffer, sendCount, MPI_DOUBLE, (rank+1), 3, communicator, MPI_STATUS_IGNORE);

      fillBufferIntoGhost(receivebuffer, ghostRight, sendCount);

      MPI_Send(sendbuffer, sendCount, MPI_DOUBLE, (rank+1), 4, communicator);
    }
  }

  if (rank == 0) {
    int periodic_neighbour_left, neighbour_right;
    MPI_Cart_shift(communicator, 0, 1, &periodic_neighbour_left, &neighbour_right);

    fillGhostIntoBuffer(ghostLeft, sendbuffer, sendCount);

    MPI_Send(sendbuffer, sendCount, MPI_DOUBLE, periodic_neighbour_left, 1, communicator);      
    MPI_Recv(receivebuffer, sendCount, MPI_DOUBLE, periodic_neighbour_left, 2, communicator, MPI_STATUS_IGNORE);

    fillBufferIntoGhost(receivebuffer, ghostLeft, sendCount);
  }

  if (rank == num_processes-1) {
    int neighbour_left, periodic_neighbour_right;
    MPI_Cart_shift(communicator, 0, 1, &neighbour_left, &periodic_neighbour_right);

    fillGhostIntoBuffer(ghostRight, sendbuffer, sendCount);

    MPI_Recv(receivebuffer, sendCount, MPI_DOUBLE, periodic_neighbour_right, 1, communicator, MPI_STATUS_IGNORE);

    fillBufferIntoGhost(receivebuffer, ghostRight, sendCount);
    
    MPI_Send(sendbuffer, sendCount, MPI_DOUBLE, periodic_neighbour_right, 2, communicator);
  }

  free(sendbuffer);
  free(receivebuffer);
}

void fillGhostIntoBuffer(double* ghost, double* buffer, int amount) {
  memcpy(buffer, ghost, amount * sizeof(double));
}

void fillBufferIntoGhost(double* buffer, double* ghost, int amount) {
  memcpy(ghost, buffer, amount * sizeof(double));
}


double* initializeField(int w, int h) {
  double* field = calloc(w * h, sizeof(double));

  int i;
  for (i = 0; i < h*w; i++) {
    field[i] = (rand() < RAND_MAX / 10) ? 1 : 0; ///< init domain randomly
  }

  return field;
}

void computeSendBuffer(double* field, double* buffer, int w, int h, int num_processes, MPI_Comm communicator) {
  int partialWidth = w / num_processes;
  int sendCount = partialWidth * h;
  int index;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      // Maps the <w * h> domain to a <(w / #processes) * #processes> domain.
      // The <(w / #processes) * h> sub domain for each process is literraly flattened into a connected block in the array
      //
      //  |   P0    |   P1    |   P2    |
      //  |_________|_________|_________|
      //  |  1 |  2 |  3 |  4 |  5 |  6 |       | 1 | 2 |  7 |  8 | 13 | 14 | 19 | 20 |   <- P0
      //  |  7 |  8 |  9 | 10 | 11 | 12 |  =>   | 3 | 4 |  9 | 10 | 15 | 16 | 21 | 22 |   <- P1
      //  | 13 | 14 | 15 | 16 | 17 | 18 |       | 5 | 6 | 11 | 12 | 17 | 18 | 23 | 24 |   <- P2
      //  | 19 | 20 | 21 | 22 | 23 | 24 |
      index = calcIndex(sendCount, (partialWidth * y) + (x % partialWidth), x / partialWidth);
      buffer[index] = field[calcIndex(w, x, y)];
      // printf("(%2d, %d) -> (%d, %d)\n", x, y, (partialWidth * y) + (x % partialWidth), x / partialWidth);
    }
  }
}
 
int main(int argc, char *argv[]) {
  srand(time(NULL));

  int w = 0, h = 0;
  if (argc > 1) w = atoi(argv[1]); ///< read width
  if (argc > 2) h = atoi(argv[2]); ///< read height
  if (w <= 0) w = 21; ///< default width
  if (h <= 0) h = 13; ///< default height

  int rank, num_processes;
  MPI_Comm world = MPI_COMM_WORLD;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(world, &num_processes);

  int processes_per_dimension[1] = { num_processes };
  int is_periodic_per_dimension[1] = { 1 }; // the 1D topology is periodic at it's left and right border
  MPI_Comm topology_comm;
  MPI_Cart_create(world, 1, processes_per_dimension, is_periodic_per_dimension, 1, &topology_comm);
  MPI_Comm_rank(topology_comm, &rank);
  MPI_Comm_size(topology_comm, &num_processes);

  int sendCount = (w / num_processes) * h;
  double* sendBuffer = (double *)calloc(w * h,sizeof(double));
  double* receiveBuffer = (double *)malloc(sendCount * sizeof(double));
  if (rank == 0) {  
    printf("Number of processes: %d\n", num_processes);
    printf("Sendcount = %d\n", sendCount);
    double* field = initializeField(w, h);
    printf("Field:\n");
    show(field, w, h);

    computeSendBuffer(field, sendBuffer, w, h, num_processes, topology_comm);
    printf("\nSendBuffer:\n");
    show(sendBuffer, sendCount, num_processes);
  }
  // MPI_Scatter to distribute to all other processes
  MPI_Scatter(sendBuffer, sendCount, MPI_DOUBLE, receiveBuffer, sendCount, MPI_DOUBLE, 0, topology_comm);

  // printf("\nReceiveBuffer (rank %d):\n", rank);
  // show(receiveBuffer, w / num_processes, h);
  game(w, h, receiveBuffer, topology_comm, rank, num_processes);

  free(sendBuffer);
  free(receiveBuffer);
  MPI_Finalize();
  return 0;
}
