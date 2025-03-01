#include <sys/time.h>
//#include <sys/resource.h>
#include <windows.h>
#include <process.h>
#include <math.h>

// Some useful functions
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define PI 3.14159265358979323846

// Variables used by FFT functions
double *_sintbl = 0;
int maxfftsize = 0;


/* ---- Prototypes ------------------------- */

// I/O-related function
int write_buff_dump(double *buff, const int n_buff, double *buff_dump, const int n_buff_dump, int *ind_dump);

// Time-related function
double get_process_time();
double get_process_time_windows();

// FFT-related functions (from SPTK toolbox https://sp-tk.sourceforge.net)
int fft(double *x, double *y, const int m);
int ifft(double *x, double *y, const int m);
int fftr(double *x, double *y, const int m);
int ifftr(double *x, double *y, const int l);
int get_nextpow2(int n);
static int checkm(const int m);

// Hanning window, not currently used, for bonus questions only (from SPTK toolbox https://sp-tk.sourceforge.net)
//static double *hanning(double *w, const int leng);

// Memory-related functions, used by FFT-related functions (from SPTK toolbox https://sp-tk.sourceforge.net)
char *getmem(int leng, unsigned size);
double *dgetmem(int leng);


///////////////////////////////////////////////

/* ---- IO-related function ---------------- */

int write_buff_dump(double* buff, const int n_buff, double* buff_dump, const int n_buff_dump, int* ind_dump) {
   
   
  int i = 0;
  for (i = 0; i < n_buff; i++) 
  {
   
    if (*ind_dump < n_buff_dump) {
      buff_dump[*ind_dump] = buff[i];
      (*ind_dump)++;
      } 
    else {
      break;
    }
  }

  return i;
}

/* ---- Time-related function -------------- */
/*
double get_process_time() {
    struct rusage usage;
    if( 0 == getrusage(RUSAGE_SELF, &usage) ) {
        return (double)(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) +
               (double)(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1.0e6;
    }
    return 0;
}
*/

double * autocor(double *  sig, int n){
   //get number of elements in sig
    double * ac = (double *) calloc(n, sizeof(double));
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n - i; j++){
            ac[i] += sig[j] * sig[j + i];
        }
    }
    return ac;   
}

double * circ_autocor(double *  circ_buffer, int n, int* read_ind){
   //get number of elements in sig
    double * ac = (double *) calloc(n, sizeof(double));
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n - i; j++){
            ac[i] += circ_buffer[(*read_ind + j) % n] * circ_buffer[(*read_ind + j + i) % n];
        }
    }
    return ac;   
}

int extract_f0(double * ac, int n, int fs, int min_f0, int max_f0){
   double max_1 = 0;
   int max_ind_1 = 0;
   double max_2 = 0;
   int max_ind_2 = 0;
   int min_ind = (int) (fs / max_f0);
   int max_ind = (int) (fs / min_f0);
   for (int i = 0; i < n; i++){
         if (ac[i] > max_1){
            max_1 = ac[i];
            max_ind_1 = i;
         }
         else if(ac[i] > max_2 && i > min_ind && i < max_ind){
            max_2 = ac[i];
            max_ind_2 = i;
         }
   }
   return (int) (fs / (max_ind_2 - max_ind_1));
}


// Sous windows
double get_process_time_windows(){
    struct timeval time_now{};
    gettimeofday(&time_now, nullptr);
    time_t msecs_time = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    return (double) msecs_time;
}

/* ---- FFT-related functions -------------- */

int get_nextpow2(int n)
{
  int k = 1;
  while (k < n){
    k *= 2;
  }

  return k;
}


/********************************************************
 $Id$                     
 
 NAME:                     
        fftr - Fast Fourier Transform for Double sequence      
 SYNOPSIS:                                             
        int   fftr(x, y, m)            
                     
        double  x[];   real part of data      
        double  y[];   working area         
        int     m;     number of data(radix 2)      
                Naohiro Isshiki    Dec.1995   modified
********************************************************/

int fftr(double *x, double *y, const int m)
{
   int i, j;
   double *xp, *yp, *xq;
   double *yq;
   int mv2, n, tblsize;
   double xt, yt, *sinp, *cosp;
   double arg;

   mv2 = m / 2;

   /* separate even and odd  */
   xq = xp = x;
   yp = y;
   for (i = mv2; --i >= 0;) {
      *xp++ = *xq++;
      *yp++ = *xq++;
   }

   if (fft(x, y, mv2) == -1)    /* m / 2 point fft */
      return (-1);


   /***********************
   * SIN table generation *
   ***********************/

   if ((_sintbl == 0) || (maxfftsize < m)) {
      tblsize = m - m / 4 + 1;
      arg = PI / m * 2;
      if (_sintbl != 0)
         free(_sintbl);
      _sintbl = sinp = dgetmem(tblsize);
      *sinp++ = 0;
      for (j = 1; j < tblsize; j++)
         *sinp++ = sin(arg * (double) j);
      _sintbl[m / 2] = 0;
      maxfftsize = m;
   }
   //printf("Debug: m=%i, maxfftsize=%i\n",m,maxfftsize);

   n = maxfftsize / m;
   sinp = _sintbl;
   cosp = _sintbl + maxfftsize / 4;

   xp = x;
   yp = y;
   xq = xp + m;
   yq = yp + m;
   *(xp + mv2) = *xp - *yp;
   *xp = *xp + *yp;
   *(yp + mv2) = *yp = 0;

   for (i = mv2, j = mv2 - 2; --i; j -= 2) {
      ++xp;
      ++yp;
      sinp += n;
      cosp += n;
      yt = *yp + *(yp + j);
      xt = *xp - *(xp + j);
      *(--xq) = (*xp + *(xp + j) + *cosp * yt - *sinp * xt) * 0.5;
      *(--yq) = (*(yp + j) - *yp + *sinp * yt + *cosp * xt) * 0.5;
   }

   xp = x + 1;
   yp = y + 1;
   xq = x + m;
   yq = y + m;

   for (i = mv2; --i;) {
      *xp++ = *(--xq);
      *yp++ = -(*(--yq));
   }

   return (0);
}


/***************************************************************
    $Id$

    Inverse Fast Fourier Transform for Real Sequence

    int ifftr(x, y, l)

    double *x : real part of data
    double *y : working area
    int     l : number of data(radix 2)

***************************************************************/

int ifftr(double *x, double *y, const int l)
{
   int i;
   double *xp, *yp;

   fftr(x, y, l);

   xp = x;
   yp = y;
   i = l;
   while (i--) {
      *xp++ /= l;
      *yp++ /= -l;
   }

   return (0);
}


static int checkm(const int m)
{
   int k;

   for (k = 4; k <= m; k <<= 1) {
      if (k == m)
         return (0);
   }
   fprintf(stderr, "fft : m must be a integer of power of 2! (m=%i)\n",m);

   return (-1);
}


/********************************************************
   $Id$               
       NAME:               
                fft - fast fourier transform    
       SYNOPSIS:               
                int   fft(x, y, m);         
                     
                double   x[];   real part      
                double   y[];   imaginary part      
                int      m;     data size      
   
                return : success = 0
                         fault   = -1
       Naohiro Isshiki          Dec.1995    modified   
********************************************************/

int fft(double *x, double *y, const int m)
{
   int j, lmx, li;
   double *xp, *yp;
   double *sinp, *cosp;
   int lf, lix, tblsize;
   int mv2, mm1;
   double t1, t2;
   double arg;
   int checkm(const int);

   /**************
   * RADIX-2 FFT *
   **************/

   if (checkm(m))
      return (-1);

   /***********************
   * SIN table generation *
   ***********************/

   if ((_sintbl == 0) || (maxfftsize < m)) {
      tblsize = m - m / 4 + 1;
      arg = PI / m * 2;
      if (_sintbl != 0)
         free(_sintbl);
      _sintbl = sinp = dgetmem(tblsize);
      *sinp++ = 0;
      for (j = 1; j < tblsize; j++)
         *sinp++ = sin(arg * (double) j);
      _sintbl[m / 2] = 0;
      maxfftsize = m;
   }

   lf = maxfftsize / m;
   lmx = m;

   for (;;) {
      lix = lmx;
      lmx /= 2;
      if (lmx <= 1)
         break;
      sinp = _sintbl;
      cosp = _sintbl + maxfftsize / 4;
      for (j = 0; j < lmx; j++) {
         xp = &x[j];
         yp = &y[j];
         for (li = lix; li <= m; li += lix) {
            t1 = *(xp) - *(xp + lmx);
            t2 = *(yp) - *(yp + lmx);
            *(xp) += *(xp + lmx);
            *(yp) += *(yp + lmx);
            *(xp + lmx) = *cosp * t1 + *sinp * t2;
            *(yp + lmx) = *cosp * t2 - *sinp * t1;
            xp += lix;
            yp += lix;
         }
         sinp += lf;
         cosp += lf;
      }
      lf += lf;
   }

   xp = x;
   yp = y;
   for (li = m / 2; li--; xp += 2, yp += 2) {
      t1 = *(xp) - *(xp + 1);
      t2 = *(yp) - *(yp + 1);
      *(xp) += *(xp + 1);
      *(yp) += *(yp + 1);
      *(xp + 1) = t1;
      *(yp + 1) = t2;
   }

   /***************
   * bit reversal *
   ***************/
   j = 0;
   xp = x;
   yp = y;
   mv2 = m / 2;
   mm1 = m - 1;
   for (lmx = 0; lmx < mm1; lmx++) {
      if ((li = lmx - j) < 0) {
         t1 = *(xp);
         t2 = *(yp);
         *(xp) = *(xp + li);
         *(yp) = *(yp + li);
         *(xp + li) = t1;
         *(yp + li) = t2;
      }
      li = mv2;
      while (li <= j) {
         j -= li;
         li /= 2;
      }
      j += li;
      xp = x + j;
      yp = y + j;
   }

   return (0);
}


/*****************************************************************
*  $Id$         *
*  NAME:                                                         *
*      ifft - Inverse Fast Fourier Transform                     *   
*  SYNOPSIS:                                                     *
*      int   ifft(x, y, m)                                       *
*                                                                *
*      real   x[];   real part                                   *
*      real   y[];   imaginary part                              *
*      int    m;     size of FFT                                 *
*****************************************************************/

int ifft(double *x, double *y, const int m)
{
   int i;

   if (fft(y, x, m) == -1)
      return (-1);

   for (i = m; --i >= 0; ++x, ++y) {
      *x /= m;
      *y /= m;
   }

   return (0);
}


/**********************************************
   Hanning window

       double  *hanning(w, leng)
       double  *w   : window values
       int     leng : window length
**********************************************/

/*
static double *hanning(double *w, const int leng)
{
  int i;
  double arg;
  double *p;

  arg = 2*PI / (leng - 1);
  for (p = w, i = 0; i < leng; i++)
    *p++ = 0.5 * (1 - cos(i * arg));

  return (w);
}
*/

/* ---- Memory-related functions ----------- */

double *dgetmem(int leng)
{
    return ( (double *)getmem(leng, sizeof(double)) );
}

char *getmem(int leng, unsigned size)
{
    char *p = NULL;

    if ((p = (char *)calloc(leng, size)) == NULL){
        fprintf(stderr, "Memory allocation error !\n");
        exit(3);
    }
    return (p);
}

