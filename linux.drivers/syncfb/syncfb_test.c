/*
 *
 * syncfb_test.c
 *
 *
 * This software has been released under the terms of the GNU Public
 * license. See http://www.gnu.org/copyleft/gpl.html for details.
 */
#define HAVE_X 1

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <X11/Xlib.h>
#include <linux/videodev.h>
#include <linux/fb.h>
#include "syncfb.h"

struct fb_con2fbmap c2f;

syncfb_config_t config;
syncfb_capability_t caps;

syncfb_buffer_info_t sbuf;

uint_8 *mga_vid_base;
uint_32 is_g400;


#define SRC_IMAGE_WIDTH 256
#define SRC_IMAGE_HEIGHT 256

uint_8 y_image[SRC_IMAGE_WIDTH * SRC_IMAGE_HEIGHT];
uint_8 cr_image[SRC_IMAGE_WIDTH * SRC_IMAGE_HEIGHT];
uint_8 cb_image[SRC_IMAGE_WIDTH * SRC_IMAGE_HEIGHT];


void
write_frame_YUV422(uint_8 *y,uint_8 *cr, uint_8 *cb, uint_32 offs)
{
	uint_8 *dest;
	uint_32 bespitch,h,w;

	dest = mga_vid_base + offs;
	bespitch = (config.src_width + 31) & ~31;


	for(h=0; h < config.src_height; h++) 
	{
		for(w=0; w < config.src_width/2; w++) 
		{
			*dest++ = *y++;
			*dest++ = *cb++;
			*dest++ = *y++;
			*dest++ = *cr++;
		}
		dest += (config.src_pitch - config.src_width) * 2 ;
	}
}




void
draw_cool_pattern(void)
{
	int i,x,y;

	i = 0;
	for (y=0; y<config.src_height; y++) {
		for (x=0; x<config.src_width; x++) {
			y_image[i++] = x*x/2 + y*y/2 - 128;
		}
	}

	i = 0;
	for (y=0; y<config.src_height; y++) 
		for (x=0; x<config.src_width/2; x++) 
		{
				//cr_image[i++] = x - 128;
				cr_image[i++] = 0x80;
		}

	i = 0;
	for (y=0; y<config.src_height; y++) 
		for (x=0; x<config.src_width/2; x++) 
		{
				//cb_image[i++] = y - 128;
				cb_image[i++] = 0x80;
		}
}

void
draw_color_blend(void)
{
	int i,x,y;

	i = 0;
	for (y=0; y<config.src_height; y++) {
		for (x=0; x<config.src_width; x++) {
			y_image[i++] = 0;
		}
	}

	i = 0;
	for (y=0; y<config.src_height; y++) 
		for (x=0; x<config.src_width/2; x++) 
		{
				cr_image[i++] = x - 128;
		}

	i = 0;
	for (y=0; y<config.src_height; y++) 
		for (x=0; x<config.src_width/2; x++) 
		{
				cb_image[i++] = y/2 - 128;
		}
}


void request_buf(int f) {

	if (ioctl(f,SYNCFB_REQUEST_BUFFER,&sbuf))
	{
		perror("Error in get_config ioctl");
	}
	// printf( "Got buffer #%d with offset %lx\n", sbuf.id, sbuf.offset);
}

void commit_buf(int f) {

	if (ioctl(f,SYNCFB_COMMIT_BUFFER,&sbuf))
	{
		perror("Error in get_config ioctl");
	}
	// printf( "Committed buffer #%d\n", sbuf.id);
}



int main(int argc, char *argv[])
{

	int sf;
	int fb = 0;
	int i, tt;
	char *display_name = NULL;

	char *ttyn;


	struct fb_fix_screeninfo fbfix;
	struct fb_var_screeninfo fbvar;

	int disp_width   = 640;
	int disp_height  = 480;
	int disp_vwidth  = 640;
	int disp_vheight = 480;
	int disp_bpp     = 8;

	int x_running = -1; // test

	
#ifdef HAVE_X
	Display *xDisplay = NULL;
	Screen *xScreen = NULL;
#endif

	/* open the syncfb device */
	sf = open("/dev/syncfb",O_RDWR);
	if(sf == -1) {
		fprintf(stderr,"Cannot open /dev/syncfb\n");
		exit(1);
	}

	/* get a list of capabilities, see syncfb.h for more */
	if (ioctl(sf,SYNCFB_GET_CAPS,&caps)) perror("Error in SYNCFB_GET_CAPS ioctl");
	printf("Syncfb device name is '%s'\n", caps.name);
	

	/* check which tty we use - /dex/ttyX could mean we are in fbdev mode */
	ttyn = ttyname(STDOUT_FILENO);
	if ( ! strncmp( "/dev/tty", ttyn, 8) ) {
		if ( sscanf(ttyn, "/dev/tty%d", &tt) ) {
			printf("We are on tty #%d\n", tt);
			x_running = 0; // dont test for x
		} else {
			printf("OOps, cannot parse tty name (%s)\n", ttyn);
		}
	} else {
		printf ("We are on unknown tty (%s)\n", ttyn);
	}




	
#ifdef HAVE_X
	/* check if X11 is running */

	if ( x_running == -1 ) {
		if(getenv("DISPLAY"))
			display_name = getenv("DISPLAY");
		else 
			display_name = ":0.0";
	
		if (display_name) xDisplay = XOpenDisplay(display_name);
	
		if ( xDisplay) {
			printf ( "X11 is running on display %s\n", display_name);
			xScreen = XDefaultScreenOfDisplay(xDisplay);
			disp_width = disp_vwidth = xScreen->width;
			disp_height = disp_vheight = xScreen->height;
			disp_bpp = xScreen->root_depth;
			printf ( "X Screen Resolution is %dx%d@%d\n", xScreen->width, xScreen->height, xScreen->root_depth);
			x_running = 1;
			XCloseDisplay(xDisplay);
		} else {
			printf ( "X11 is NOT running, will probe fbdev next...\n");
			x_running = 0;
		}

	}
#else
	x_running = 0;
#endif


	
	/* get fbdev parameters */
	if ( ! x_running ) {	
		fb = open("/dev/fb0",O_RDWR);
		if(fb == -1)
		{
			printf("Cannot open fb0(write)\n");

			fb = open("/dev/fb0",O_RDONLY);
			if ( fb == -1 ) {
				printf("Cannot open fb0(read)\n");
				exit(1);

			}


		}

		if (ioctl(fb,FBIOGET_FSCREENINFO,&fbfix)) perror("Error in FBIOGET_FSCRRENINFO ioctl");
	
		printf ( "[ScreenInfo] Id: %s\n", fbfix.id );
		printf ( "[ScreenInfo] SMem: %d\n", fbfix.smem_len );
		// printf ( "[ScreenInfo] MMIO len: %d\n", fbfix.mmio_len );

		if (ioctl(fb,FBIOGET_VSCREENINFO,&fbvar)) perror("Error in FBIOGET_FSCRRENINFO ioctl");

		printf ( "[ScreenInfo] res: %dx%d@%d (%dx%d)\n", fbvar.xres, fbvar.yres, fbvar.bits_per_pixel, fbvar.xres_virtual, fbvar.yres_virtual );
		disp_width = fbvar.xres; disp_height = fbvar.yres;
		disp_vwidth = fbvar.xres_virtual; disp_vheight = fbvar.yres_virtual;
		disp_bpp = fbvar.bits_per_pixel;


		i = disp_vwidth * disp_vheight * disp_bpp / 8;
		if ( i > fbfix.smem_len-614400 ) {
			printf ( "\n\n================== WARNING ===========================\n");
			printf ( "Console resolution of %dx%d@%d leaves only\n", disp_vwidth, disp_vheight, disp_bpp);
			printf ( "%d bytes of Memory for overlay use\n", fbfix.smem_len-i );
			printf ( "Please set your (virtual) display resolution to match\nthe physical resolution\n" );
			exit(1);
		}
		
	}



	if (ioctl(sf,SYNCFB_GET_CONFIG,&config)) perror("Error in get_config ioctl");

	config.syncfb_mode = SYNCFB_FEATURE_BLOCK_REQUEST;
	config.fb_screen_size = 14*0x100000; // 14MB //disp_vwidth * disp_vheight * disp_bpp / 8; // width * height * bits/8 
	config.src_width = SRC_IMAGE_WIDTH;
	config.src_height= SRC_IMAGE_HEIGHT;
	config.src_palette= VIDEO_PALETTE_YUV422;
	config.image_width = SRC_IMAGE_WIDTH * 2;
	config.image_height = SRC_IMAGE_HEIGHT;
	config.image_xorg = 18;
	config.image_yorg = 18;

	if (ioctl(sf,SYNCFB_SET_CONFIG,&config)) perror("Error in set_config ioctl");
	if (ioctl(sf,SYNCFB_ON)) perror("Error in ON ioctl");

	mga_vid_base = (uint_8*)mmap(0,caps.memory_size,PROT_WRITE,MAP_SHARED,sf,0);
	printf("syncfb_vid_base = %8p\n",mga_vid_base);


	memset(y_image,0xFF,256 * 256);

	printf("[Test 01] Color translation from green to yellow\n");
	for ( i = 0; i < 256; i++ ) {
		request_buf(sf);
		memset(cr_image,i,256*128);
		memset(cb_image,0x0,256*128);
		write_frame_YUV422(y_image,cr_image,cb_image, sbuf.offset);
		commit_buf(sf);
		// printf("+");
	}

	sleep(2);
	printf("[Test 01] finished.\n\n");

	printf("[Test 02] A white square\n");
	request_buf(sf);
	memset(cr_image,0x80,256*128);
	memset(cb_image,0x80,256*128);
	write_frame_YUV422(y_image,cr_image,cb_image, sbuf.offset);
	commit_buf(sf);
	sleep(2);
	printf("[Test 02] finished.\n\n");

	printf("[Test 03] A cool pattern\n");
	request_buf(sf);
	draw_cool_pattern();
	write_frame_YUV422(y_image,cr_image,cb_image, sbuf.offset);
	printf("[Test 03] Wrote the pattern, you should not see it yet...\n");
	sleep(3);
	commit_buf(sf);
	printf("[Test 03] Commited the pattern, you should not see it now.\n");
	sleep(2);
	printf("[Test 03] finished.\n\n");

	/* Test 4 */

	printf("[Test 04] Horizontal scaling\n");
	config.image_height = SRC_IMAGE_HEIGHT;
	config.image_width = SRC_IMAGE_WIDTH * 2;
	config.syncfb_mode = SYNCFB_FEATURE_BLOCK_REQUEST | SYNCFB_FEATURE_SCALE_H;
	if (ioctl(sf,SYNCFB_SET_CONFIG,&config))
	{
		perror("Error in set_config ioctl");
	}
	sleep(2);
	printf("[Test 04] finished.\n\n");

	
	printf("[Test 05] Vertical scaling\n");
	config.image_height = SRC_IMAGE_HEIGHT * 2;
	config.image_width = SRC_IMAGE_WIDTH;
	config.syncfb_mode = SYNCFB_FEATURE_BLOCK_REQUEST | SYNCFB_FEATURE_SCALE_V;
	if (ioctl(sf,SYNCFB_SET_CONFIG,&config))
	{
		perror("Error in set_config ioctl");
	}
	sleep(2);
	printf("[Test 05] finished.\n\n");



	printf("[Test 06] Fullscreen scaling\n");
	config.image_height = disp_height;
	config.image_width = disp_width;
	config.image_xorg = 0;
	config.image_yorg = 0;
	config.syncfb_mode = SYNCFB_FEATURE_BLOCK_REQUEST | SYNCFB_FEATURE_SCALE;
	if (ioctl(sf,SYNCFB_SET_CONFIG,&config))
	{
		perror("Error in set_config ioctl");
	}
	sleep(2);
	printf("[Test 06] finished.\n\n");

	printf("[Test 07] A color blend.\n");
	request_buf(sf);
	draw_color_blend();
	write_frame_YUV422(y_image,cr_image,cb_image, sbuf.offset);
	commit_buf(sf);
	sleep(3);
	printf("[Test 07] finished.\n");









	if (ioctl(sf,SYNCFB_OFF))
	{
		perror("Error in OFF ioctl");
	}

	close (fb);
	close (sf);

	exit(1);

}
