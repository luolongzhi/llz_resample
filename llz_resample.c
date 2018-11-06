/*
  llzlab - luolongzhi algorithm lab 
  Copyright (C) 2013 luolongzhi 罗龙智 (Chengdu, China)

  This program is part of llzlab, all copyrights are reserved by luolongzhi. 

  filename: llz_resample.c 
  time    : 2012/07/12 20:42 
  author  : luolongzhi ( luolongzhi@gmail.com )

  Note    : this source code is written by the reference article 
            [
              Interploation and Decimation of Digital Signals - A Tutorial Review
                 PROCEEDINGS OF THE IEEE, VOL.69, NO.3, MARCH 1981        
                 RONALD E.CROCHIERE, senior member, IEEE, AND LAWRENCE R.RABINER, fellow, IEEE
            ]

*/


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include "llz_fir.h"
#include "llz_resample.h"

#define MAXV(a, b)	(((a) > (b)) ? (a) : (b))
#define MINV(a, b)	(((a) < (b)) ? (a) : (b))

typedef	struct _llz_polyphase_filter_t {
    double *h;           /* the proto-type lpf confficients */
    double **p;          /* the ployphase cofficients matrix */
    int n;              /* the total number of the proto-type lpf cofficients */
    int m;              /* the number of the phase */
    int k;              /* the number of the subfilter cofficients, k = n/m + 1*/

    double gain;

}llz_polyphase_filter_t;

typedef struct _llz_timevary_filter_t {
    double *h;           /* the proto-type lpf confficients */
    double **g;          /* the ployphase cofficients matrix */
    int n;              /* N ,the total number of the proto-type lpf cofficients */
    int l;              /* L ,the number of the phase */
    int m;              /* M ,the deciminator of fractional ratio */
    int k;              /* Q ,the number of the subfilter cofficients, k = n/m + 1*/

    double gain;
    
}llz_timevary_filter_t;

typedef struct _llz_resample_filter_t {
    int L;                  /* interp factor */
    int M;                  /* decimate factor */
    double fc;               /* cutoff freqency, normalized */

    llz_polyphase_filter_t ppflt;
    llz_timevary_filter_t  tvflt;
    double gain;

    int bytes_per_sample;
    int num_in;             /* number of sample in */
    int num_out;            /* number of sample out */
    /*int out_index;*/
#ifdef WIN32
    __int64 out_index;
#else
    long long out_index;
#endif
    int bytes_in;
    int bytes_out;

    int buf_len;
    unsigned char *buf;
       
}llz_resample_filter_t;



static int gcd ( int i, int j )
{
    return j ? gcd(j, i % j) : i;
}


static double ** matrix_init(int Nrow, int Ncol)
{
    double **A;
    int i;

    /* Allocate the row pointers */
    A = (double **) malloc (Nrow * sizeof (double *));

    /* Allocate the matrix of double values */
    A[0] = (double *) malloc (Nrow * Ncol * sizeof (double));
    memset(A[0], 0, Nrow*Ncol*sizeof(double));

    /* Set up the pointers to the rows */
    for (i = 1; i < Nrow; ++i)
        A[i] = A[i-1] + Ncol;

    return A;
}

/*static void matrix_uninit(double **A, int Nrow, int Ncol)*/
static void matrix_uninit(double **A)
{
    /*free the Nrow*Ncol data space*/
    if (A[0])
        free(A[0]);

    /*free the row pointers*/
    if (A)
        free(A);

}

/*
 * m: the number of phase needed
 * fc: the proto-type lpf cofficients number
 */
static int polyphase_filter_init(llz_polyphase_filter_t *ppflt, int m, 
                                 double fc, double gain, win_t win_type)
{
    int i, j;
    int n, k;
    double ftrans;

    if (gain == 0)
        gain = 1.0;
    
    ftrans = 0.15*fc;           /*common sense: transition band = 0.15*pi (pi~fm=0.5fs)*/
    switch (win_type) {
        case HAMMING:
           n = llz_hamming_cof_num(ftrans);
           break;
        case BLACKMAN:
           n = llz_blackman_cof_num(ftrans);
           break;
        case KAISER:
           n = llz_kaiser_cof_num(ftrans, 90);         /*the estimate number of cofficients needed*/
           break;
    }

    /*it is better to set odd number of the proto-type lpf*/
    k = n/(2*m);
    ppflt->n = 2*k*m + 1;
    ppflt->m = m;
    ppflt->k = ppflt->n/ppflt->m + 1;
    ppflt->p = (double **)matrix_init(ppflt->m, ppflt->k);
    ppflt->gain = gain;

    switch (win_type) {
        case HAMMING:
            llz_fir_lpf_cof(&(ppflt->h), ppflt->n, fc, HAMMING);
            break;
        case BLACKMAN:
            llz_fir_lpf_cof(&(ppflt->h), ppflt->n, fc, BLACKMAN);
            break;
        case KAISER:
            llz_fir_lpf_cof(&(ppflt->h), ppflt->n, fc, KAISER);
            break;
    }
    
    for (i = 0; i < ppflt->m; i++)
        for (j = 0; j < ppflt->k; j++) {
            if (ppflt->m * j + i < ppflt->n)
                ppflt->p[i][j] = gain * ppflt->h[ppflt->m * j + i];
            else
                ppflt->p[i][j] = 0;
        }

    return 0;
}

static int polyphase_filter_uninit(llz_polyphase_filter_t *ppflt)
{
    /*matrix_uninit(ppflt->p, ppflt->m, ppflt->k);*/
    matrix_uninit(ppflt->p);
    ppflt->p = NULL;
    
    if (ppflt->h) {
        free(ppflt->h);
        ppflt->h = NULL;
    }

    return 0;
}


static int timevary_filter_init(llz_timevary_filter_t *tvflt, int l, int m,
                                double fc, double gain, win_t win_type)
{
    int i, j;
    int n, k;
    int u;
    double ftrans;

    if (gain == 0)
        gain = 1.0;
    
    ftrans = 0.15*fc;           /*common sense: transition band = 0.15*pi (pi~fm=0.5fs)*/
    switch (win_type) {
        case HAMMING:
           n = llz_hamming_cof_num(ftrans);
           break;
        case BLACKMAN:
           n = llz_blackman_cof_num(ftrans);
           break;
        case KAISER:
           n = llz_kaiser_cof_num(ftrans, 90);         /*the estimate number of cofficients needed*/
           break;
    }

    /*it is better to set odd number of the proto-type lpf*/
    k = n/(2*l);
    tvflt->n = 2*k*l + 1;
    tvflt->l = l;
    tvflt->m = m;
    tvflt->k = tvflt->n/tvflt->l + 1;
    tvflt->g = (double **)matrix_init(tvflt->l, tvflt->k);
    tvflt->gain = gain;

    switch (win_type) {
        case HAMMING:
            llz_fir_lpf_cof(&(tvflt->h), tvflt->n, fc, HAMMING);
            break;
        case BLACKMAN:
            llz_fir_lpf_cof(&(tvflt->h), tvflt->n, fc, BLACKMAN);
            break;
        case KAISER:
            llz_fir_lpf_cof(&(tvflt->h), tvflt->n, fc, KAISER);
            break;
    }
    
    for (i = 0; i < tvflt->l; i++)
        for (j = 0; j < tvflt->k; j++) {
            /*
             * gl(n)= h(nL + <lM>L)
             * gl(n): g(i)(j), i is l(l phase), j is n(the l pahse subfilter cofficients)
             * nL:    j is n, tvflt->l is L, j*tvflt->l
             * <lM>L: i is l, tvflt->m is M, (i*tvflt->m)%tvflt->l
             */
            u = j*tvflt->l + (i*tvflt->m)%tvflt->l;
//            u = j*tvflt->l + i; //you can try this, you will find phase err(study this)
            if (u < tvflt->n)
                tvflt->g[i][j] = gain * tvflt->h[u];
            else
                tvflt->g[i][j] = 0;
        }

    return 0;
}

static int timevary_filter_uninit(llz_timevary_filter_t *tvflt)
{
    /*matrix_uninit(tvflt->g, tvflt->l, tvflt->k);*/
    matrix_uninit(tvflt->g);
    tvflt->g = NULL;
    
    if (tvflt->h) {
        free(tvflt->h);
        tvflt->h = NULL;
    }

    return 0;
}

uintptr_t llz_decimate_init(int M, double gain, win_t win_type)
{
    int m;
    llz_resample_filter_t *resflt = NULL;

    resflt = (llz_resample_filter_t *)malloc(sizeof(llz_resample_filter_t));

    if (M > LLZ_RATIO_MAX) 
        return -1;

    resflt->L = 1;
    resflt->M = M;
    resflt->fc = 1./M;
    resflt->gain = gain;

    resflt->bytes_per_sample = 2;       /*default we set to 2 bytes per sample */
    
    
    polyphase_filter_init(&resflt->ppflt, M, resflt->fc, 1, win_type);
    
    m = LLZ_DEFAULT_FRAMELEN/M;
    resflt->num_in = m*M;
    resflt->num_out = m;
    resflt->bytes_in = resflt->bytes_per_sample * resflt->num_in;
    resflt->bytes_out = resflt->bytes_per_sample * resflt->num_out;

    resflt->buf_len = (resflt->ppflt.n + resflt->num_in) * resflt->bytes_per_sample;
    resflt->buf = (unsigned char *)malloc(resflt->buf_len * sizeof(char));
    memset(resflt->buf, 0, sizeof(char)*resflt->buf_len);

    return (uintptr_t)resflt;        
}

void llz_decimate_uninit(uintptr_t handle)
{
    llz_resample_filter_t *resflt = (llz_resample_filter_t *)handle;

    if (resflt) {
        if (resflt->buf) {
            free(resflt->buf);
            resflt->buf = NULL;
        }
        polyphase_filter_uninit(&(resflt->ppflt));
        
        free(resflt);
        resflt = NULL;
    }
}

uintptr_t llz_interp_init(int L, double gain, win_t win_type)
{
    llz_resample_filter_t *resflt = NULL;

    resflt = (llz_resample_filter_t *)malloc(sizeof(llz_resample_filter_t));

    if (L > LLZ_RATIO_MAX) 
        return -1;

    resflt->L = L;
    resflt->M = 1;
    resflt->fc = 1./L;
    resflt->gain = gain;

    resflt->bytes_per_sample = 2;       /*default we set to 2 bytes per sample */
    
    polyphase_filter_init(&resflt->ppflt, L, resflt->fc, L, win_type);
    
    resflt->num_in = LLZ_DEFAULT_FRAMELEN;
    resflt->num_out = LLZ_DEFAULT_FRAMELEN*L;
    resflt->bytes_in = resflt->bytes_per_sample * resflt->num_in;
    resflt->bytes_out = resflt->bytes_per_sample * resflt->num_out;

    resflt->buf_len = resflt->num_in * resflt->bytes_per_sample;
    resflt->buf = (unsigned char *)malloc(resflt->buf_len * sizeof(char));
    memset(resflt->buf, 0, sizeof(char)*resflt->buf_len);

    return (uintptr_t)resflt;
}
       

void llz_interp_uninit(uintptr_t handle)
{
    llz_resample_filter_t *resflt = (llz_resample_filter_t *)handle;

    if (resflt) {
        if (resflt->buf) {
            free(resflt->buf);
            resflt->buf = NULL;
        }
        polyphase_filter_uninit(&(resflt->ppflt));
        
        free(resflt);
        resflt = NULL;
    }
}

uintptr_t llz_resample_filter_init(int L, int M, double gain, win_t win_type)
{
    llz_resample_filter_t *resflt = NULL;
    int lm_gcd;
    double ratio;

    resflt = (llz_resample_filter_t *)malloc(sizeof(llz_resample_filter_t));

    ratio = ((double)L)/M;
    if ((ratio      > LLZ_RATIO_MAX) || 
       ((1./ratio) > LLZ_RATIO_MAX))
        return -1;

    resflt->L = L;
    resflt->M = M;
    resflt->fc = MINV(1./L, 1./M);
    resflt->gain = gain;

    resflt->bytes_per_sample = 2;       /*default we set to 2 bytes per sample */
    resflt->out_index = 0;

    timevary_filter_init(&(resflt->tvflt), L, M, resflt->fc,  L, win_type);

    lm_gcd = gcd(L, M);
  
    /*L*M/lm_gcd is the lowest multiplier of L&M*/
    /*resflt->num_in = ((L*M)/lm_gcd)*RES_DEFAULT_NUM_IN;*/
    resflt->num_in = ((L*M)/lm_gcd);
    while (resflt->num_in < LLZ_DEFAULT_FRAMELEN)
        resflt->num_in *= 2;

    resflt->num_out = (resflt->num_in*L)/M;
    resflt->bytes_in = resflt->bytes_per_sample * resflt->num_in;
    resflt->bytes_out = resflt->bytes_per_sample * resflt->num_out;

    resflt->buf_len = (resflt->tvflt.k + resflt->num_in ) * resflt->bytes_per_sample;
    resflt->buf = (unsigned char *)malloc(resflt->buf_len * sizeof(char));
    memset(resflt->buf, 0, sizeof(char)*resflt->buf_len);

    return (uintptr_t)resflt;
}

void llz_resample_filter_uninit(uintptr_t handle)
{
    llz_resample_filter_t *resflt = (llz_resample_filter_t *)handle;

    if (resflt) {
        if (resflt->buf) {
            free(resflt->buf);
            resflt->buf = NULL;
        }
        timevary_filter_uninit(&(resflt->tvflt));

        free(resflt);
        resflt = NULL;
    }
}

int llz_decimate(uintptr_t handle, unsigned char *sample_in, int sample_in_size,
                                   unsigned char *sample_out, int *sample_out_size)
{
    llz_resample_filter_t *resflt = (llz_resample_filter_t *)handle;

    int i;
    int m,k;
    int offset;
    int flt_len;
    short *pbuf;
    short *psample_in;
    short *psample_out;
    short *x;
    short *xp;
    double gain = resflt->gain;

    int M;

    assert(sample_in_size == resflt->bytes_in);
    
    flt_len = resflt->ppflt.n;
    M = resflt->M;
    
    pbuf = (short *)resflt->buf;
    psample_in = (short *)sample_in;
    psample_out = (short *)sample_out;
    
    /*move the old datas (flt_len-1) to the buf[0]*/
    offset = resflt->buf_len/resflt->bytes_per_sample - flt_len;
    
    for (i = 0; i < flt_len; i++) 
        pbuf[i] = pbuf[offset+i];
    for (i = 0; i < resflt->num_in; i++)
        pbuf[flt_len+i] = psample_in[i];
    

    /*set the xp point to the first sample*/
    x = &(pbuf[flt_len]);

    for (i = 0; i < resflt->num_out; i++) {
        double y= 0.0;

		for (m = 0 ; m < M; m++){
			xp = x + m;     /* delay */
			for (k = 0 ; k < resflt->ppflt.k; k++)
				y += xp[M*k-flt_len] * resflt->ppflt.p[m][k]; /* decimate by M */
//				y += xp[M*k] * resflt->ppflt.p[m][k]; //maybe right maybe wrong, because of the symmetric of the filter
//				                                      //but I am afraid the subfilter maybe not symmetric, so maybe wrong
		}

        y *= gain;

        if (y > 32767)
            y = 32767;
        if (y < -32768)
            y = -32768;

        psample_out[i] = (short)y;
        x += M;

    }

    *sample_out_size = resflt->bytes_out;

    return 0;

}


int llz_interp(uintptr_t handle, unsigned char *sample_in, int sample_in_size,
                                 unsigned char *sample_out, int *sample_out_size)
{
    llz_resample_filter_t *resflt = (llz_resample_filter_t *)handle;
    int i;
    int m,k;
    short *psample_in;
    short *psample_out;
    short *x;

    int L;
    double gain = resflt->gain;

    assert(sample_in_size == resflt->bytes_in);
    
    L = resflt->L;
    
    psample_in = (short *)sample_in;
    psample_out = (short *)sample_out;

    /*set the xp point to the first sample*/
    x = psample_in;

    for (i = 0; i < resflt->num_in; i++) {
        double y= 0.0;

		for (m = 0 ; m < L; m++){
            y = 0.0;
			for (k = 0 ; k < resflt->ppflt.k; k++)
				y += x[k] * resflt->ppflt.p[m][k];

            y *= gain;

            if (y > 32767)
                y = 32767;
            if (y < -32768)
                y = -32768;
            psample_out[i*L+(L-1-m)] = y;       /* interp by L , place y into L position */
           
		}
        x++;
    }

    *sample_out_size = resflt->bytes_out;

    return 0;

}


int llz_resample(uintptr_t handle, unsigned char *sample_in, int sample_in_size,
                                   unsigned char *sample_out, int *sample_out_size)
{
    llz_resample_filter_t *resflt = (llz_resample_filter_t *)handle;
    int i;
    int l, k;
    int offset;
    short *pbuf;
    short *psample_in;
    short *psample_out;
    short *x;
    short *xp;

    int L, M, Q;
    double gain = resflt->gain;

    assert(sample_in_size == resflt->bytes_in);
    
    L = resflt->L;
    M = resflt->M;
    Q = resflt->tvflt.k;
    
    pbuf = (short *)resflt->buf;
    psample_in = (short *)sample_in;
    psample_out = (short *)sample_out;
    
    /*move the old datas (flt_len-1) to the buf[0]*/
    offset = resflt->buf_len/resflt->bytes_per_sample - Q;
   
    for (i = 0; i < Q; i++) 
        pbuf[i] = pbuf[offset+i];
    for (i = 0; i < resflt->num_in; i++)
        pbuf[Q+i] = psample_in[i];
   

    /*set the xp point to the first sample*/
    x = &(pbuf[Q]);
    *sample_out_size =0;

    for (i = 0; i < resflt->num_out; i++) {
        double y= 0.0;

        xp = x + (i*M)/L;
        l = (int)(resflt->out_index%L); 
        resflt->out_index++;

		for (k = 0 ; k < Q; k++) {
			y += xp[-k] * resflt->tvflt.g[l][k]; /* decimate by M */
        }

        y *= gain;

        if (y > 32767)
            y = 32767;
        if (y < -32768)
            y = -32768;

        psample_out[i] = (short)y;

    }

    *sample_out_size = resflt->bytes_out;

    return 0;

}


int llz_get_resample_framelen_bytes(uintptr_t handle)
{
    llz_resample_filter_t *resflt = (llz_resample_filter_t *)handle;

    return resflt->bytes_in;
}
