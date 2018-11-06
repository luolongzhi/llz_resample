/*
  llzlab - luolongzhi algorithm lab 
  Copyright (C) 2013 luolongzhi 罗龙智 (Chengdu, China)

  This program is part of llzlab, all copyrights are reserved by luolongzhi. 

  filename: llz_fir.c 
  time    : 2012/07/09 23:38 
  author  : luolongzhi ( luolongzhi@gmail.com )
*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <assert.h>
#include "llz_fir.h"

#undef  EPS
#define EPS		1E-16

typedef struct _llz_fir_filter_t {
	double   fc;           //normalized, for lowpass and highpass
	double   fc1,fc2;      //normalized, for bandpass and bandstop

	int     flt_len;
    int     frame_len;
    int     buf_len;
   
	double   *h;
	double   *buf;
}llz_fir_filter_t;


/*
 * Calculate the sin(pi*x)/(pi*x)
 */
static double sinc (double x)
{
    double xm, val;

    xm = .0;
    /* Calculate sin(x)/x, taking care at multiples of pi */
    if (x == 0.0)
        val = 1.0;
    else {
        val = sin (M_PI * xm) / (M_PI * x);

        xm = fmod (x, 2.0);
        if (x == floor (x))
            val = 0.0;
        else
            val = sin (M_PI * xm) / (M_PI * x);

    }

    return val;
}

int llz_hamming(double *w,const int N)
{
	int i,j;
    
	for (i = 0 , j = N-1; i <= j ; i++, j--) {
		w[i] = (double)(0.54-0.46*cos(2*M_PI*i/(N-1)));
        w[j] = w[i];
    }

	return N;
}

int llz_blackman(double *w,const int N)
{
	int i,j;
    
	for (i = 0 , j = N-1; i <= j ; i++, j--) {
		w[i] = (double)(0.42 - 0.5*cos(2*M_PI*i/(N-1))  + 0.08*cos(4*M_PI*i/(N-1)));
        w[j] = w[i];
    }

	return N;
}

static double bessel(double x)
{
  	double  xh, sum, pow, ds;
 	int k;

  	xh  = (double)0.5 * x;
  	sum = 1.0;
  	pow = 1.0;
  	k   = 0;
  	ds  = 1.0;
 	while (ds > sum * EPS) {
        ++k;
        pow = pow * (xh / k);
        ds = pow * pow;
        sum = sum + ds;
  	}

 	return sum;	
}

double llz_kaiser_atten2beta(double atten)
{
	double beta;

	if (atten <= 21.)
		beta = 0.;
	else if (atten < 50.)
		beta = (double)0.5842 * (double)pow(atten-21. , 0.4) + (double)0.07886 * (double)(atten-21.);
	else
		beta = (double)0.1102 * (double)(atten - 8.7);

	return beta;
	
}


int llz_kaiser(double *w, const int N)
{
	int   i;
	double Ia,Ib;
    double beta = 8.96;

	for (i = 0 ; i < N ; i++){
		double x;
		
		Ib = bessel(beta);
		
		x = (double)((2. *i/(N-1))-1);
		Ia = bessel(beta*(double)sqrt(1. - x*x));
		
		w[i] = (double)(Ia/Ib);
	}

	return N;
}

int llz_kaiser_beta(double *w, const int N, const double beta)
{
	int i;
	double Ia,Ib;

	for (i = 0 ; i < N ; i++){
		double x;
		
		Ib = bessel(beta);
		
		x = (double)((2. *i/(N-1))-1);
		Ia = bessel(beta*(double)sqrt(1. - x*x));
		
		w[i] = (double)(Ia/Ib);
	}

	return N;
}

/*
 * ftrans: normalized, 1->pi(the max freqency of signal =0.5fs)
 * f = transband/fs && ftrans = transband/(0.5*fs)
 *   => f = 0.5ftrans;
 *Hamming: N = 3.1/f = 6.2/ftrans (f is normalize by fs, 1->2*pi=fs)
 *         passband ripple              :  0.0194dB
 *         mainlobe relative to sidelobe:  41dB
 *         stopband atten               :  53dB
 *Blackman:N = 3.3/f = 6.6/ftrans 
 *         passband ripple              :  0.0017dB
 *         mainlobe relative to sidelobe:  57dB
 *         stopband atten               :  75dB
 */
int llz_hamming_cof_num(double ftrans)
{
    return (int)(6.2/ftrans);
}

int llz_blackman_cof_num(double ftrans)
{
    return (int)(6.6/ftrans);
}

int llz_kaiser_cof_num(double ftrans, double atten)
{
    int cof_num;

	if (atten <= 21.)
		cof_num = (int)((0.9222*2.)/ftrans);
	else
		cof_num = (int)(((atten - 7.95)*2.)/(14.36*ftrans));

    return cof_num;
}

/*
 * generate the fir filter cofficients for the given N(number of the fir length)
 * and the fc(cutoff normalized frequency , pi<->1). e.g fs=44100Hz, we need to 
 * suppress the freqency higher than 11025, we can set fc=0.5(fm=0.5*44100=22050,
 * 11025 if half of fm<pi>, so we set to 0.5)
 */
static int fir_lowpass(double *h, double *w, int N, double fc)
{
	int i,j;
    double delay; /*if N is even, the delay is not integer*/
	
    delay = (double)(N - 1)/2;

    for (i = 0, j = N-1; i <= delay; i++,j--) {
        h[i] = (double)fc*sinc(fc*(i-delay))* w[i];
        h[j] = h[i];
    }
   
	return N;

}

static int fir_highpass(double *h, double *w, int N, double fc)
{
	int i,j;
    int delay;

    assert(N&1);
    delay = (N - 1)/2;

    for (i = 0, j = N-1; i <= delay; i++,j--) {
        h[i] = -(double)fc*sinc(fc*(i-delay))* w[i];
        h[j] = h[i];
    }
    h[delay] = 1-fc;
    
	return N;

}

static int fir_bandpass(double *h, double *w, int N, double fc1, double fc2)
{
	int i,j;
    int delay;

    assert(N&1);
    delay = (N - 1)/2;

    for (i = 0, j = N-1; i <= delay; i++,j--) {
        h[i] = ((double)fc2*sinc(fc2*(i-delay))-(double)fc1*sinc(fc1*(i-delay)))* w[i];
        h[j] = h[i];
    }
    h[delay] = fc2-fc1;
   
	return N;

}

static int fir_bandstop(double *h, double *w, int N, double fc1, double fc2)
{
	int i,j;
    int delay;

    assert(N&1);
    delay = (N - 1)/2;

    for (i = 0, j = N-1; i <= delay; i++,j--) {
        h[i] = -((double)fc2*sinc(fc2*(i-delay))-(double)fc1*sinc(fc1*(i-delay)))* w[i];
        h[j] = h[i];
    }
    h[delay] = 1-(fc2-fc1);
    
	return N;

}

int llz_fir_lpf_cof(double **h, int N, double fc, win_t win_type)
{
	double *w;
	double *hcof;


	w = (double *)malloc(sizeof(double)*N);
    switch (win_type) {
        case HAMMING:
            llz_hamming(w, N);
            break;
        case BLACKMAN:
            llz_blackman(w, N);
            break;
        case KAISER:
            llz_kaiser(w, N);
            break;
    }
	*h = (double *)malloc(sizeof(double)*N);
	hcof = *h;

    fir_lowpass(hcof, w, N, fc);

    free(w);

    return N;
}

int llz_fir_hpf_cof(double **h, int N, double fc, win_t win_type)
{
	double *w;
	double *hcof;
    int odd;

    odd = N&1;
    if (!odd)
        N = N+1;

	w = (double *)malloc(sizeof(double)*N);
    switch (win_type) {
        case HAMMING:
            llz_hamming(w, N);
            break;
        case BLACKMAN:
            llz_blackman(w, N);
            break;
        case KAISER:
            llz_kaiser(w, N);
            break;
    }
	*h = (double *)malloc(sizeof(double)*N);
	hcof = *h;

    fir_highpass(hcof, w, N, fc);

    free(w);

    return N;
}

int llz_fir_bandpass_cof(double **h, int N, double fc1, double fc2, win_t win_type)
{
	double *w;
	double *hcof;
    int odd;

    odd = N&1;
    if (!odd)
        N = N+1;

	w = (double *)malloc(sizeof(double)*N);
    switch (win_type) {
        case HAMMING:
            llz_hamming(w, N);
            break;
        case BLACKMAN:
            llz_blackman(w, N);
            break;
        case KAISER:
            llz_kaiser(w, N);
            break;
    }
	*h = (double *)malloc(sizeof(double)*N);
	hcof = *h;

    fir_bandpass(hcof, w, N, fc1, fc2);

    free(w);

    return N;
}

int llz_fir_bandstop_cof(double **h, int N, double fc1, double fc2, win_t win_type)
{
	double *w;
	double *hcof;
    int odd;

    odd = N&1;
    if (!odd)
        N = N+1;

	w = (double *)malloc(sizeof(double)*N);
    switch (win_type) {
        case HAMMING:
            llz_hamming(w, N);
            break;
        case BLACKMAN:
            llz_blackman(w, N);
            break;
        case KAISER:
            llz_kaiser(w, N);
            break;
    }
	*h = (double *)malloc(sizeof(double)*N);
	hcof = *h;

    fir_bandstop(hcof, w, N, fc1, fc2);

    free(w);

    return N;
}

/*
 * compute nth input x(n) convolution with h
 * return the nth convolution result y
 *
 *  x                     xp
 *  |                     |
 *-x(n)-x(n-1)-x(n-2)-...-x(n-h_len+1)
 *  |    |      |     ... |
 * h(0)-h(1)----h(2)--... h(h_len-1)------>=y
 *
 *double *x: point to x(n-1)
 *      xp: x - (h_len-1), point to x(0)
 *    e.g : i=0,j=h_len-1   y+=h[0]*x[0]=h(0)*x(n-1)
 *          i=1,j=h_len-2   y+=h[1]*x[1]=h(1)*x(n-2)
 *          ...
 */
double llz_conv(const double *x, const double h[], int h_len)
{
    int i,j;
    double y;
    const double *xp;

    /* Convolution with filter */
    /* x->x[n], so xp->x[0]  */
    y = 0.0;
    xp = x - (h_len-1);
        
    for (i = 0, j = h_len-1; i < h_len; i++, j--)
        y += h[i] * xp[j]; // y[n] += h[i]*xp[n-i]

    return y;
}


/*  |---------------------process buf len------------------------|
 *                   px ---> moving from x0 to xn, generats n outputs
 *                    |
 *                   x0 x1 x2 x3        ...                    xn
 *  |----old data---|  |---------------frame_len-1---------------|
 *  |-----flt_len------|
 *   hm  ...  h2 h1  h0 ---> moving sync with px
 *                  |--|(the first sample of the frame)
 *
 *   so the h0 align to x(n)     [n from 0 to n]
 *          h1 align to x(n-1)              
 *   y[n] = h[0]*x[n] + h[1]*x[n-1] + ... + h[flt_len-1]*x[n - flt_len+1]
 */
uintptr_t llz_fir_filter_lpf_init(int frame_len, 
                                 int flt_len, double fc, win_t win_type)
{   
    llz_fir_filter_t *flt = NULL;

    flt = (llz_fir_filter_t *)malloc(sizeof(llz_fir_filter_t));
    memset(flt, 0, sizeof(llz_fir_filter_t));

	flt->fc = fc;
    flt->fc1 = 0;
    flt->fc2 = 0;

    flt->flt_len = llz_fir_lpf_cof(&(flt->h), flt_len, fc, win_type);
    flt->frame_len = frame_len;
    
    flt->buf_len = flt->flt_len + flt->frame_len -1;    //align at the x[0]
	flt->buf = (double *)malloc(sizeof(double)*flt->buf_len);
	memset(flt->buf,0,sizeof(double)*flt->buf_len);		//very important in the first time of convolution

    return (uintptr_t)flt;
}

uintptr_t llz_fir_filter_hpf_init(int frame_len, 
                                 int flt_len, double fc, win_t win_type)
{   
    llz_fir_filter_t *flt = NULL;

    flt = (llz_fir_filter_t *)malloc(sizeof(llz_fir_filter_t));
    memset(flt, 0, sizeof(llz_fir_filter_t));

	flt->fc = fc;
    flt->fc1 = 0;
    flt->fc2 = 0;

    flt->flt_len = llz_fir_hpf_cof(&(flt->h), flt_len, fc, win_type);
    flt->frame_len = frame_len;
    
    flt->buf_len = flt->flt_len + flt->frame_len -1;
	flt->buf = (double *)malloc(sizeof(double)*flt->buf_len);
	memset(flt->buf,0,sizeof(double)*flt->buf_len);		//very important in the first time of convolution

    return (uintptr_t)flt;

}

uintptr_t llz_fir_filter_bandpass_init(int frame_len, 
                                      int flt_len, double fc1, double fc2, win_t win_type)
{   
    llz_fir_filter_t *flt = NULL;

    flt = (llz_fir_filter_t *)malloc(sizeof(llz_fir_filter_t));
    memset(flt, 0, sizeof(llz_fir_filter_t));

	flt->fc = 0;
    flt->fc1 = fc1;
    flt->fc2 = fc2;

    flt->flt_len = llz_fir_bandpass_cof(&(flt->h), flt_len, fc1, fc2, win_type);
    flt->frame_len = frame_len;
    
    flt->buf_len = flt->flt_len + flt->frame_len -1;
	flt->buf = (double *)malloc(sizeof(double)*flt->buf_len);
	memset(flt->buf,0,sizeof(double)*flt->buf_len);		//very important in the first time of convolution

    return (uintptr_t)flt;
}

uintptr_t llz_fir_filter_bandstop_init(int frame_len, 
                                      int flt_len, double fc1, double fc2, win_t win_type)
{   
    llz_fir_filter_t *flt = NULL;

    flt = (llz_fir_filter_t *)malloc(sizeof(llz_fir_filter_t));
    memset(flt, 0, sizeof(llz_fir_filter_t));

	flt->fc = 0;
    flt->fc1 = fc1;
    flt->fc2 = fc2;

    flt->flt_len = llz_fir_bandstop_cof(&(flt->h), flt_len, fc1, fc2, win_type);
    flt->frame_len = frame_len;
    
    flt->buf_len = flt->flt_len + flt->frame_len -1;
	flt->buf = (double *)malloc(sizeof(double)*flt->buf_len);
	memset(flt->buf,0,sizeof(double)*flt->buf_len);		//very important in the first time of convolution

    return (uintptr_t)flt;

}

void llz_fir_filter_uninit(uintptr_t handle)
{
    llz_fir_filter_t *flt = (llz_fir_filter_t *)handle;

	free(flt->h);
    flt->h = NULL;
	free(flt->buf);
    flt->buf = NULL;

    free(flt);
    flt = NULL;
}



int llz_fir_filter(uintptr_t handle, double *buf_in, double *buf_out, int frame_len)
{
	int i;
    llz_fir_filter_t *flt = (llz_fir_filter_t *)handle;
	
	int   flt_len = flt->flt_len;
    int   buf_len = flt->buf_len;
	double *buf    = flt->buf;
	double *h      = flt->h;
    double *xp;
    int   offset;

    assert(frame_len <= flt->frame_len);

    /*move the old datas (flt_len-1) to the buf[0]*/
    offset = buf_len - flt_len + 1;
    for (i = 0; i < flt_len-1; i++) 
        buf[i] = buf[offset+i];
    for (i = 0; i < frame_len; i++)
        buf[flt_len-1+i] = buf_in[i];

    /*set the xp point to the first sample*/
    xp = &(buf[flt_len -1]);
	for (i = 0 ; i < frame_len ; ++i){
        /*
         *  x[0] x[1] x[2] ... x[n-1] x[n]
         *                 ... z(-1)  z(0)  z-transform
         *  old  <-----------------   new
         *  use conv, input x(n) address, h(0) address
         *  compute y += h(k)*x(n-k)  (k=0,1.. h_len-1)
         */
        buf_out[i] = llz_conv(xp, h, flt_len);
        xp++;
	}

    return frame_len;

}

/*
 * we will flush the last flt_len-1 datas out
 * so the total data of the outputs is (x_len+flt_len-1)=frame_len*frame_cnt+flt_len-1
 */
int llz_fir_filter_flush(uintptr_t handle, double *buf_out)
{
	int i;
    llz_fir_filter_t *flt = (llz_fir_filter_t *)handle;
	
	int   flt_len = flt->flt_len;
    int   buf_len = flt->buf_len;
	double *buf    = flt->buf;
	double *h      = flt->h;
    double *xp;
    int   offset;
    int   frame_len = flt->frame_len;


    /*move the old datas (flt_len-1) to the buf[0]*/
    offset = buf_len - flt_len + 1;
    for (i = 0; i < flt_len-1; i++) 
        buf[i] = buf[offset+i];
    for (i = 0; i < frame_len; i++)
        buf[flt_len-1+i] = 0;

    xp = &(buf[flt_len -1]);
	for (i = 0 ; i < flt_len-1 ; ++i){
        /*
         *  x[0] x[1] x[2] ... x[n-1] x[n]
         *                 ... z(-1)  z(0)  z-transform
         *  old  <-----------------   new
         *  use conv, input x(n) address, h(0) address
         *  compute y += h(k)*x(n-k)  (k=0,1.. h_len-1)
         */
        buf_out[i] = llz_conv(xp, h, flt_len);
        xp++;
	}

    return flt_len-1;
}

