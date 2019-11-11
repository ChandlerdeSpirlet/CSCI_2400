#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include "Filter.h"

using namespace std;

#include "rtdsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

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
  }
}


double
applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

  long long cycStart, cycStop;

  cycStart = rdtscll();

  output -> width = input -> width;
  output -> height = input -> height;
  unsigned int h = input -> height - 1;
  unsigned int w = input -> width - 1;
  int t, k, r;
  unsigned int a, b, c, d;
  int filt[3][3];
  unsigned int div = filter -> getDivisor();

  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {	
      filt[i][j] = filter -> get(i, j);
    }
  }

  if (filt[1][0] == 0 && filt[1][1] == 0 && filt[1][2] == 0) {
    for(int plane = 0; plane < 3; plane++) {
      for(int row = h-1; row != 0 ; row--) {
        a = row-1;
        for(int col = w-1; col != 0 ; col--) {
          b = col-1;  

	  t = 0;
          k = 0;
          r = 0;

	  for (int j = 0; j < 3; j++) {
            c = b+j;
	    t += (input -> color[plane][a][c] 
		  * filt[0][j] )
                  + (input -> color[plane][a+2][c] 
	          * filt[2][j] );
	  }

	  if ( t < 0 ) {
	    t = 0;
	  }
	  if ( t  > 255 ) { 
	    t = 255;
	  }

          output -> color[plane][row][col] = t;
        }
      }
    }
  }

  else if (filt[0][0] == 1 && filt[0][1] == 1 && filt[0][2] == 1 &&
           filt[1][0] == 1 && filt[1][1] == 1 && filt[1][2] == 1 &&
           filt[2][0] == 1 && filt[2][1] == 1 && filt[2][2] == 1) {
   for(int plane = 0; plane < 3; plane++) {
     for(int row = h-1; row != 0 ; row--) {
       a = row-1;
         for(int col = w-1; col != 0 ; col--) {
           b = col-1;  

	  t = 0;

          for (int i = 0; i < 3; i++) {	
            d = a+i;
	    for (int j = 0; j < 3; j++) {	  
	      t += (input -> color[plane][d][b+j]);
	    }
	  }

          t /= div;

	  if ( t < 0 ) {
	    t = 0;
	  }

	  if ( t  > 255 ) { 
	    t = 255;
          }

          output -> color[plane][row][col] = t;
        }
      }
    }
  }

  else if (div == 1) {
    for(int plane = 0; plane < 3; plane++) {
      for(int row = h-1; row != 0 ; row--) {
        a = row-1;
        for(int col = w-1; col != 0 ; col--) {
          b = col-1;  

	  t = 0;

          for (int i = 0; i < 3; i++) {	
            d = a+i;
	    for (int j = 0; j < 3; j++) {
              c = b+j;
	  
	      t += (input -> color[plane][d][c] 
		    * filt[i][j] );
	    }
	  }

	  if ( t < 0 ) {
	    t = 0;
	  }

	  if ( t  > 255 ) { 
	    t = 255;
          }

          output -> color[plane][row][col] = t;
        }
      }
    }
  }

  else {
  for(int plane = 0; plane < 3; plane++) {
  for(int row = h-1; row != 0 ; row--) {
    a = row-1;
    for(int col = w-1; col != 0 ; col--) {
        b = col-1;  

	t = 0;
        k = 0;
        r = 0;

	for (int j = 0; j < 3; j++) {
          c = b+j;
	  for (int i = 0; i < 3; i++) {	
            d = a+i;
	    t += (input -> color[plane][d][c] 
		 * filt[i][j] );
	  }
	}
	
	t /= div;

	if ( t < 0 ) {
	  t = 0;
	}
	if ( t  > 255 ) { 
	  t = 255;
	}

      output -> color[plane][row][col] = t;
    }
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
