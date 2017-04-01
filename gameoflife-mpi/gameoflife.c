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

long TimeSteps = 40;

void writeVTK2Piece(long timestep, double *data, char prefix[1024],int xStart, int xEnd, int yStart, int yEnd, long w, long h, int process_rank) {
  char filename[2048];  
  int x,y; 
  
  long offsetX=0;
  long offsetY=0;
  float deltax=1.0;
  float deltay=1.0;
  long  nxy = w * h * sizeof(float);  

  snprintf(filename, sizeof(filename), "%s-%05ld-%02d%s", prefix, timestep, process_rank, ".vti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d 0 0\" Origin=\"0 0 0\" Spacing=\"1 1 0\">\n",
    offsetX, offsetX + w, offsetY, offsetY + h);
  fprintf(fp, "<Piece Extent=\"%d %d %d %d 0 0\">\n", xStart, xEnd + 1, yStart, yEnd + 1);
  fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
  fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp, "</CellData>\n");
  fprintf(fp, "</Piece>\n");
  fprintf(fp, "</ImageData>\n");
  fprintf(fp, "<AppendedData encoding=\"raw\">\n");
  fprintf(fp, "_");
  fwrite((unsigned char*)&nxy, sizeof(long), 1, fp);

  for (y = yStart; y <= yEnd; y++) {
    for (x = xStart; x <= xEnd; x++) {
      float value = data[calcIndex(w,x,y)];
      fwrite((unsigned char*)&value, sizeof(float), 1, fp);
    }
  }
  
  fprintf(fp, "\n</AppendedData>\n");
  fprintf(fp, "</VTKFile>\n");
  fclose(fp);
}

void writeVTK2Container(long timestep, double *data, char prefix[1024], long w, long h, int *area_bounds, int num_processes) {
  char filename[2048];  
  int x,y; 
  
  long offsetX=0;
  long offsetY=0;
  float deltax=1.0;
  float deltay=1.0;
  long  nxy = w * h * sizeof(float);  

  snprintf(filename, sizeof(filename), "%s-%05ld%s", prefix, timestep, ".pvti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp,"<?xml version=\"1.0\"?>\n");
  fprintf(fp,"<VTKFile type=\"PImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp,"<PImageData WholeExtent=\"%d %d %d %d 0 0\" Origin=\"0 0 0\" Spacing =\"1 1 0\" GhostLevel=\"1\">\n", offsetX, offsetX + w, offsetY, offsetY + h);
  fprintf(fp,"<PCellData Scalars=\"%s\">\n", prefix);
  fprintf(fp,"<PDataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp,"</PCellData>\n");

  for(int i = 0; i < num_processes; i++) {
    fprintf(fp, "<Piece Extent=\"%d %d %d %d 0 0\" Source=\"%s-%05ld-%02d%s\"/>\n",
      area_bounds[i * 4], area_bounds[i * 4 + 1] + 1, area_bounds[i * 4 + 2], area_bounds[i * 4 + 3] + 1, prefix, timestep, i, ".vti");
  }

  fprintf(fp,"</PImageData>\n");
  fprintf(fp, "</VTKFile>\n");
  fclose(fp);
}


void show(double* currentfield, int w, int h) {
  printf("\033[H");
  int x,y;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) printf(currentfield[calcIndex(w, x,y)] ? "\033[07m  \033[m" : "  ");
    //printf("\033[E");
    printf("\n");
  }
  fflush(stdout);
}
 
 
void evolve(double* currentfield, double* newfield, int startX, int endX, int startY, int endY, int w, int h) {
  int x,y;
  for (y = startY; y <= endY; y++) {
    for (x = startX; x <= endX; x++) {
      
      int n = countNeighbours(currentfield, x, y, w, h);
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
    }
  }
}

int countNeighbours(double* currentfield, int x, int y, int width, int height) {
  int n = 0;

  for (int stencilX = (x-1); stencilX <= (x+1); stencilX++) {
    for (int stencilY = (y-1); stencilY <= (y+1); stencilY++) {
      // the modulo operations makes the field be tested in a periodic way
      n += currentfield[calcIndex(width, (stencilX + width) % width, (stencilY + height) % height)];
    }
  }

  // the center of the stencil should not be taken into account
  n -= currentfield[calcIndex(width, x, y)];

  return n;
}

double* readFromASCIIFile(char filename[256], int* w, int* h) {
    FILE* file = fopen(filename, "r"); /* should check the result */

    int size = 10*10;
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
 
// void game() {
//   int w, h;
//   int* width = &w;
//   int* height = &h;
//   double *currentfield = readFromASCIIFile("test.txt", width, height);  //filling(currentfield, w, h);
//   double *newfield = calloc(w*h, sizeof(double));
//   //printf("size unsigned %d, size long %d\n",sizeof(float), sizeof(long));

//   long t;
//   int startX, startY, endX, endY;
//   int xFactor = 3;  // :-)
//   int yFactor = 1;
//   int number_of_areas = xFactor * yFactor;
//   int *area_bounds = calloc(number_of_areas * 4, sizeof(int));
//   int fieldWidth = (w/xFactor) + (w % xFactor > 0 ? 1 : 0);
//   int fieldHeight = (h/yFactor) + (h % yFactor > 0 ? 1 : 0);

//   for (t=0;t<TimeSteps;t++) {
//     show(currentfield, w, h);
        
//     #pragma omp parallel private(startX, startY, endX, endY) firstprivate(fieldWidth, fieldHeight, xFactor, yFactor, w, h) shared(area_bounds) num_threads(number_of_areas)
//     {
//       //printf("fieldWidth=%d\n", fieldWidth);
//       //printf("fieldHeight=%d\n", fieldHeight);
//       startX = fieldWidth * (omp_get_thread_num() % xFactor);
//       endX = (fieldWidth * ((omp_get_thread_num() % xFactor) + 1)) - 1;
//       startY = fieldHeight * (omp_get_thread_num() / xFactor);
//       endY = (fieldHeight * ((omp_get_thread_num() / xFactor) + 1)) - 1;

//       if(omp_get_thread_num() % xFactor == (xFactor - 1)) {
//         endX = w - 1;
//       }

//       if(omp_get_thread_num() / xFactor == (h - 1)) {
//         endY = h - 1;
//       }

//       //printf("Thread %d has area: [%d..%d][%d..%d]\n", omp_get_thread_num(), startX, endX, startY, endY);
      

//       evolve(currentfield, newfield, startX, endX, startY, endY, w, h);
//       writeVTK2Piece(t,currentfield,"gol", startX, endX, startY, endY, w, h, omp_get_thread_num());
//       area_bounds[omp_get_thread_num() * 4] = startX;
//       area_bounds[omp_get_thread_num() * 4 + 1] = endX;
//       area_bounds[omp_get_thread_num() * 4 + 2] = startY;
//       area_bounds[omp_get_thread_num() * 4 + 3] = endY;
//     }

//     writeVTK2Container(t,currentfield,"gol", w, h, area_bounds, number_of_areas);
    
    
//     printf("%ld timestep\n",t);
//     usleep(200000);

//     //SWAP
//     double *temp = currentfield;
//     currentfield = newfield;
//     newfield = temp;
//   }
  
//   free(currentfield);
//   free(newfield);
//   free(area_bounds);
// }

void game(int w, int h, int* initialField, MPI_Comm communicator, int rank, int num_processes) {

}


double* initializeField(int w, int h) {
  double* field = calloc(w * h, sizeof(double));
  return field;
}

int* computeSendBuffer(double* field, int w, int h, int num_processes, MPI_Comm communicator) {
  int sendBuffer[w * h];
  int partialWidth = w / num_processes;
  int sendCount = partialWidth * h;
  int index;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      index = calcIndex(sendCount, (h * y) + x % partialWidth, x / partialWidth);
      sendBuffer[index] = field[calcIndex(w, x, y)];
      // printf("map (%2d, %d) to (%d, %d)\n", x, y, (h * y) + x % partialWidth, x / partialWidth);
      // printf("index = %2d\n", index);
    }
  }

  return sendBuffer;
}
 
int main(int argc, char *argv[]) {
  int w = 0, h = 0;
  if (argc > 1) w = atoi(argv[1]); ///< read width
  if (argc > 2) h = atoi(argv[2]); ///< read height
  if (w <= 0) w = 12; ///< default width
  if (h <= 0) h = 4; ///< default height

  int rank, num_processes, leftNeighbourRank, rightNeighbourRank;
  MPI_Comm world = MPI_COMM_WORLD;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(world, &num_processes);

  int processes_per_dimension[1] = { num_processes };
  int is_periodic_per_dimension[1] = { 1 }; // the 1D topology is periodic at it's left and right border
  MPI_Comm topology_comm;
  MPI_Cart_create(world, 1, processes_per_dimension, is_periodic_per_dimension, 1, &topology_comm);
  MPI_Comm_rank(topology_comm, &rank);
  MPI_Comm_size(topology_comm, &num_processes);
  
  printf("Number of processes: %d\n", num_processes);

  int* sendBuffer;
  int receiveBuffer[sendCount];
  int sendCount = (w / num_processes) * h;
  if (rank == 0) {
    double* field = initializeField(w, h);
    // int partialFieldSize = (w / num_processes) * h;
    sendBuffer = computeSendBuffer(field, w, h, num_processes, topology_comm);
  }

  // MPI_Scatter to distribute to all other processes
  MPI_Scatter(sendBuffer, sendCount, MPI_INT, receiveBuffer, sendCount, MPI_INT, 0, topology_comm);

  game(w, h, receiveBuffer, topology_comm, rank, num_processes);
  
  // MPI_Cart_shift(topology_comm, 0, 1, &leftNeighbourRank, &rightNeighbourRank);
  
  // printf("(left_rank=%d | my_rank=%d | right_rank=%d)\n", leftNeighbourRank, rank, rightNeighbourRank);

  MPI_Finalize();
  return 0;
}
