/*
  llzlab - luolongzhi algorithm lab 
  Copyright (C) 2013 luolongzhi ÂÞÁúÖÇ (Chengdu, China)

  This program is part of llzlab, all copyrights are reserved by luolongzhi. 

  filename: llz_wavfmt.h 
  time    : 2012/07/08 18:33 
  author  : luolongzhi ( luolongzhi@gmail.com )
*/


#ifndef	_LLZ_WAVFMT_H
#define	_LLZ_WAVFMT_H

#include <stdio.h>

#ifdef __cplusplus 
extern "C"
{ 
#endif  


typedef struct _llz_wavfmt_t
{
	unsigned short  format;

	unsigned short  channels;

	unsigned long   samplerate;

	unsigned short  bytes_per_sample;

	unsigned short  block_align;

	unsigned long   data_size;

}llz_wavfmt_t;


llz_wavfmt_t  llz_wavfmt_readheader (FILE *fp);
void          llz_wavfmt_writeheader(llz_wavfmt_t fmt, FILE *fp);

#ifdef __cplusplus 
}
#endif  



#endif
