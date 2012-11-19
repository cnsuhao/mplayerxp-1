#ifdef TEST
#include <stdio.h>
#undef printf
main(){
    uint64_t md5val;
    int i;
    uint8_t in[1000];

    for(i=0; i<1000; i++) in[i]= i*i;
    av_md5_sum( (uint8_t*)&md5val, in,  1000); printf("%"PRId64"\n", md5val);
    av_md5_sum( (uint8_t*)&md5val, in,  63); printf("%"PRId64"\n", md5val);
    av_md5_sum( (uint8_t*)&md5val, in,  64); printf("%"PRId64"\n", md5val);
    av_md5_sum( (uint8_t*)&md5val, in,  65); printf("%"PRId64"\n", md5val);
    for(i=0; i<1000; i++) in[i]= i % 127;
    av_md5_sum( (uint8_t*)&md5val, in,  999); printf("%"PRId64"\n", md5val);
}
#endif
