/*
 *  video_out_vesa.c
 *
 *	Copyright (C) Nickols_K <nickols_k@mail.ru> - Oct 2001
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 *  This file is partly based on vbetest.c from lrmi distributive.
 */

/*
  TODO:
  - hw YUV support (need volunteers who have corresponding hardware)
  - triple buffering (if it will really speedup playback).
    note: triple buffering requires VBE 3.0 - need volunteers.
  - refresh rate support (need additional info from mplayer)
*/
#include "mp_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mplayer.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "osdep/fastmemcpy.h"
#include "sub.h"
#include "osdep/vbelib.h"
#include "osdep/bswap.h"
#include "osdep/mplib.h"
#include "aspect.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#endif
#include "dri_vo.h"
#include "help_mp.h"
#include "vo_msg.h"
LIBVO_EXTERN(vesa)

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#define UNUSED(x) ((void)(x)) /**< Removes warning about unused arguments */

static vo_info_t vo_info =
{
	"VESA VBE 2.0 video output"
#ifdef CONFIG_VIDIX
	" (with vesa:vidix subdevice)"
#endif
	,
	"vesa",
	"Nickols_K <nickols_k@mail.ru>",
        "Requires ROOT privileges"
};

/* driver data */

struct win_frame
{
  uint8_t   *ptr;   /* pointer to window's frame memory */
  uint32_t   low;   /* lowest boundary of frame */
  uint32_t   high;  /* highest boundary of frame */
  char       idx;   /* indicates index of relocatable frame (A=0 or B=1)
			special case for DGA: idx=-1
			idx=-2 indicates invalid frame, exists only in init() */
};

static void (* __FASTCALL__ cpy_blk_fnc)(vo_data_t*,unsigned long,uint8_t *,unsigned long) = NULL;

typedef struct priv_s {
    uint32_t		srcW,srcH,srcBpp,srcFourcc; /* source image description */
    uint32_t		dstBpp,dstW,dstH,dstFourcc; /* destinition image description */

    uint32_t		x_offset,y_offset; /* to center image on screen */
    unsigned		init_mode; /* mode before run of mplayer */
    any_t*		init_state; /* state before run of mplayer */
    struct win_frame	win; /* real-mode window to video memory */
    uint8_t *		dga_buffer; /* for yuv2rgb and sw_scaling */
    unsigned		video_mode; /* selected video mode for playback */
    struct VesaModeInfoBlock vmode_info;

/* multibuffering */
    uint8_t*		video_base; /* should be never changed */
    uint32_t		multi_buff[MAX_DRI_BUFFERS]; /* contains offsets of buffers */
    uint8_t		multi_size; /* total number of buffers */
/* Linux Video Overlay */
#ifdef CONFIG_VIDIX
    const char *	vidix_name;
#endif
    uint32_t		subdev_flags;
}priv_t;

#define HAS_DGA()  (priv->win.idx == -1)
#define MOVIE_MODE (MODE_ATTR_COLOR | MODE_ATTR_GRAPHICS)
#define FRAME_MODE (MODE_WIN_RELOCATABLE | MODE_WIN_WRITEABLE)

static char * __FASTCALL__ vbeErrToStr(int err)
{
    char *retval;
    static char sbuff[80];
    if((err & VBE_VESA_ERROR_MASK) == VBE_VESA_ERROR_MASK) {
	sprintf(sbuff,"VESA failed = 0x4f%02x",(err & VBE_VESA_ERRCODE_MASK)>>8);
	retval = sbuff;
    } else
	switch(err) {
	    case VBE_OK: retval = "No error"; break;
	    case VBE_VM86_FAIL: retval = "vm86() syscall failed"; break;
	    case VBE_OUT_OF_DOS_MEM: retval = "Out of DOS memory"; break;
	    case VBE_OUT_OF_MEM: retval = MSGTR_OutOfMemory; break;
	    case VBE_BROKEN_BIOS: retval = "Broken BIOS or DOS TSR"; break;
	    default: sprintf(sbuff,"Unknown or internal error: %i",err); retval=sbuff; break;
    }
    return retval;
}

#define PRINT_VBE_ERR(name,err) { MSG_ERR("vo_vesa: %s returns: %s\n",name,vbeErrToStr(err)); fflush(stdout); }

static void vesa_term( vo_data_t*vo )
{
    priv_t*priv=(priv_t*)vo->priv;
    int err;
#ifdef CONFIG_VIDIX
    if(priv->vidix_name) vidix_term(vo);
#endif
    if((err=vbeRestoreState(priv->init_state)) != VBE_OK) PRINT_VBE_ERR("vbeRestoreState",err);
    if((err=vbeSetMode(priv->init_mode,NULL)) != VBE_OK) PRINT_VBE_ERR("vbeSetMode",err);
    if(HAS_DGA()) vbeUnmapVideoBuffer((unsigned long)priv->win.ptr,priv->win.high);
    if(priv->dga_buffer && !HAS_DGA()) mp_free(priv->dga_buffer);
    vbeDestroy();
}

#define VALID_WIN_FRAME(offset) (offset >= priv->win.low && offset < priv->win.high)
#define VIDEO_PTR(offset) (priv->win.ptr + offset - priv->win.low)

static inline void __vbeSwitchBank(vo_data_t* vo,unsigned long offset)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned long gran;
    unsigned new_offset;
    int err;
    gran = priv->vmode_info.WinGranularity*1024;
    new_offset = offset / gran;
    if(HAS_DGA()) { err = -1; goto show_err; }
    if((err=vbeSetWindow(priv->win.idx,new_offset)) != VBE_OK) {
	show_err:
	vesa_term(vo);
	PRINT_VBE_ERR("vbeSetWindow",err);
	MSG_FATAL("vo_vesa: Fatal error occured! Can't continue\n");
	exit(-1);
    }
    priv->win.low = new_offset * gran;
    priv->win.high = priv->win.low + priv->vmode_info.WinSize*1024;
}

static void __FASTCALL__ __vbeSetPixel(vo_data_t*vo,int x, int y, int r, int g, int b)
{
    priv_t*priv=(priv_t*)vo->priv;
    int x_res = priv->vmode_info.XResolution;
    int y_res = priv->vmode_info.YResolution;
    int shift_r = priv->vmode_info.RedFieldPosition;
    int shift_g = priv->vmode_info.GreenFieldPosition;
    int shift_b = priv->vmode_info.BlueFieldPosition;
    int pixel_size = (priv->dstBpp+7)/8;
    int bpl = priv->vmode_info.BytesPerScanLine;
    int color;
    unsigned offset;

    if (x < 0 || x >= x_res || y < 0 || y >= y_res)	return;
    r >>= 8 - priv->vmode_info.RedMaskSize;
    g >>= 8 - priv->vmode_info.GreenMaskSize;
    b >>= 8 - priv->vmode_info.BlueMaskSize;
    color = (r << shift_r) | (g << shift_g) | (b << shift_b);
    offset = y * bpl + (x * pixel_size);
    if(!VALID_WIN_FRAME(offset)) __vbeSwitchBank(vo,offset);
    memcpy(VIDEO_PTR(offset), &color, pixel_size);
}

/*
  Copies part of frame to video memory. Data should be in the same format
  as video memory.
*/
static void __FASTCALL__ __vbeCopyBlockFast(vo_data_t*vo,unsigned long offset,uint8_t *image,unsigned long size)
{
    priv_t*priv=(priv_t*)vo->priv;
    memcpy(&priv->win.ptr[offset],image,size);
}

static void __FASTCALL__ __vbeCopyBlock(vo_data_t*vo,unsigned long offset,uint8_t *image,unsigned long size)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned long delta,src_idx = 0;
    while(size) {
	if(!VALID_WIN_FRAME(offset)) __vbeSwitchBank(vo,offset);
	delta = min(size,priv->win.high - offset);
	memcpy(VIDEO_PTR(offset),&image[src_idx],delta);
	src_idx += delta;
	offset += delta;
	size -= delta;
    }
}

/*
  Copies frame to video memory. Data should be in the same format as video
  memory.
*/

#define PIXEL_SIZE() ((priv->dstBpp+7)/8)
#define SCREEN_LINE_SIZE(pixel_size) (priv->vmode_info.XResolution*(pixel_size) )
#define IMAGE_LINE_SIZE(pixel_size) (priv->dstW*(pixel_size))

static void __FASTCALL__ __vbeCopyData(vo_data_t*vo,uint8_t *image)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned long i,j,image_offset,offset;
    unsigned pixel_size,image_line_size,screen_line_size,x_shift;
    pixel_size = PIXEL_SIZE();
    screen_line_size = SCREEN_LINE_SIZE(pixel_size);
    image_line_size = IMAGE_LINE_SIZE(pixel_size);
    if(priv->dstW == priv->vmode_info.XResolution) {
	/* Special case for zooming */
	(*cpy_blk_fnc)(vo,priv->y_offset*screen_line_size,image,image_line_size*priv->dstH);
    } else {
	x_shift = priv->x_offset*pixel_size;
	for(j=0,i=priv->y_offset;j<priv->dstH;i++,j++) {
	    offset = i*screen_line_size+x_shift;
	    image_offset = j*image_line_size;
	    (*cpy_blk_fnc)(vo,offset,&image[image_offset],image_line_size);
	}
    }
}

static void __FASTCALL__ select_frame(vo_data_t* vo,unsigned idx)
{
    priv_t*priv=(priv_t*)vo->priv;
    MSG_DBG3("vo_vesa: select_frame was called\n");
    if(!HAS_DGA()) __vbeCopyData(vo,priv->dga_buffer);
    else {
	int err;
	if((err=vbeSetDisplayStart(priv->multi_buff[idx],vo_conf.vsync)) != VBE_OK) {
	    vesa_term(vo);
	    PRINT_VBE_ERR("vbeSetDisplayStart",err);
	    MSG_FATAL("vo_vesa: Fatal error occured! Can't continue\n");
	    exit(EXIT_FAILURE);
	}
	priv->win.ptr = priv->dga_buffer = priv->video_base + priv->multi_buff[(idx+1)%priv->multi_size];
    }
}

#define SUBDEV_NODGA     0x00000001UL
#define SUBDEV_FORCEDGA  0x00000002UL
static uint32_t __FASTCALL__ parseSubDevice(vo_data_t*vo,const char *sd)
{
    priv_t*priv=(priv_t*)vo->priv;
    uint32_t flags;
    flags = 0;
    if(strcmp(sd,"nodga") == 0) { flags |= SUBDEV_NODGA; flags &= ~(SUBDEV_FORCEDGA); }
    else
    if(strcmp(sd,"dga") == 0)   { flags &= ~(SUBDEV_NODGA); flags |= SUBDEV_FORCEDGA; }
#ifdef CONFIG_VIDIX
    else
    if(memcmp(sd,"vidix",5) == 0) priv->vidix_name = &sd[5]; /* priv->vidix_name will be valid within init() */
#endif
    else { MSG_ERR("vo_vesa: Unknown subdevice: '%s'\n", sd); return 0xFFFFFFFFUL; }
    return flags;
}

static int __FASTCALL__ check_depth(unsigned bpp)
{
    struct VbeInfoBlock vib;
    struct VesaModeInfoBlock vmib;
    int err;
    unsigned i,num_modes;
    unsigned short *mode_ptr;
    if((err=vbeGetControllerInfo(&vib)) != VBE_OK) return 0;
    num_modes = 0;
    mode_ptr = vib.VideoModePtr;
    while(*mode_ptr++ != 0xffff) num_modes++;
    mode_ptr = vib.VideoModePtr;
    for(i=0;i < num_modes;i++)
    {
	if((err=vbeGetModeInfo(mode_ptr[i],&vmib)) != VBE_OK) return 0;
	if(vmib.BitsPerPixel == bpp) return 1;
    }
    return 0;
}

static uint32_t __FASTCALL__ query_format(vo_query_fourcc_t* format)
{
    MSG_DBG3("vo_vesa: query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
    switch(format->fourcc) {
	case IMGFMT_BGR8: return check_depth(8);
	case IMGFMT_BGR15: return check_depth(15);
	case IMGFMT_BGR16: return check_depth(16);
	case IMGFMT_BGR24: return check_depth(24);
	case IMGFMT_BGR32: return check_depth(32);
	default: break;
    }
    return 0;
}

static void paintBkGnd( vo_data_t*vo )
{
    priv_t*priv=(priv_t*)vo->priv;
    int x_res = priv->vmode_info.XResolution;
    int y_res = priv->vmode_info.YResolution;
    int x, y;

    for (y = 0; y < y_res; ++y) {
	for (x = 0; x < x_res; ++x) {
	    int r, g, b;
	    if ((x & 16) ^ (y & 16)) {
		r = x * 255 / x_res;
		g = y * 255 / y_res;
		b = 255 - x * 255 / x_res;
	    } else {
		r = 255 - x * 255 / x_res;
		g = y * 255 / y_res;
		b = 255 - y * 255 / y_res;
	    }
	    __vbeSetPixel(vo, x, y, r, g, b);
	}
    }
}

static void clear_screen( vo_data_t*vo )
{
    priv_t*priv=(priv_t*)vo->priv;
    int x_res = priv->vmode_info.XResolution;
    int y_res = priv->vmode_info.YResolution;
    int x, y;

    for (y = 0; y < y_res; ++y)
	for (x = 0; x < x_res; ++x)
	    __vbeSetPixel(vo, x, y, 0, 0, 0);
}

static void clear_screen_fast( vo_data_t*vo )
{
    priv_t*priv=(priv_t*)vo->priv;
    int x_res = priv->vmode_info.XResolution;
    int y_res = priv->vmode_info.YResolution;
    int Bpp = (priv->vmode_info.BitsPerPixel+7)/8;

    memset(priv->dga_buffer,0,x_res*y_res*Bpp);
}

static char * __FASTCALL__ model2str(unsigned char type)
{
    char *retval;
    switch(type) {
	case memText: retval = "Text"; break;
	case memCGA:  retval="CGA"; break;
	case memHercules: retval="Hercules"; break;
	case memPL: retval="Planar"; break;
	case memPK: retval="Packed pixel"; break;
	case mem256: retval="256"; break;
	case memRGB: retval="Direct color RGB"; break;
	case memYUV: retval="Direct color YUV"; break;
	default: retval="Unknown"; break;
    }
    return retval;
}

static unsigned __FASTCALL__ fillMultiBuffer(vo_data_t*vo,unsigned long vsize, unsigned nbuffs )
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned long screen_size, offset;
    unsigned total,i;
    screen_size = priv->vmode_info.XResolution*priv->vmode_info.YResolution*((priv->dstBpp+7)/8);
    if(screen_size%64) screen_size=((screen_size/64)*64)+64;
    total = vsize / screen_size;
    i = 0;
    offset = 0;
    total = min(total,nbuffs);
    while(i < total) { priv->multi_buff[i++] = offset; offset += screen_size; }
    if(!i)
	MSG_ERR("vo_vesa: Your have too small size of video memory for this mode:\n"
		"vo_vesa: Requires: %08lX exists: %08lX\n", screen_size, vsize);
    return i;
}


/* fullscreen:
 * bit 0 (0x01) means fullscreen (-fs)
 * bit 1 (0x02) means mode switching (-vm)
 * bit 2 (0x04) enables software scaling (-zoom)
 * bit 3 (0x08) enables flipping (-flip) (NK: and for what?)
 */
static uint32_t __FASTCALL__ config(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format,const vo_tune_info_t *info)
{
    priv_t*priv=(priv_t*)vo->priv;
    struct VbeInfoBlock vib;
    struct VesaModeInfoBlock vmib;
    size_t i,num_modes;
    uint32_t w,h;
    unsigned short *mode_ptr,win_seg;
    unsigned bpp,best_x = UINT_MAX,best_y=UINT_MAX,best_mode_idx = UINT_MAX;
    int err,fs_mode,use_scaler=0;

    priv->srcW = priv->dstW = width;
    priv->srcH = priv->dstH = height;
    fs_mode = 0;
    if(priv->subdev_flags == 0xFFFFFFFEUL) {
	MSG_ERR("vo_vesa: detected internal fatal error: init is called before preinit\n");
	return -1;
    }
    if(priv->subdev_flags == 0xFFFFFFFFUL) return -1;
    if(flags & 0x8) {
	MSG_WARN("vo_vesa: switch -flip is not supported\n");
    }
    if(flags & 0x04) use_scaler = 1;
    if(flags & 0x01) {
	if(use_scaler) use_scaler = 2;
	else          fs_mode = 1;
    }
    memcpy(vib.VESASignature,"VBE2",4);
    if((err=vbeGetControllerInfo(&vib)) != VBE_OK) {
	PRINT_VBE_ERR("vbeGetControllerInfo",err);
	MSG_FATAL("vo_vesa: possible reason: No VBE2 BIOS found\n");
	return -1;
    }
    /* Print general info here */
    MSG_V("vo_vesa: Found VESA VBE BIOS Version %x.%x Revision: %x\n",
	(int)(vib.VESAVersion >> 8) & 0xff,
	(int)(vib.VESAVersion & 0xff),
	(int)(vib.OemSoftwareRev & 0xffff));
    MSG_V("vo_vesa: Video memory: %u Kb\n",vib.TotalMemory*64);
    MSG_V("vo_vesa: VESA Capabilities: %s %s %s %s %s\n"
	,vib.Capabilities & VBE_DAC_8BIT ? "8-bit DAC," : "6-bit DAC,"
	,vib.Capabilities & VBE_NONVGA_CRTC ? "non-VGA CRTC,":"VGA CRTC,"
	,vib.Capabilities & VBE_SNOWED_RAMDAC ? "snowed RAMDAC,":"normal RAMDAC,"
	,vib.Capabilities & VBE_STEREOSCOPIC ? "stereoscopic,":"no stereoscopic,"
	,vib.Capabilities & VBE_STEREO_EVC ? "Stereo EVC":"no stereo");
    MSG_V("vo_vesa: !!! Below will be printed OEM info. !!!\n");
    MSG_V("vo_vesa: You should watch 5 OEM related lines below else you've broken vm86\n"
	  "vo_vesa: OEM info: %s\n"
	  "vo_vesa: OEM Revision: %x\n"
	  "vo_vesa: OEM vendor: %s\n"
	  "vo_vesa: OEM Product Name: %s\n"
	  "vo_vesa: OEM Product Rev: %s\n"
	  ,vib.OemStringPtr
	  ,vib.OemSoftwareRev
	  ,vib.OemVendorNamePtr
	  ,vib.OemProductNamePtr
	  ,vib.OemProductRevPtr);
    MSG_HINT("vo_vesa: Hint: To get workable TV-Out you should have plugged tv-connector in\n"
	     "vo_vesa: before booting PC since VESA BIOS initializes itself only during POST\n");
    /* Find best mode here */
    num_modes = 0;
    mode_ptr = vib.VideoModePtr;
    while(*mode_ptr++ != 0xffff) num_modes++;
    switch(format) {
	case IMGFMT_BGR8:
	case IMGFMT_RGB8:  bpp = 8; break;
	case IMGFMT_BGR15:
	case IMGFMT_RGB15: bpp = 15; break;
	case IMGFMT_BGR16:
	case IMGFMT_RGB16: bpp = 16; break;
	case IMGFMT_BGR24:
	case IMGFMT_RGB24: bpp = 24; break;
	case IMGFMT_BGR32:
	case IMGFMT_RGB32: bpp = 32; break;
	default:	   bpp = 16; break;
    }
    priv->srcBpp = bpp;
    priv->srcFourcc = format;
    if(vo_conf.dbpp) bpp = vo_conf.dbpp;
    switch(bpp) {
	case 8:
	    priv->dstFourcc = IMGFMT_BGR8;
	    break;
	case 15:
	    priv->dstFourcc = IMGFMT_BGR15;
	    break;
	case 16:
	    priv->dstFourcc = IMGFMT_BGR16;
	    break;
	case 24:
	    priv->dstFourcc = IMGFMT_BGR24;
	    break;
	case 32:
	    priv->dstFourcc = IMGFMT_BGR32;
	    break;
	default:
	    priv->dstFourcc = IMGFMT_BGR16;
	    break;
    }
    if(mp_conf.verbose) {
	MSG_V("vo_vesa: Requested mode: %ux%u@%u (%s)\n",width,height,bpp,vo_format_name(format));
	MSG_V("vo_vesa: Total modes found: %u\n",num_modes);
	mode_ptr = vib.VideoModePtr;
	MSG_V("vo_vesa: Mode list:");
	for(i = 0;i < num_modes;i++) {
	    MSG_V(" %04X",mode_ptr[i]);
	}
	MSG_V("\nvo_vesa: Modes in detail:\n");
    }
    mode_ptr = vib.VideoModePtr;
    if(use_scaler) {
	priv->dstW = d_width;
	priv->dstH = d_height;
    }
    if(vo_conf.screenwidth) w = vo_conf.screenwidth;
    else w = max(priv->dstW,width);
    if(vo_conf.screenheight) h = vo_conf.screenheight;
    else h = max(priv->dstH,height);
    for(i=0;i < num_modes;i++) {
	if((err=vbeGetModeInfo(mode_ptr[i],&vmib)) != VBE_OK) {
	    PRINT_VBE_ERR("vbeGetModeInfo",err);
	    return -1;
	}
	if(vmib.XResolution >= w &&
	   vmib.YResolution >= h &&
	   (vmib.ModeAttributes & MOVIE_MODE) == MOVIE_MODE &&
	   vmib.BitsPerPixel == bpp) {
	    if((bpp > 8 && vmib.MemoryModel == memRGB) || bpp < 15)
	    if(vmib.XResolution <= best_x &&
		vmib.YResolution <= best_y) {
		    best_x = vmib.XResolution;
		    best_y = vmib.YResolution;
		    best_mode_idx = i;
	    }
	}
	if(mp_conf.verbose) {
	    MSG_V("vo_vesa: Mode (%03u): mode=%04X %ux%u@%u attr=%04X\n"
		  "vo_vesa:             #planes=%u model=%u(%s) #pages=%u\n"
		  "vo_vesa:             winA=%X(attr=%u) winB=%X(attr=%u) winSize=%u winGran=%u\n"
		  "vo_vesa:             direct_color=%u DGA_phys_addr=%08lX\n"
		  ,i,mode_ptr[i],vmib.XResolution,vmib.YResolution,vmib.BitsPerPixel,vmib.ModeAttributes
		  ,vmib.NumberOfPlanes,vmib.MemoryModel,model2str(vmib.MemoryModel),vmib.NumberOfImagePages
		  ,vmib.WinASegment,vmib.WinAAttributes,vmib.WinBSegment,vmib.WinBAttributes,vmib.WinSize,vmib.WinGranularity
		  ,vmib.DirectColorModeInfo,vmib.PhysBasePtr);
	    if(vmib.MemoryModel == 6 || vmib.MemoryModel == 7)
		MSG_V("vo_vesa:             direct_color_info = %u:%u:%u:%u\n"
			,vmib.RedMaskSize,vmib.GreenMaskSize,vmib.BlueMaskSize,vmib.RsvdMaskSize);
	    fflush(stdout);
	}
    }
    if(best_mode_idx != UINT_MAX) {
	priv->video_mode = vib.VideoModePtr[best_mode_idx];
	fflush(stdout);
	if((err=vbeGetMode(&priv->init_mode)) != VBE_OK) {
	    PRINT_VBE_ERR("vbeGetMode",err);
	    return -1;
	}
	MSG_V("vo_vesa: Initial video mode: %x\n",priv->init_mode);
	if((err=vbeGetModeInfo(priv->video_mode,&priv->vmode_info)) != VBE_OK) {
	    PRINT_VBE_ERR("vbeGetModeInfo",err);
	    return -1;
	}
	priv->dstBpp = priv->vmode_info.BitsPerPixel;
	MSG_V("vo_vesa: Using VESA mode (%u) = %x [%ux%u@%u]\n"
		,best_mode_idx,priv->video_mode,priv->vmode_info.XResolution
		,priv->vmode_info.YResolution,priv->dstBpp);
	if(priv->subdev_flags & SUBDEV_NODGA) priv->vmode_info.PhysBasePtr = 0;
	if(use_scaler || fs_mode) {
	    /* software scale */
	    if(use_scaler > 1) {
		aspect_save_orig(width,height);
		aspect_save_prescale(d_width,d_height);
		aspect_save_screenres(priv->vmode_info.XResolution,priv->vmode_info.YResolution);
		aspect(&priv->dstW,&priv->dstH,A_ZOOM);
	    } else if(fs_mode) {
		priv->dstW = priv->vmode_info.XResolution;
		priv->dstH = priv->vmode_info.YResolution;
	    }
	    use_scaler = 1;
	}
	if((priv->vmode_info.WinAAttributes & FRAME_MODE) == FRAME_MODE)
	    priv->win.idx = 0; /* frame A */
	else if((priv->vmode_info.WinBAttributes & FRAME_MODE) == FRAME_MODE)
	    priv->win.idx = 1; /* frame B */
	else priv->win.idx = -2;
	/* Try use DGA instead */
	if(priv->vmode_info.PhysBasePtr && vib.TotalMemory && (priv->vmode_info.ModeAttributes & MODE_ATTR_LINEAR)) {
	    any_t*lfb;
	    unsigned long vsize;
	    vsize = vib.TotalMemory*64*1024;
	    lfb = vbeMapVideoBuffer(priv->vmode_info.PhysBasePtr,vsize);
	    if(lfb == NULL) MSG_WARN("vo_vesa: Can't use DGA. Force bank switching mode. :(\n");
	    else {
		priv->video_base = priv->win.ptr = lfb;
		priv->win.low = 0UL;
		priv->win.high = vsize;
		priv->win.idx = -1; /* HAS_DGA() is on */
		priv->video_mode |= VESA_MODE_USE_LINEAR;
		MSG_V("vo_vesa: Using DGA (physical resources: %08lXh, %08lXh)"
		     ,priv->vmode_info.PhysBasePtr
		     ,vsize);
		MSG_V(" at %08lXh",(unsigned long)lfb);
		MSG_V("\n");
		if(!(priv->multi_size = fillMultiBuffer(vo,vsize,vo_conf.da_buffs))) return -1;
		if(priv->multi_size < 2) MSG_ERR("vo_vesa: Can't use double buffering: not enough video memory\n");
		else MSG_V("vo_vesa: using %u buffers for multi buffering\n",priv->multi_size);
	    }
	}
	if(priv->win.idx == -2) {
	   MSG_ERR("vo_vesa: Can't find neither DGA nor relocatable window's frame.\n");
	   return -1;
	}
	if(!HAS_DGA()) {
	    if(priv->subdev_flags & SUBDEV_FORCEDGA) {
		MSG_ERR("vo_vesa: you've forced DGA. Exiting\n");
		return -1;
	    }
	    if(!(win_seg = priv->win.idx == 0 ? priv->vmode_info.WinASegment:priv->vmode_info.WinBSegment)) {
		MSG_ERR("vo_vesa: Can't find valid window address\n");
		return -1;
	    }
	    priv->win.ptr = PhysToVirtSO(win_seg,0);
	    priv->win.low = 0L;
	    priv->win.high= priv->vmode_info.WinSize*1024;
	    MSG_V("vo_vesa: Using bank switching mode (physical resources: %08lXh, %08lXh)\n"
		 ,(unsigned long)priv->win.ptr,(unsigned long)priv->win.high);
	}
	if(priv->vmode_info.XResolution > priv->dstW) priv->x_offset = (priv->vmode_info.XResolution - priv->dstW) / 2;
	else priv->x_offset = 0;
	if(priv->vmode_info.YResolution > priv->dstH)
	    priv->y_offset = (priv->vmode_info.YResolution - priv->dstH) / 2;
	else priv->y_offset = 0;
	    MSG_V("vo_vesa: image: %ux%u screen = %ux%u priv->x_offset = %u priv->y_offset = %u\n"
		,priv->dstW,priv->dstH
		,priv->vmode_info.XResolution,priv->vmode_info.YResolution
		,priv->x_offset,priv->y_offset);
	if(HAS_DGA()) {
	    priv->dga_buffer = priv->win.ptr; /* Trickly ;) */
	    cpy_blk_fnc = __vbeCopyBlockFast;
	} else {
	    cpy_blk_fnc = __vbeCopyBlock;
#ifdef CONFIG_VIDIX
	    if(!priv->vidix_name)
#endif
	    {
		if(!(priv->dga_buffer = mp_memalign(64,priv->vmode_info.XResolution*priv->vmode_info.YResolution*priv->dstBpp))) {
		    MSG_ERR("vo_vesa: Can't allocate temporary buffer\n");
		    return -1;
		}
		MSG_V("vo_vesa: dga emulator was allocated = %p\n",priv->dga_buffer);
	    }
	}
	if((err=vbeSaveState(&priv->init_state)) != VBE_OK) {
	    PRINT_VBE_ERR("vbeSaveState",err);
	    return -1;
	}
	if((err=vbeSetMode(priv->video_mode,NULL)) != VBE_OK) {
	    PRINT_VBE_ERR("vbeSetMode",err);
	    return -1;
	}
	/* Now we are in video mode!!!*/
	/* Below 'return -1' is impossible */
	MSG_V("vo_vesa: Graphics mode was activated\n");
#ifdef CONFIG_VIDIX
	if(priv->vidix_name) {
	    if(vidix_init(vo,width,height,priv->x_offset,priv->y_offset,priv->dstW,
			priv->dstH,format,priv->dstBpp,
			priv->vmode_info.XResolution,priv->vmode_info.YResolution,info) != 0) {
		MSG_ERR("vo_vesa: Can't initialize VIDIX driver\n");
		priv->vidix_name = NULL;
		vesa_term(vo);
		return -1;
	    } else MSG_V("vo_vesa: Using VIDIX\n");
	    if(vidix_start(vo)!=0) {
		vesa_term(vo);
		return -1;
	    }
	}
#endif
    } else {
	MSG_ERR("vo_vesa: Can't find mode for: %ux%u@%u\n",width,height,bpp);
	return -1;
    }
    MSG_V("vo_vesa: VESA initialization complete\n");
    if(HAS_DGA()) {
	for(i=0;i<priv->multi_size;i++) {
	    priv->win.ptr = priv->dga_buffer = priv->video_base + priv->multi_buff[i];
	    if(mp_conf.verbose>1) paintBkGnd(vo);
	    else	  clear_screen_fast(vo);
	}
    } else {
	int x;
	if(mp_conf.verbose>1) paintBkGnd(vo);
	else clear_screen(vo);
	x = (priv->vmode_info.XResolution/priv->vmode_info.XCharSize)/2-strlen(title)/2;
	if(x < 0) x = 0;
	vbeWriteString(x,0,7,title);
    }
    return 0;
}

static const vo_info_t* get_info(vo_data_t*vo)
{
    UNUSED(vo);
    MSG_DBG3("vo_vesa: get_info was called\n");
    return &vo_info;
}

static void uninit(vo_data_t*vo)
{
    vesa_term(vo);
    MSG_DBG3("vo_vesa: uninit was called\n");
    mp_free(vo->priv);
}

static uint32_t __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    int pre_init_err = 0;
    MSG_DBG2("vo_vesa: preinit(%s) was called\n",arg);
    MSG_DBG3("vo_vesa: subdevice %s is being initialized\n",arg);
    vo->priv=mp_mallocz(sizeof(priv_t));
    priv_t*priv=(priv_t*)vo->priv;
    priv->subdev_flags = 0xFFFFFFFEUL;
    if(arg) priv->subdev_flags = parseSubDevice(vo,arg);
#ifdef CONFIG_VIDIX
    if(priv->vidix_name) pre_init_err = vidix_preinit(vo,priv->vidix_name,&video_out_vesa);
#endif
    MSG_DBG3("vo_subdevice: initialization returns: %i\n",pre_init_err);
    if(!pre_init_err)
	if((pre_init_err=vbeInit()) != VBE_OK) PRINT_VBE_ERR("vbeInit",pre_init_err);
    return pre_init_err;
}

static void __FASTCALL__ vesa_dri_get_surface_caps(vo_data_t*vo,dri_surface_cap_t *caps)
{
    priv_t*priv=(priv_t*)vo->priv;
    caps->caps = HAS_DGA() ? DRI_CAP_VIDEO_MMAPED : DRI_CAP_TEMP_VIDEO;
    caps->fourcc = priv->dstFourcc;
    caps->width=HAS_DGA()?priv->vmode_info.XResolution:priv->dstW;
    caps->height=HAS_DGA()?priv->vmode_info.YResolution:priv->dstH;
    caps->x=priv->x_offset;
    caps->y=priv->y_offset;
    caps->w=priv->dstW;
    caps->h=priv->dstH;
    caps->strides[0] = (HAS_DGA()?priv->vmode_info.XResolution:priv->dstW)*((priv->dstBpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

static void __FASTCALL__ vesa_dri_get_surface(vo_data_t*vo,dri_surface_t *surf)
{
    priv_t*priv=(priv_t*)vo->priv;
    surf->planes[0] = HAS_DGA()?priv->video_base + priv->multi_buff[surf->idx]:priv->dga_buffer;
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

static ControlCodes __FASTCALL__ control(vo_data_t*vo,uint32_t request, any_t*data)
{
    priv_t*priv=(priv_t*)vo->priv;
    switch (request) {
	case VOCTRL_QUERY_FORMAT:
	    return query_format((vo_query_fourcc_t*)data);
	case VOCTRL_GET_NUM_FRAMES:
	    *(uint32_t *)data = priv->multi_size;
	    return CONTROL_TRUE;
	case DRI_GET_SURFACE_CAPS:
	    vesa_dri_get_surface_caps(vo,data);
	    return CONTROL_TRUE;
	case DRI_GET_SURFACE:
	    vesa_dri_get_surface(vo,data);
	    return CONTROL_TRUE;
    }
    return CONTROL_NA;
}
