#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/time.h>

#define calcIndex(width, x,y)  ((y)*(width) + (x))

long TimeSteps = 100;

void writeVTK2(long timestep, double *data, char prefix[1024], long w, long h) {
  char filename[2048];  
  int x,y; 
  
  long offsetX=0;
  long offsetY=0;
  float deltax=1.0;
  float deltay=1.0;
  long  nxy = w * h * sizeof(float);  

  snprintf(filename, sizeof(filename), "%s-%05ld%s", prefix, timestep, ".vti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n", offsetX, offsetX + w-1, offsetY, offsetY + h-1, 0, 0, deltax, deltax, 0.0);
  fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
  fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp, "</CellData>\n");
  fprintf(fp, "</ImageData>\n");
  fprintf(fp, "<AppendedData encoding=\"raw\">\n");
  fprintf(fp, "_");
  fwrite((unsigned char*)&nxy, sizeof(long), 1, fp);

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      float value = data[calcIndex(w, x,y)];
      fwrite((unsigned char*)&value, sizeof(float), 1, fp);
    }
  }
  
  fprintf(fp, "\n</AppendedData>\n");
  fprintf(fp, "</VTKFile>\n");
  fclose(fp);
}

void writeVTK2Piece(long timestep, double *data, char prefix[1024],int xStart, int xEnd, int yStart, int yEnd, long w, long h, int thread_num) {
  char filename[2048];  
  int x,y; 
  
  long offsetX=0;
  long offsetY=0;
  float deltax=1.0;
  float deltay=1.0;
  long  nxy = w * h * sizeof(float);  

  snprintf(filename, sizeof(filename), "%s-%05ld-%02d%s", prefix, timestep, thread_num, ".vti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n", offsetX, offsetX + (w-1), offsetY, offsetY + (h-1), 0, 0, deltax, deltax, 0.0);
  fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
  fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp, "</CellData>\n");
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

/*
* This method should create the PImageData Container file for the other vtk files generated by each thread.
*/
/*void writeVTK2Container(long timestep, double *data, char prefix[1024],int xStart, int xEnd, int yStart, int yEnd, long w, long h, int thread_num) {
  char filename[2048];  
  int x,y; 
  
  long offsetX=0;
  long offsetY=0;
  float deltax=1.0;
  float deltay=1.0;
  long  nxy = w * h * sizeof(float);  

  snprintf(filename, sizeof(filename), "%s-%05ld-%02d%s", prefix, timestep, thread_num, ".vti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n", offsetX, offsetX + (xEnd - xStart), offsetY, offsetY + (yEnd - yStart), 0, 0, deltax, deltax, 0.0);
  fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
  fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp, "</CellData>\n");
  fprintf(fp, "</ImageData>\n");
  fprintf(fp, "<AppendedData encoding=\"raw\">\n");
  fprintf(fp, "_");
  fwrite((unsigned char*)&nxy, sizeof(long), 1, fp);

  for (y = yStart; y <= yEnd; y++) {
    for (x = xStart; x <= xEnd; x++) {
      float value = data[calcIndex(h, x,y)];
      fwrite((unsigned char*)&value, sizeof(float), 1, fp);
    }
  }
  
  fprintf(fp, "\n</AppendedData>\n");
  fprintf(fp, "</VTKFile>\n");
  fclose(fp);
}*/


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
 
void filling(double* currentfield, int w, int h) {
  /*int i;
  for (i = 0; i < h*w; i++) {
    currentfield[i] = (rand() < RAND_MAX / 10) ? 1 : 0; ///< init domain randomly
  }*/
  currentfield[0] = 1;
  currentfield[4] = 1;
}
 
void game(int w, int h) {
  double *currentfield = calloc(w*h, sizeof(double));
  double *newfield     = calloc(w*h, sizeof(double));
  
  //printf("size unsigned %d, size long %d\n",sizeof(float), sizeof(long));
  
  filling(currentfield, w, h);
  long t;
  int startX, startY, endX, endY;
  int xFactor = 1;  // :-)
  int yFactor = 1;
  int number_of_areas = xFactor * yFactor;
  int fieldWidth = (w/xFactor) + (w % xFactor > 0 ? 1 : 0);
  int fieldHeight = (h/yFactor) + (h % yFactor > 0 ? 1 : 0);

  for (t=0;t<2;t++) {
    show(currentfield, w, h);
        
    #pragma omp parallel private(startX, startY, endX, endY) firstprivate(fieldWidth, fieldHeight, xFactor, yFactor, w, h) num_threads(number_of_areas)
    {
      //printf("fieldWidth=%d\n", fieldWidth);
      //printf("fieldHeight=%d\n", fieldHeight);
      startX = fieldWidth * (omp_get_thread_num() % xFactor);
      endX = (fieldWidth * ((omp_get_thread_num() % xFactor) + 1)) - 1;
      startY = fieldHeight * (omp_get_thread_num() / xFactor);
      endY = (fieldHeight * ((omp_get_thread_num() / xFactor) + 1)) - 1;

      if(omp_get_thread_num() % xFactor == (xFactor - 1)) {
        endX = w - 1;
      }

      if(omp_get_thread_num() / xFactor == (h - 1)) {
        endY = h - 1;
      }

      //printf("Thread %d has area: [%d..%d][%d..%d]\n", omp_get_thread_num(), startX, endX, startY, endY);
      

      evolve(currentfield, newfield, startX, endX, startY, endY, w, h);
      //writeVTK2Piece(t,currentfield,"gol", startX, endX, startY, endY, w, h, omp_get_thread_num());
      writeVTK2(t,currentfield,"gol", w, h);
    }
    
    
    printf("%ld timestep\n",t);
    usleep(200000);

    //SWAP
    double *temp = currentfield;
    currentfield = newfield;
    newfield = temp;
  }
  
  free(currentfield);
  free(newfield);
  
}
 
int main(int c, char **v) {
  int w = 0, h = 0;
  if (c > 1) w = atoi(v[1]); ///< read width
  if (c > 2) h = atoi(v[2]); ///< read height
  if (w <= 0) w = 5; ///< default width
  if (h <= 0) h = 5; ///< default height
  game(w, h);
}
