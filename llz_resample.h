/*
  llzlab - luolongzhi algorithm lab 
  Copyright (C) 2013 luolongzhi 罗龙智 (Chengdu, China)

  This program is part of llzlab, all copyrights are reserved by luolongzhi. 

  filename: llz_resample.h 
  time    : 2012/07/12 20:42 
  author  : luolongzhi ( luolongzhi@gmail.com )

  Note    : this source code is written by the reference article 
            [
              Interploation and Decimation of Digital Signals - A Tutorial Review
                 PROCEEDINGS OF THE IEEE, VOL.69, NO.3, MARCH 1981        
                 RONALD E.CROCHIERE, senior member, IEEE, AND LAWRENCE R.RABINER, fellow, IEEE
            ]

*/


#ifndef _LLZ_RESAMPLE_H
#define _LLZ_RESAMPLE_H

#include "llz_fir.h"

#ifdef __cplusplus 
extern "C"
{ 
#endif  


#ifndef __MacOSX__
typedef unsigned uintptr_t;
#endif

#define LLZ_DEFAULT_FRAMELEN     1024            /* default num of sample in */
#define LLZ_FRAMELEN_MAX         (160*147+8192)  /* 8192 no meaning, I just set here to safe regarding */

#define LLZ_RATIO_MAX            16

uintptr_t llz_decimate_init(int M, double gain, win_t win_type);
void      llz_decimate_uninit(uintptr_t handle);

uintptr_t llz_interp_init(int L, double gain, win_t win_type);
void      llz_interp_uninit(uintptr_t handle);

uintptr_t llz_resample_filter_init(int L, int M, double gain, win_t win_type);
void      llz_resample_filter_uninit(uintptr_t handle);

int llz_get_resample_framelen_bytes(uintptr_t handle);
int llz_decimate(uintptr_t handle, unsigned char *sample_in, int sample_in_size,
                                  unsigned char *sample_out, int *sample_out_size);
int llz_interp(uintptr_t handle, unsigned char *sample_in, int sample_in_size,
                                unsigned char *sample_out, int *sample_out_size);
int llz_resample(uintptr_t handle, unsigned char *sample_in, int sample_in_size,
                                  unsigned char *sample_out, int *sample_out_size);


#ifdef __cplusplus 
}
#endif  


#endif
