#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 *  my_profile.c
 *
 *	Copyright (C) Nickols_K <nickols_k@mail.ru> - Oct 2001
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 */
#include <stdlib.h>
#include <string.h>

namespace	usr {

volatile unsigned long long int my_profile_start,my_profile_end,my_profile_total;

any_t* rnd_fill(any_t* buffer,size_t size)
{
    unsigned i;
    char ch;
    for(i=0;i<size;i++) {
	ch=::rand()%255;
	((char *)buffer)[i]=ch;
    }
    return buffer;
}

any_t* make_false_pointer(any_t* tmplt) {
    long lo_mask=(sizeof(any_t*)*8/2)-1;
    long hi_mask=~lo_mask;
    long false_pointer;
    false_pointer=::rand()&lo_mask;
    false_pointer|=(reinterpret_cast<long>(tmplt)&hi_mask);
    return reinterpret_cast<any_t*>(false_pointer);
}

any_t*	__FASTCALL__ make_false_pointer_to(any_t* tmplt,unsigned size) {
    long false_pointer=reinterpret_cast<long>(tmplt);
    false_pointer+=::rand()%size;
    return reinterpret_cast<any_t*>(false_pointer);
}

any_t* fill_false_pointers(any_t* buffer,size_t size)
{
    unsigned i,psize=(size/sizeof(any_t*))*sizeof(any_t*);
    any_t* filler;
    for(i=0;i<psize/sizeof(long);i++) {
	filler=make_false_pointer(buffer);
	((long *)buffer)[i]=::rand()%2?reinterpret_cast<long>(filler):0;
    }
    ::memset(&((char *)buffer)[psize],0,size-psize);
    return buffer;
}

any_t* get_caller_address(unsigned num_caller) {
    any_t*	stack[3+num_caller];
    ::backtrace(stack,3+num_caller);
    return stack[2+num_caller];
}

Opaque::Opaque() {
    fill_false_pointers(&false_pointers,reinterpret_cast<long>(&unusable)-reinterpret_cast<long>(&false_pointers));
    fill_false_pointers(&unusable,sizeof(any_t*));
}

Opaque::~Opaque() {}
} // namespace	usr
