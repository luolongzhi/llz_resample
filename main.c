/*
  llzlab - luolongzhi algorithm lab 
  Copyright (C) 2013 luolongzhi 罗龙智 (Chengdu, China)

  This program is part of llzlab, all copyrights are reserved by luolongzhi. 

  filename: main.c 
  time    : 2012/07/08 18:33 
  author  : luolongzhi ( luolongzhi@gmail.com )
*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include "llz_wavfmt.h"
#include "llz_parseopt.h"
#include "llz_fir.h"
#include "llz_resample.h"

int main(int argc, char *argv[])
{
    int ret;
    int frame_index = 0;

	FILE  * destfile;
	FILE  * sourcefile;
	llz_wavfmt_t fmt;
    int samplerate_in;
    int samplerate_out;

    int is_last          = 0;
    int in_len_bytes     = 0;
    int out_len_bytes    = 0;
    int read_len         = 0;
    int write_total_size = 0;

    uintptr_t h_resflt;

	short wavsamples_in[LLZ_FRAMELEN_MAX];
	short wavsamples_out[LLZ_FRAMELEN_MAX];
    unsigned char * p_wavin  = (unsigned char *)wavsamples_in;
    unsigned char * p_wavout = (unsigned char *)wavsamples_out;


    ret = llz_parseopt(argc, argv);
    if(ret) return -1;

    if ((destfile = fopen(opt_outputfile, "w+b")) == NULL) {
		printf("output file can not be opened\n");
		return 0; 
	}                         

	if ((sourcefile = fopen(opt_inputfile, "rb")) == NULL) {
		printf("input file can not be opened;\n");
		return 0; 
    }

    fmt = llz_wavfmt_readheader(sourcefile);
    samplerate_in = fmt.samplerate;
    fseek(sourcefile,44,0);

    /*init filter*/
    switch(opt_type) {
        case LLZ_DECIMATE:
            h_resflt = llz_decimate_init(opt_downfactor, opt_gain, BLACKMAN);
            samplerate_out = samplerate_in / opt_downfactor;  
            break;
        case LLZ_INTERP:
            h_resflt = llz_interp_init(opt_upfactor, opt_gain, BLACKMAN);
            samplerate_out = samplerate_in * opt_upfactor;
            break;
        case LLZ_RESAMPLE:
            h_resflt = llz_resample_filter_init(opt_upfactor, opt_downfactor, opt_gain, BLACKMAN);
            samplerate_out = (samplerate_in * opt_upfactor)/opt_downfactor;
            break;
    }

    if(ret) {
        printf(" not support! ");
        return -1;
    }

    fseek(destfile  ,  0, SEEK_SET);
    fmt.samplerate = samplerate_out;
    llz_wavfmt_writeheader(fmt, destfile);

    in_len_bytes = llz_get_resample_framelen_bytes(h_resflt);

    while(1)
    {
        if(is_last)
            break;

        memset(p_wavin, 0, in_len_bytes);
        read_len = fread(p_wavin, 1, in_len_bytes, sourcefile);
        if(read_len < in_len_bytes)
            is_last = 1;

        switch(opt_type) {
            case LLZ_DECIMATE:
                llz_decimate(h_resflt, p_wavin, in_len_bytes, p_wavout, &out_len_bytes);
                break;
            case LLZ_INTERP:
                llz_interp(h_resflt, p_wavin, in_len_bytes, p_wavout, &out_len_bytes);
                break;
            case LLZ_RESAMPLE:
                llz_resample(h_resflt, p_wavin, in_len_bytes, p_wavout, &out_len_bytes);
                break;
        }


        fwrite(p_wavout, 1, out_len_bytes, destfile);
        write_total_size += out_len_bytes;

        frame_index++;
        printf("the frame = [%d]\r", frame_index);
    }

    fmt.data_size = write_total_size/fmt.block_align;
    fseek(destfile, 0, SEEK_SET);
    llz_wavfmt_writeheader(fmt,destfile);

    llz_resample_filter_uninit(h_resflt);
    fclose(sourcefile);
    fclose(destfile);

    return 0;
}
