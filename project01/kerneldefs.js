
   var gpu = new GPU();

   function sqr(x) {
      return x*x;
   }
   function dist(x1,y1,x2,y2) {
      return Math.sqrt( sqr(x2-x1)+sqr(y2-y1) );
   }

   gpu.addFunction(sqr);
   gpu.addFunction(dist);

   function makeAnim(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };

      var y = gpu.createKernel(function(img) {
            return img[this.thread.z][this.thread.y][this.thread.x];
      }, opt);
      return y;
   }

   function makefx4Anim(mode) {
    var opt = {
	dimensions: [250, 159, 4],
	debug: true,
	graphical: false,
	outputToTexture: true,
	mode: mode
    };

    var y = gpu.createKernel(function(img) {
            return img[this.thread.z][this.thread.y][this.thread.x];
	}, opt);
    return y;
    }


   var toimg = gpu.createKernel(function(A) {
    this.color(A[0][this.thread.y][this.thread.x],A[1][this.thread.y][this.thread.x],A[2][this.thread.y][this.thread.x]);
   }).dimensions([800, 600]).graphical(true);

  function makeEdgeFilter(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 0 && this.thread.y < 600-2 && this.thread.x < 800-2 && this.thread.x >0 && this.thread.z <3) {
             var c = A[this.thread.z][this.thread.y-1][this.thread.x-1]*-1 +
                     A[this.thread.z][this.thread.y][this.thread.x-1]*-2 +
                     A[this.thread.z][this.thread.y+1][this.thread.x-1]*-1 +
//                     A[this.thread.z][this.thread.y-1][this.thread.x]*-1 +
//                     A[this.thread.z][this.thread.y][this.thread.x]*9 +
//                     A[this.thread.z][this.thread.y+1][this.thread.x]*-1 +
                     A[this.thread.z][this.thread.y-1][this.thread.x+1] +
                     A[this.thread.z][this.thread.y][this.thread.x+1]*2 +
                     A[this.thread.z][this.thread.y+1][this.thread.x+1];

             var d = A[this.thread.z][this.thread.y-1][this.thread.x-1]*-1 +
                     A[this.thread.z][this.thread.y-1][this.thread.x]*-2 +
                     A[this.thread.z][this.thread.y-1][this.thread.x+1]*-1 +
//                     A[this.thread.z][this.thread.y-1][this.thread.x]*-1 +
//                     A[this.thread.z][this.thread.y][this.thread.x]*9 +
//                     A[this.thread.z][this.thread.y+1][this.thread.x]*-1 +
                     A[this.thread.z][this.thread.y+1][this.thread.x-1] +
                     A[this.thread.z][this.thread.y+1][this.thread.x]*2 +
                     A[this.thread.z][this.thread.y+1][this.thread.x+1];
                 return (c+d)  / 2;
          } else {
             return A[this.thread.z][this.thread.y][this.thread.x];
          }
   },opt);
      return filt;
   }

  function makeEdgeFilter2(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 0 && this.thread.y < 600-2 && this.thread.x < 800-2 && this.thread.x >0 && this.thread.z <3) {
            // [ [-1. -1. -1.]
            //   [-1.  8. -1.]
            //   [-1. -1. -1.]]
            return  (A[this.thread.z][this.thread.y-1][this.thread.x-1]*-1 +
                    A[this.thread.z][this.thread.y-1][this.thread.x]*-1 +
                    A[this.thread.z][this.thread.y-1][this.thread.x+1]*-1 +
                    A[this.thread.z][this.thread.y][this.thread.x-1]*-1 +
                    A[this.thread.z][this.thread.y][this.thread.x]*8 +
                    A[this.thread.z][this.thread.y][this.thread.x+1]*-1 +
                    A[this.thread.z][this.thread.y+1][this.thread.x-1]*-1 +
                    A[this.thread.z][this.thread.y+1][this.thread.x]*-1 +
                    A[this.thread.z][this.thread.y+1][this.thread.x+1]*-1);
          } else {
            return A[this.thread.z][this.thread.y][this.thread.x];
          }
   },opt);
      return filt;
   }

  function makeEdgeSharpenFilter(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 0 && this.thread.y < 600-2 && this.thread.x < 800-2 && this.thread.x >0 && this.thread.z <3) {
// [[ 42.  43.  42.]
//  [ 43.  44.  43.]
//  [ 42.  43.  42.]]

          return  A[this.thread.z][this.thread.y-1][this.thread.x-1]*42 +
                  A[this.thread.z][this.thread.y-1][this.thread.x]*43 +
                  A[this.thread.z][this.thread.y-1][this.thread.x+1]*42 +
                  A[this.thread.z][this.thread.y][this.thread.x-1]*43 + 
                  A[this.thread.z][this.thread.y][this.thread.x]*44 +
                  A[this.thread.z][this.thread.y][this.thread.x+1]*43 +
                  A[this.thread.z][this.thread.y+1][this.thread.x-1]*42 + 
                  A[this.thread.z][this.thread.y+1][this.thread.x]*43 + 
                  A[this.thread.z][this.thread.y+1][this.thread.x+1]*42;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeSharpenFilter(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 0 && this.thread.y < 600-2 && this.thread.x < 800-2 && this.thread.x >0 && this.thread.z <3) {
          //          [ 0 -1  0 ]
          // kernel = [-1  5 -1 ]
          //          [ 0 -1  0 ]
          return A[this.thread.z][this.thread.y-1][this.thread.x]*-1 +
                 A[this.thread.z][this.thread.y][this.thread.x-1]*-1 +
                 A[this.thread.z][this.thread.y][this.thread.x]*5 +
                 A[this.thread.z][this.thread.y][this.thread.x+1]*-1 +
                 A[this.thread.z][this.thread.y+1][this.thread.x]*-1;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur3x3Filter(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 0 && this.thread.y < 600-2 && this.thread.x < 800-2 && this.thread.x >0 && this.thread.z <3) {
          //          [ 1  1  1 ]
          // kernel = [ 1  1  1 ] x 1/9
          //          [ 1  1  1 ]
          var res = A[this.thread.z][this.thread.y-1][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-1][this.thread.x] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+1] +

                    A[this.thread.z][this.thread.y][this.thread.x-1] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x+1] +

                    A[this.thread.z][this.thread.y+1][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+1][this.thread.x] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+1];

          return res / 9.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur3x3FilterH(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.x < 800-2 && this.thread.x >0 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y][this.thread.x-1] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x+1];
          return res / 3.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur3x3FilterV(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 0 && this.thread.y < 600-2 && this.thread.z <3) {

          var res = A[this.thread.z][this.thread.y-1][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y+1][this.thread.x];
          return res / 3.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur5x5Filter(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 1 && this.thread.y < 600-3 && this.thread.x < 800-3 && this.thread.x > 1 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y-2][this.thread.x-2] + 
                    A[this.thread.z][this.thread.y-2][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-2][this.thread.x] +
                    A[this.thread.z][this.thread.y-2][this.thread.x+1] +
                    A[this.thread.z][this.thread.y-2][this.thread.x+2] +

                    A[this.thread.z][this.thread.y-1][this.thread.x-2] +
                    A[this.thread.z][this.thread.y-1][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-1][this.thread.x] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+1] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+2] +

                    A[this.thread.z][this.thread.y][this.thread.x-2] +
                    A[this.thread.z][this.thread.y][this.thread.x-1] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x+1] +
                    A[this.thread.z][this.thread.y][this.thread.x+2] +

                    A[this.thread.z][this.thread.y+1][this.thread.x-2] +
                    A[this.thread.z][this.thread.y+1][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+1][this.thread.x] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+1] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+2] +

                    A[this.thread.z][this.thread.y+2][this.thread.x-2] + 
                    A[this.thread.z][this.thread.y+2][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+2][this.thread.x] +
                    A[this.thread.z][this.thread.y+2][this.thread.x+1] +
                    A[this.thread.z][this.thread.y+2][this.thread.x+2];

          return res / 25.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }
  function makeBlur5x5FilterH(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.x < 800-3 && this.thread.x > 1 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y][this.thread.x-2] + 
                    A[this.thread.z][this.thread.y][this.thread.x-1] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x+1] +
                    A[this.thread.z][this.thread.y][this.thread.x+2];

          return res / 5.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur5x5FilterV(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 1 && this.thread.y < 600-3 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y-2][this.thread.x] + 
                    A[this.thread.z][this.thread.y-1][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y+1][this.thread.x] +
                    A[this.thread.z][this.thread.y+2][this.thread.x];

          return res / 5.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur7x7Filter(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 2 && this.thread.y < 600-4 && this.thread.x < 800-4 && this.thread.x > 2 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y-3][this.thread.x-3] +
                    A[this.thread.z][this.thread.y-3][this.thread.x-2] +
                    A[this.thread.z][this.thread.y-3][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-3][this.thread.x] + 
                    A[this.thread.z][this.thread.y-3][this.thread.x+1] + 
                    A[this.thread.z][this.thread.y-3][this.thread.x+2] +
                    A[this.thread.z][this.thread.y-3][this.thread.x+3] +

                    A[this.thread.z][this.thread.y-2][this.thread.x-3] +
                    A[this.thread.z][this.thread.y-2][this.thread.x-2] + 
                    A[this.thread.z][this.thread.y-2][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-2][this.thread.x] +
                    A[this.thread.z][this.thread.y-2][this.thread.x+1] +
                    A[this.thread.z][this.thread.y-2][this.thread.x+2] +
                    A[this.thread.z][this.thread.y-2][this.thread.x+3] +

                    A[this.thread.z][this.thread.y-1][this.thread.x-3] +
                    A[this.thread.z][this.thread.y-1][this.thread.x-2] +
                    A[this.thread.z][this.thread.y-1][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-1][this.thread.x] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+1] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+2] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+3] +

                    A[this.thread.z][this.thread.y][this.thread.x-3] +
                    A[this.thread.z][this.thread.y][this.thread.x-2] +
                    A[this.thread.z][this.thread.y][this.thread.x-1] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x+1] +
                    A[this.thread.z][this.thread.y][this.thread.x+2] +
                    A[this.thread.z][this.thread.y][this.thread.x+3] +

                    A[this.thread.z][this.thread.y+1][this.thread.x-3] +
                    A[this.thread.z][this.thread.y+1][this.thread.x-2] +
                    A[this.thread.z][this.thread.y+1][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+1][this.thread.x] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+1] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+2] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+3] +

                    A[this.thread.z][this.thread.y+2][this.thread.x-3] +
                    A[this.thread.z][this.thread.y+2][this.thread.x-2] + 
                    A[this.thread.z][this.thread.y+2][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+2][this.thread.x] +
                    A[this.thread.z][this.thread.y+2][this.thread.x+1] +
                    A[this.thread.z][this.thread.y+2][this.thread.x+2] +
                    A[this.thread.z][this.thread.y+2][this.thread.x+3] +

                    A[this.thread.z][this.thread.y+3][this.thread.x-3] +
                    A[this.thread.z][this.thread.y+3][this.thread.x-2] +
                    A[this.thread.z][this.thread.y+3][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+3][this.thread.x] + 
                    A[this.thread.z][this.thread.y+3][this.thread.x+1] + 
                    A[this.thread.z][this.thread.y+3][this.thread.x+2] +
                    A[this.thread.z][this.thread.y+3][this.thread.x+3];

          return res / 49.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur7x7FilterH(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.x < 800-4 && this.thread.x > 2 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y][this.thread.x-3] +
                    A[this.thread.z][this.thread.y][this.thread.x-2] +
                    A[this.thread.z][this.thread.y][this.thread.x-1] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x+1] +
                    A[this.thread.z][this.thread.y][this.thread.x+2] +
                    A[this.thread.z][this.thread.y][this.thread.x+3];

          return res / 7.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur7x7FilterV(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y < 600-4 && this.thread.y > 2 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y-3][this.thread.x] +
                    A[this.thread.z][this.thread.y-2][this.thread.x] +
                    A[this.thread.z][this.thread.y-1][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y+1][this.thread.x] +
                    A[this.thread.z][this.thread.y+2][this.thread.x] +
                    A[this.thread.z][this.thread.y+3][this.thread.x];

          return res / 7.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur9x9Filter(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y > 3 && this.thread.y < 600-5 && this.thread.x < 800-5 && this.thread.x > 3 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y-4][this.thread.x-4] +
                    A[this.thread.z][this.thread.y-4][this.thread.x-3] +
                    A[this.thread.z][this.thread.y-4][this.thread.x-2] +
                    A[this.thread.z][this.thread.y-4][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-4][this.thread.x] + 
                    A[this.thread.z][this.thread.y-4][this.thread.x+1] + 
                    A[this.thread.z][this.thread.y-4][this.thread.x+2] +
                    A[this.thread.z][this.thread.y-4][this.thread.x+3] +
                    A[this.thread.z][this.thread.y-4][this.thread.x+4] +

                    A[this.thread.z][this.thread.y-3][this.thread.x-4] +
                    A[this.thread.z][this.thread.y-3][this.thread.x-3] +
                    A[this.thread.z][this.thread.y-3][this.thread.x-2] +
                    A[this.thread.z][this.thread.y-3][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-3][this.thread.x] + 
                    A[this.thread.z][this.thread.y-3][this.thread.x+1] + 
                    A[this.thread.z][this.thread.y-3][this.thread.x+2] +
                    A[this.thread.z][this.thread.y-3][this.thread.x+3] +
                    A[this.thread.z][this.thread.y-3][this.thread.x+4] +

                    A[this.thread.z][this.thread.y-2][this.thread.x-4] +
                    A[this.thread.z][this.thread.y-2][this.thread.x-3] +
                    A[this.thread.z][this.thread.y-2][this.thread.x-2] + 
                    A[this.thread.z][this.thread.y-2][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-2][this.thread.x] +
                    A[this.thread.z][this.thread.y-2][this.thread.x+1] +
                    A[this.thread.z][this.thread.y-2][this.thread.x+2] +
                    A[this.thread.z][this.thread.y-2][this.thread.x+3] +
                    A[this.thread.z][this.thread.y-2][this.thread.x+4] +

                    A[this.thread.z][this.thread.y-1][this.thread.x-4] +
                    A[this.thread.z][this.thread.y-1][this.thread.x-3] +
                    A[this.thread.z][this.thread.y-1][this.thread.x-2] +
                    A[this.thread.z][this.thread.y-1][this.thread.x-1] +
                    A[this.thread.z][this.thread.y-1][this.thread.x] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+1] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+2] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+3] +
                    A[this.thread.z][this.thread.y-1][this.thread.x+4] +

                    A[this.thread.z][this.thread.y][this.thread.x-4] +
                    A[this.thread.z][this.thread.y][this.thread.x-3] +
                    A[this.thread.z][this.thread.y][this.thread.x-2] +
                    A[this.thread.z][this.thread.y][this.thread.x-1] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x+1] +
                    A[this.thread.z][this.thread.y][this.thread.x+2] +
                    A[this.thread.z][this.thread.y][this.thread.x+3] +
                    A[this.thread.z][this.thread.y][this.thread.x+4] +

                    A[this.thread.z][this.thread.y+1][this.thread.x-4] +
                    A[this.thread.z][this.thread.y+1][this.thread.x-3] +
                    A[this.thread.z][this.thread.y+1][this.thread.x-2] +
                    A[this.thread.z][this.thread.y+1][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+1][this.thread.x] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+1] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+2] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+3] +
                    A[this.thread.z][this.thread.y+1][this.thread.x+4] +

                    A[this.thread.z][this.thread.y+2][this.thread.x-4] +
                    A[this.thread.z][this.thread.y+2][this.thread.x-3] +
                    A[this.thread.z][this.thread.y+2][this.thread.x-2] + 
                    A[this.thread.z][this.thread.y+2][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+2][this.thread.x] +
                    A[this.thread.z][this.thread.y+2][this.thread.x+1] +
                    A[this.thread.z][this.thread.y+2][this.thread.x+2] +
                    A[this.thread.z][this.thread.y+2][this.thread.x+3] +
                    A[this.thread.z][this.thread.y+2][this.thread.x+4] +

                    A[this.thread.z][this.thread.y+3][this.thread.x-4] +
                    A[this.thread.z][this.thread.y+3][this.thread.x-3] +
                    A[this.thread.z][this.thread.y+3][this.thread.x-2] +
                    A[this.thread.z][this.thread.y+3][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+3][this.thread.x] + 
                    A[this.thread.z][this.thread.y+3][this.thread.x+1] + 
                    A[this.thread.z][this.thread.y+3][this.thread.x+2] +
                    A[this.thread.z][this.thread.y+3][this.thread.x+3] +
                    A[this.thread.z][this.thread.y+3][this.thread.x+4] +

                    A[this.thread.z][this.thread.y+4][this.thread.x-4] +
                    A[this.thread.z][this.thread.y+4][this.thread.x-3] +
                    A[this.thread.z][this.thread.y+4][this.thread.x-2] +
                    A[this.thread.z][this.thread.y+4][this.thread.x-1] +
                    A[this.thread.z][this.thread.y+4][this.thread.x] + 
                    A[this.thread.z][this.thread.y+4][this.thread.x+1] + 
                    A[this.thread.z][this.thread.y+4][this.thread.x+2] +
                    A[this.thread.z][this.thread.y+4][this.thread.x+3] +
                    A[this.thread.z][this.thread.y+4][this.thread.x+4];

          return res / 81.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }
  function makeBlur9x9FilterH(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.x < 800-5 && this.thread.x > 3 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y][this.thread.x-4] +
                    A[this.thread.z][this.thread.y][this.thread.x-3] +
                    A[this.thread.z][this.thread.y][this.thread.x-2] +
                    A[this.thread.z][this.thread.y][this.thread.x-1] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x+1] +
                    A[this.thread.z][this.thread.y][this.thread.x+2] +
                    A[this.thread.z][this.thread.y][this.thread.x+3] +
                    A[this.thread.z][this.thread.y][this.thread.x+4];

          return res / 9.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }

  function makeBlur9x9FilterV(mode) {
      var opt = {
         dimensions: [800, 600, 4],
         debug: true,
         graphical: false,
         outputToTexture: true,
         mode: mode
      };
      var filt = gpu.createKernel(function(A) {
        if (this.thread.y < 600-5 && this.thread.y > 3 && this.thread.z <3) {
          var res = A[this.thread.z][this.thread.y-4][this.thread.x] +
                    A[this.thread.z][this.thread.y-3][this.thread.x] +
                    A[this.thread.z][this.thread.y-2][this.thread.x] +
                    A[this.thread.z][this.thread.y-1][this.thread.x] +
                    A[this.thread.z][this.thread.y][this.thread.x] +
                    A[this.thread.z][this.thread.y+1][this.thread.x] +
                    A[this.thread.z][this.thread.y+2][this.thread.x] +
                    A[this.thread.z][this.thread.y+3][this.thread.x] +
                    A[this.thread.z][this.thread.y+4][this.thread.x];

          return res / 9.0;
        } else {
           return A[this.thread.z][this.thread.y][this.thread.x];
        }
   },opt);
      return filt;
   }
function makeAnimator(mode) {
    var opt = {
	dimensions: [800, 600, 4],
	debug: true,
	graphical: false,
	outputToTexture: true,
	mode: mode
    };
    var filt = gpu.createKernel(function(A,x) {
	return A[this.thread.z][this.thread.y][(this.thread.x + x)];
	  },opt);
      return filt;
}
