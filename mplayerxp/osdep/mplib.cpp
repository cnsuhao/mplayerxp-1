/*
 *  my_profile.c
 *
 *	Copyright (C) Nickols_K <nickols_k@mail.ru> - Oct 2001
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 */
#include <stdlib.h>
#include "mp_config.h"
#include "mplib.h"

volatile unsigned long long int my_profile_start,my_profile_end,my_profile_total;

any_t* SECURE_NAME9(rnd_fill)(any_t* buffer,size_t size)
{
    unsigned i;
    char ch;
    for(i=0;i<size;i++) {
	ch=rand()%255;
	((char *)buffer)[i]=ch;
    }
    return buffer;
}
