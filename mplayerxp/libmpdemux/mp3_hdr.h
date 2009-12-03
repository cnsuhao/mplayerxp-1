
int mp_decode_mp3_header(unsigned char* hbuf,int *fmt,int *brate,int *samplerate,int *channels);

static inline int mp_check_mp3_header(unsigned int head,int *fmt,int *brate,int *samplerate,int *channels){
    if( (head & 0x0000e0ff) != 0x0000e0ff ||  
        (head & 0x00fc0000) == 0x00fc0000) return 0;
    if(mp_decode_mp3_header((unsigned char*)(&head),fmt,brate,samplerate,channels)<=0) return 0;
    return 1;
}
