/* MP3 Player Library 2.0      (C) 1999 A'rpi/Astral&ESP-team  */

/* decoder level: */
extern void MP3_Init(int fakemono,unsigned accel,int (*audio_read)(char *buf,int size,float *pts),const char *param);
extern int MP3_Open(char *filename,int buffsize);
extern void MP3_SeekFrame(int num,int dir);
extern void MP3_SeekForward(int num);
extern int MP3_PrintTAG();
extern int MP3_DecodeFrame(unsigned char *hova,short single,float *pts);
extern int MP3_FillBuffers();
extern void MP3_PrintHeader();
extern void MP3_Close();
/* public variables: */
extern int MP3_eof;        // set if EOF reached
extern int MP3_pause;      // lock playing
/* informational: */
extern int MP3_filesize;   // filesize
extern int MP3_frames;     // current frame no
extern int MP3_fpos;       // current file pos
extern int MP3_framesize;  // current framesize in bytes (including header)
extern int MP3_bitrate;    // current bitrate (kbits)
extern int MP3_samplerate; // current sampling freq (Hz)
extern int MP3_channels;

/* player level: */
extern int  MP3_OpenDevice(char *devname);  /* devname can be NULL for default) */
extern void MP3_Play();
extern void MP3_Stop();
extern void MP3_CloseDevice();

