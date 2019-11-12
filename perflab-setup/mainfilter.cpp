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