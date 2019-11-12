#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include "Filter.h"
#include <omp.h>

using namespace std;

#include "rdtsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{
    #pragma omp parallel num_threads(8)

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

struct Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}


double
applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

    long long cycStart, cycStop;

    cycStart = rdtscll();

    //create local variables instead of dereferenced memory
    output -> width = input -> width;
    output -> height = input -> height;
    
    //calculate outside loop
    unsigned int height = input -> height - 1;
    unsigned int width = input -> width - 1;
    
    //t0 - t2 correspond to the unrolling of the plane loop. plane = 0 => t0 ...
    int t0, t1, t2, k, r; 
    unsigned int a, b, c, d;
    
    //array to hold filter values
    int temp_filter[3][3];
    
    //move getDivisor() outside of loop
    unsigned int div = filter -> getDivisor();
    
    //New loop to parse the x, y filter values of the filter outside main loop
    for (int j = 0; j < 3; j++){ 
        for (int i = 0; i < 3; i++){
            temp_filter[i][j] = filter -> get(i, j);
        }
    }
    
    if (temp_filter[1][0] == 0 && temp_filter[1][1] == 0 && temp_filter[1][2] == 0){
        for (int row = height - 1; row != 0; row--){
            a = row - 1;
            for (int col = width - 1; col != 0; col--){
                b = col - 1;
                k = 0;
                r = 0;
                //t0 - t2 in Stride-1 
                t0 = 0;
                t1 = 0;
                t2 = 0;
                    
                for (int j = 0; j < 3; j++){
                    c = b + j;
                    t0 += (input -> color[0][a][c] * temp_filter[0][j]) + 
                        (input -> color[0][a + 2][c] * temp_filter[2][j]);
                    t1 += (input -> color[1][a][c] * temp_filter[0][j]) + 
                            (input -> color[1][a + 2][c] * temp_filter[2][j]);
                    t2 += (input -> color[2][a][c] * temp_filter[0][j]) + 
                        (input -> color[2][a + 2][c] * temp_filter[2][j]);
                }
                //Use nested conditionals instead of if statements
                t0 = t0 < 0 ? 0 : t0 > 255 ? 255 : t0;
                t1 = t1 < 0 ? 0 : t1 > 255 ? 255 : t1;
                t2 = t2 < 0 ? 0 : t2 > 255 ? 255 : t2;
                output -> color[0][row][col] = t0;
                output -> color[1][row][col] = t1;
                output -> color[2][row][col] = t2;
            }
        }
    } else if (temp_filter[0][0] == 1 && temp_filter[0][1] == 1 && temp_filter[0][2] == 1 &&
              temp_filter[1][0] == 1 && temp_filter[1][1] == 1 && temp_filter[1][2] == 1 &&
              temp_filter[2][0] == 1 && temp_filter[2][1] == 1 && temp_filter[2][2] == 1){
        for (int row = height - 1; row != 0; row--){
            a = row - 1;
            for (int col = width - 1; col != 0; col--){
                b = col - 1;
                t0 = 0;
                t1 = 0;
                t2 = 0;
                    
                for (int i = 0; i < 3; i++){
                    d = a + i;
                    for (int j = 0; j < 3; j++){
                        t0 += (input -> color[0][d][b + j]);
                        t1 += (input -> color[1][d][b + j]);
                        t2 += (input -> color[2][d][b + j]);
                    }
                }
                    
                t0 /= div;
                t1 /= div;
                t2 /= div;
                    
                t0 = t0 < 0 ? 0 : t0 > 255 ? 255 : t0;
                t1 = t1 < 0 ? 0 : t1 > 255 ? 255 : t1;
                t2 = t2 < 0 ? 0 : t2 > 255 ? 255 : t2;
                output -> color[0][row][col] = t0;
                output -> color[1][row][col] = t1;
                output -> color[2][row][col] = t2;
            }
        }
    } else if (div == 1) {
        for (int row = height - 1; row != 0; row--){
            a = row - 1;
            for (int col = width - 1; col != 0; col--){
                b = col - 1;
                t0 = 0;
                t1 = 0;
                t2 = 0;
                    
                for (int i = 0; i < 3; i++){
                    d = a + i;
                    for (int j = 0; j < 3; j++){
                        c = b + j;
                        t0 += (input -> color[0][d][c] * temp_filter[i][j]);
                        t1 += (input -> color[1][d][c] * temp_filter[i][j]);
                        t2 += (input -> color[2][d][c] * temp_filter[i][j]);
                    }
                }
                    
                t0 = t0 < 0 ? 0 : t0 > 255 ? 255 : t0;
                t1 = t1 < 0 ? 0 : t1 > 255 ? 255 : t1;
                t2 = t2 < 0 ? 0 : t2 > 255 ? 255 : t2;
                output -> color[0][row][col] = t0;
                output -> color[1][row][col] = t1;
                output -> color[2][row][col] = t2;
            }
        }
    } else {
        for (int row = height - 1; row != 0; row--){
            a = row - 1;
            for (int col = width - 1; col != 0; col--){
                b = col - 1;
                t0 = 0;
                t1 = 0;
                t2 = 0;
                k = 0;
                r = 0;
                    
                for (int j = 0; j < 3; j++){
                    c = b + j;
                    for (int i = 0; i < 3; i++){
                        d = a + i;
                        t0 += (input -> color[0][d][c] * temp_filter[i][j]);
                        t1 += (input -> color[1][d][c] * temp_filter[i][j]);
                        t2 += (input -> color[2][d][c] * temp_filter[i][j]);
                    }
                }
                t0 /= div;
                t1 /= div;
                t2 /= div;
                
                t0 = t0 < 0 ? 0 : t0 > 255 ? 255 : t0;
                t1 = t1 < 0 ? 0 : t1 > 255 ? 255 : t1;
                t2 = t2 < 0 ? 0 : t2 > 255 ? 255 : t2;
                output -> color[0][row][col] = t0;
                output -> color[1][row][col] = t1;
                output -> color[2][row][col] = t2;
            }
        }
    }
    
    cycStop = rdtscll();
    double diff = cycStop - cycStart;
    double diffPerPixel = diff / (output -> width * output -> height);
    fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
    return diffPerPixel;
}
