#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 *  my_profile.c
 *
 *	Copyright (C) Nickols_K <nickols_k@mail.ru> - Oct 2001
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 */
#include <stdlib.h>

namespace mpxp {

volatile unsigned long long int my_profile_start,my_profile_end,my_profile_total;

any_t* rnd_fill(any_t* buffer,size_t size)
{
    unsigned i;
    char ch;
    for(i=0;i<size;i++) {
	ch=rand()%255;
	((char *)buffer)[i]=ch;
    }
    return buffer;
}

any_t* fill_false_pointers(any_t* buffer,size_t size)
{
    unsigned i,psize=(size/sizeof(any_t*))*sizeof(any_t*);
    long lo_mask=(sizeof(any_t*)*8/2)-1;
    long hi_mask=~lo_mask;
    long filler;
    for(i=0;i<psize/sizeof(long);i++) {
	filler=rand()&lo_mask;
	filler=(reinterpret_cast<long>(buffer)&hi_mask)|lo_mask;
	((long *)buffer)[i]=filler;
    }
    char ch;
    for(i=psize;i<size;i++) {
	ch=rand()%255;
	((char *)buffer)[i]=ch;
    }
    return buffer;
}

any_t* get_caller_address(unsigned num_caller) {
    any_t*	stack[3+num_caller];
    backtrace(stack,3+num_caller);
    return stack[2+num_caller];
}

} // namespace mpxp
