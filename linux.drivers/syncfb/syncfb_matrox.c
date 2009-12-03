
/*
 *
 * syncfb_matrox.c
 *
 * Copyright (C) 2000 Matthias Oelmann
 * 
 * based on mga_vid.c by Aaron Holzmann
 * Module skeleton based on gutted agpgart module by Jeff Hartmann 
 * <slicer@ionet.net>
 *
 * Synchronous Framebuffer Addon Module Version 0.0.0
 * 
 * This software has been released under the terms of the GNU Public
 * license. See http://www.gnu.org/copyleft/gpl.html for details.
 */

#include "syncfb.h"
#include "syncfb_matrox.h"

#ifdef SYNCFB_MATROX_SUPPORT
#define BYTE_ALIGN 32

/*
	PROTO
*/

static int  mga_probe_card(void);
static void mga_cleanup(void);
static void mga_enable(void);
static void mga_disable(void);
static void mga_handle_irq(int irq, void *dev_id, struct pt_regs *regs);
static void mga_vid_write_regs(void);
static int mga_set_config(syncfb_config_t *config);
static int mga_config_bes(syncfb_config_t *config);


/*
	HARDWARE
*/

struct pci_dev *mga_pci_dev = NULL;
static uint_8  *mga_mmio_base = 0;
static uint_32 mga_mem_base = 0;
static uint_32 mga_gx = 2; /* G200 */


static int mga_irq = -1;
static uint_32 mga_mem_size = 32 * 1024 * 1024;

static bes_registers_t regs;

static uint_8 mga_enabled_flag = 0;
static uint_32 mga_buf_offset[4];
static uint_32 mga_buf_offset_p2[4];
static uint_32 mga_buf_offset_p3[4];

static uint_32 mga_buf_size;
// static uint_32 mga_buf_pitch;

static uint_32 mga_conf_palette = 0;

static uint_8 mga_conf_deinterlace = 0;
static uint_8 mga_current_field;
static uint_8 mga_first_field = 1;

//static uint_32 debug_irqline;
static uint_32 debug_irqcnt = 0;
//static uint_32 debug_irqignore = 0;



static int debug_simulate_interrupt = 0;

static syncfb_capability_t mga_caps = {
	"Matrox",
	0,	/* palettes */
	0,      /*features */
	0	/* memsize */
};




static syncfb_capability_t *mga_capability() {
	return &mga_caps;
}



static int mga_config(uint_8 set, syncfb_config_t *config)
{	
	if ( set )
		return mga_set_config(config);
	else 
		return 0;
}

static int mga_set_config(syncfb_config_t *config)
{
	int i;
	uint_32 offs0, bufmin;

	if ( config->syncfb_mode & SYNCFB_FEATURE_DEINTERLACE ) mga_conf_deinterlace = 1;
	else mga_conf_deinterlace = 0;

	if ( ! (mga_caps.palettes & ( (uint_32)1 << config->src_palette)) ) {
		printk(KERN_INFO "syncfb (mga): palette not supported: %ld\n", config->src_palette);
		/* return -1;  palette not supported */
	}

	mga_conf_palette = config->src_palette;
	config->src_pitch = ( config->src_width + 31 ) & ~31; /* pixel align on 32 pix */

	bufmin = (config->src_pitch * config->src_height * 2) + 64; /* 64 is for cb cr offsets ???  */
	// bufmin = (config->src_pitch * config->src_height * syncfb_get_bpp(config->src_palette) / 8) + 64; /* 64 is for cb cr offsets ???  */
//	if ( bufmin * 4 + config->fb_screen_size  + BYTE_ALIGN * 5  > mga_mem_size ) {
//		printk("syncfb (mga): not enough buffer space\n");
//		return -1; /* not enough memory to create buffers */
//	}
	
	offs0 = config->fb_screen_size + (BYTE_ALIGN - (config->fb_screen_size % BYTE_ALIGN));
	mga_buf_size = bufmin + ( BYTE_ALIGN - (bufmin % BYTE_ALIGN) );
	config->buffer_size = mga_buf_size;

	for ( i = 0; i<4; i++ ) {
		mga_buf_offset[i] = offs0 + (i * mga_buf_size);
		mga_buf_offset_p2[i] = mga_buf_offset[i] + (config->src_pitch * config->src_height);
		mga_buf_offset_p3[i] = mga_buf_offset_p2[i] + (config->src_pitch * config->src_height / 2 );
	}

	if ( mga_conf_deinterlace ) config->buffers = 2;
	else config->buffers = 4;

	return mga_config_bes(config);
}



static int mga_config_bes(syncfb_config_t *config)
{
	uint_32 tmp;

	besctl_t besctl;
	besglobctl_t besglobctl;

	uint_32 x,y,sw,sh,dw,dh;
	uint_32 cropleft, cropright, croptop, cropbot;
	uint_32 offsetleft, offsetright, offsettop, offsetbot;
	uint_32 bespitch, besleft, besright, bestop, besbot;

	uint_32 ifactor, ifactorb ;
	uint_32 intrep, roundoff, acczoom;
	uint_32 beshiscal, besviscal;
	uint_32 beshsrcst, beshsrcend;
	uint_32 besv1srclst, besv2srclst;
	uint_32 sc_upper, sc_lower, besorg_sc1, besorg_sc2, besorg_mir, besorg_bytes;
	uint_32 bes1wght, bes1wghts, bes2wght, bes2wghts;
	uint_32 dataformat, format420;

	//FIXME check that window is valid and inside desktop
        x = config->image_xorg;
        y = config->image_yorg;

        sw = config->src_width;
        sh = config->src_height;

	if ( config->syncfb_mode & SYNCFB_FEATURE_SCALE_H)
        	dw = config->image_width;
	else
		dw = config->src_width;


	if ( config->syncfb_mode & SYNCFB_FEATURE_SCALE_V)
        	dh = config->image_height;
	else
		dh = config->src_height;


	if ( mga_conf_deinterlace ) sh = config->src_height/2;

	if ( config->syncfb_mode & SYNCFB_FEATURE_CROP ) {
		cropleft = config->src_crop_left;
		cropright = config->src_crop_right;
		croptop = config->src_crop_top;
		cropbot = config->src_crop_bot;
	} else {
		cropleft = 0;
		cropright = 0;
		croptop = 0;
		cropbot = 0;
	}
	

	if ( config->syncfb_mode & SYNCFB_FEATURE_OFFSET ) {
		offsetleft = config->image_offset_left;
		offsetright = config->image_offset_right;
		offsettop = config->image_offset_top;
		offsetbot = config->image_offset_bot;
	} else {
		offsetleft = 0;
		offsetright = 0;
		offsettop = 0;
		offsetbot = 0;
	}




	printk(KERN_DEBUG "mga_vid: Setting up a %ldx%ld+%ld+%ld video window (src %ldx%ld)\n",
	       dw, dh, x, y, sw, sh);

	
	//FIXME figure out a better way to allocate memory on card
	//allocate 2 megs
	//mga_src_base = mga_mem_base + (MGA_VIDMEM_SIZE-2) * 0x100000;
	/* mga_src_base = (MGA_VIDMEM_SIZE-2) * 0x100000; */
	
	//Setup the BES registers for composite 4:2:2 video source 
	
	//BES enabled, even start polarity, filtering enabled, chroma upsampling
	//enabled, 420 mode enabled, dither enabled, mirror disabled, b/w
	//disabled, blanking enabled, software field select, buffer a1 displayed


	// ====================================================
	//	BESCTL
	// ====================================================

	besctl.value = 0;

	besctl.f.besen = TRUE;
	besctl.f.besv1srcstp = 0; /* EVEN */
	if ( mga_conf_deinterlace ) besctl.f.besv2srcstp = 1; /* odd */
	else  besctl.f.besv2srcstp = 0;

	besctl.f.beshfen = TRUE;
	besctl.f.besvfen = TRUE;

	besctl.f.bescups = TRUE;
	besctl.f.besdith = TRUE;

	besctl.f.besbwen = FALSE;
	besctl.f.bes420pl = FALSE;



	// ====================================================
	//	BESGLOBCTL
	// ====================================================

	// FIX: G200 support

	besglobctl.value = 0;
	besglobctl.f.besvcnt = 1;


	// ====================================================
	//	PALETTE
	// ====================================================

	besglobctl.f.bescorder = 0;

	switch ( mga_conf_palette ) {
		case VIDEO_PALETTE_YUYV:
		case VIDEO_PALETTE_YUV422:
			besctl.f.bes420pl      = 0;
			besglobctl.f.besuyvyfmt= 0;
			break;

		case VIDEO_PALETTE_UYVY:
			besctl.f.bes420pl      = 0;
			besglobctl.f.besuyvyfmt= 1;
			break;

		case VIDEO_PALETTE_YUV420P2:
			besctl.f.bes420pl      = 1;
			besglobctl.f.bes3plane = 0;
			break;

		case VIDEO_PALETTE_YUV420P3:
			besctl.f.bes420pl      = 1;
			besglobctl.f.bes3plane = 1;
			break;

		case VIDEO_PALETTE_RGB565: 
			besctl.f.bes420pl      = 0; 
			besglobctl.f.besuyvyfmt= 0; 
			besglobctl.f.besrgbmode = 2; 
			besctl.f.beshfen = 0; 
			besctl.f.besvfen = 0; 
			besctl.f.bescups = 0; 
			besctl.f.besdith = 0; 
			break; 

		default:
			besctl.f.bes420pl      = 0;
			besglobctl.f.besuyvyfmt= 0;
	}

	regs.besctl = besctl.value; 
	if ( mga_gx < 4 ) besglobctl.value = besglobctl.value & 0x0FFF001b;
	else regs.besglobctl = besglobctl.value;




	// ====================================================
	//	PITCH
	// ====================================================
	if ( mga_conf_deinterlace ) bespitch = (config->src_pitch*2) & 0xFFF;	
	else bespitch = config->src_pitch & 0xFFF;
	
	regs.bespitch    = bespitch ; 

	// ====================================================
	//	CONTRAST/BRIGHTNESS/COLOR(on-off)
	// ====================================================

	//Disable contrast and brightness control
	regs.beslumactl = 0x80<<0;


	// ====================================================
	//	WINDOW BOUNDARIES
	// ====================================================

	besleft = x > 0 ? x : 0;
	besright = x + dw - 1;

	bestop  = y > 0 ? y : 0;
	besbot = y + dh - 1;
	
	regs.beshcoord = (besleft<<16) + besright;
	regs.besvcoord = (bestop <<16) + besbot;



	// ====================================================
	//	HORIZONTAL SCALING
	// ====================================================
	
	// Step 1 interval representation value
	// ====================================
	
	intrep = 0;
	if ( besctl.f.beshfen ) {
		if ( sw == dw  || dw < 2 ) intrep = 0;
		else intrep = 1;
	} else {
		if ( dw < sw ) intrep = 1;
		else intrep = 0;
	}
	
	// Step 2 Inverse Scaling Factor
	// =============================

	ifactor =  (( sw - cropleft - cropright - intrep ) << 14) / ( dw - intrep );
	ifactorb =  (( sw - cropleft - cropright - intrep ) << 20) / ( dw - intrep ) + intrep;

	ifactor = ifactor & 0x7FFFF;
	ifactorb = ifactorb & 0x7FFFF;

	// Step 3 Round Off Variable
	// =========================

	if ( besctl.f.beshfen ) {
		roundoff = 0;
	} else {
		if ( (ifactorb * (dw - 1)) >> 20 > (ifactor * (dw -1)) >> 14 ) roundoff = 1;
		else roundoff = 0; 
	}

	// Step 4 Accelerated 2x Zoom
	// ==========================

	acczoom = (besglobctl.f.beshzoom) ? 2 : 1; 

	// Step 5 beshiscal
	// =================

	beshiscal = ((acczoom * ifactor) + roundoff) & 0x7FFFF;  // 5.14

	regs.beshiscal = (beshiscal & 0x7FFFF) << 2;

	// ====================================================
	//	HORIZONTAL SOURCE POSITIONING
	// ====================================================

	if ( besctl.f.beshmir ) {
		beshsrcst = (cropleft<<14) + offsetleft * (ifactor + roundoff);
	} else {
		beshsrcst = (cropright << 14 ) + offsetleft  * (ifactor + roundoff);
	}

	beshsrcend = beshsrcst + ((dw - offsetleft - offsetright -1) / acczoom) * (acczoom * ifactor + roundoff);

	regs.beshsrcst = (beshsrcst & 0xFFFFFF) << 2;
	regs.beshsrcend = (beshsrcend & 0xFFFFFF) << 2;


	// ====================================================
	//	VERTICAL SCALING
	// ====================================================

	// Step 1 interval representation value
	// ====================================
	
	intrep = 0;
	if ( besctl.f.besvfen ) {
		if ( sh == dh  || dw < 2 ) intrep = 0;
		else intrep = 1;
	} else {
		if ( dh < sh ) intrep = 1;
		else intrep = 0;
	}

	// Step 2 Inverse Scaling Factor (V)
	// =================================

	ifactor =   (( sh - croptop - cropbot - intrep ) << 14) / ( dh - intrep );
	ifactorb =  (( sh - croptop - cropbot - intrep ) << 20) / ( dh - intrep ) + intrep;

	ifactor = ifactor & 0x7FFFF;
	ifactorb = ifactorb & 0x7FFFF;

	// Step 3 Round Off Variable
	// =========================

	if ( besctl.f.besvfen ) {
		roundoff = 0;
	} else {
		if ( (ifactorb * (dh -1)) >> 20 > (ifactor * (dh -1)) >> 14 ) roundoff = 1;
		else roundoff = 0; 
	}


	// Step 4 besviscal
	// ================

	besviscal = (ifactor + roundoff) & 0x7FFFF;

	regs.besviscal = (besviscal & 0x7FFFF) << 2;





	// ====================================================
	//	VERTICAL SUBPIXEL COMPENSATION
	// ====================================================

	if ( dh < sh ) { /* DOWNSCALE */
		sc_upper = 0;
		sc_lower = ( 2 * besviscal - (1<<14)) / 2;
	} else { /* UPSCALE */
		sc_upper = 0;
		sc_lower =  ( (dh-intrep)<<14 ) / ( 2 * (sh-croptop - cropbot - intrep)) ;
	}


	/* 	
		Vertical Source Positioning 
	*/
	dataformat = (besctl.f.bes420pl) ? 1 : 2;
	format420 = (besglobctl.f.bes3plane) ? 2 : 1;



	// ====================================================
	//	ORIGINS
	// ====================================================

	if ( dh < sh ) { /* DOWNSCALE */
		besorg_sc1 = (((croptop<<14) + (offsettop * besviscal) + sc_upper) >> 14) * bespitch * dataformat;
		besorg_sc2 = (((croptop<<14) + (offsettop * besviscal) + sc_lower) >> 14) * bespitch * dataformat;
		besorg_mir = besctl.f.beshmir ? (sw * dataformat -1) : 0 ;
		if ( ! mga_conf_deinterlace ) besorg_sc2 = besorg_sc1;

		regs.besa1org = mga_buf_offset[0] + besorg_sc1 + besorg_mir; 
		regs.besb1org = mga_buf_offset[1] + besorg_sc1 + besorg_mir; 

		if ( mga_conf_deinterlace ) regs.besa2org = mga_buf_offset[0] + besorg_sc2 + besorg_mir; 
		else regs.besa2org = mga_buf_offset[2] + besorg_sc2 + besorg_mir; 

		if ( mga_conf_deinterlace ) regs.besb2org = mga_buf_offset[1] + besorg_sc2 + besorg_mir; 
		else regs.besb2org = mga_buf_offset[3] + besorg_sc2 + besorg_mir; 
	} else { /* UPSCALE */

		if ( (offsettop<<14) > sc_lower ) 
			tmp = ((offsettop<<14) - sc_lower) >> 14;
		else	
			tmp = (sc_lower - (offsettop<<14)) >> 14;


		besorg_sc1 =  (((croptop + offsettop) * besviscal) >>14 ) ;
		besorg_sc2 =  (((croptop + tmp) * besviscal) >>14 ) ;
		besorg_bytes = bespitch*dataformat;


		besorg_mir =  besctl.f.beshmir ? (sw * dataformat -1) : 0;
		if ( ! mga_conf_deinterlace ) besorg_sc2 = besorg_sc1;

		regs.besa1org = mga_buf_offset[0] + besorg_sc1*besorg_bytes + besorg_mir; 
		regs.besb1org = mga_buf_offset[1] + besorg_sc1*besorg_bytes + besorg_mir; 

		regs.besa1corg = mga_buf_offset_p2[0] + (besorg_sc1 / (2*format420)) * bespitch;
		regs.besb1corg = mga_buf_offset_p2[1] + (besorg_sc1 / (2*format420)) * bespitch;

		regs.besa1c3org = mga_buf_offset_p3[0] + (besorg_sc1 / 4) * bespitch;
		regs.besb1c3org = mga_buf_offset_p3[1] + (besorg_sc1 / 4) * bespitch;
		
		if ( mga_conf_deinterlace ) {
			regs.besa2org = mga_buf_offset[0] + (bespitch/2) * dataformat  + besorg_sc2*besorg_bytes + besorg_mir; 
			regs.besb2org = mga_buf_offset[1] + (bespitch/2) * dataformat + besorg_sc2*besorg_bytes + besorg_mir;
			regs.besa2corg = mga_buf_offset_p2[0] + (besorg_sc1 / (2*format420)) * bespitch;
			regs.besb2corg = mga_buf_offset_p2[1] + (besorg_sc1 / (2*format420)) * bespitch;
			//regs.besa2corg = mga_buf_offset_p2[2] + (bespitch/(2*format420)) * dataformat + (besorg_sc1 / (2*format420)) * bespitch;
			// regs.besb2corg = mga_buf_offset_p2[3] + (bespitch/(2*format420)) * dataformat + (besorg_sc1 / (2*format420)) * bespitch;
			regs.besa2c3org = mga_buf_offset_p3[0] + (bespitch/(2*format420)) * dataformat + (besorg_sc1 / 4) * bespitch;
			regs.besb2c3org = mga_buf_offset_p3[1] + (bespitch/(2*format420)) * dataformat + (besorg_sc1 / 4) * bespitch;
		} else {
			regs.besa2org = mga_buf_offset[2] + besorg_sc2*besorg_bytes + besorg_mir; 
			regs.besb2org = mga_buf_offset[3] + besorg_sc2*besorg_bytes + besorg_mir;
			regs.besa2corg = mga_buf_offset_p2[2] + (besorg_sc1 / (2*format420)) * bespitch;
			regs.besb2corg = mga_buf_offset_p2[3] + (besorg_sc1 / (2*format420)) * bespitch;
			regs.besa2c3org = mga_buf_offset_p3[2] + (besorg_sc1 / 4) * bespitch;
			regs.besb2c3org = mga_buf_offset_p3[3] + (besorg_sc1 / 4) * bespitch;
		}
	}

	// ====================================================
	//	WEIGHTS
	// ====================================================
	
	if ( mga_conf_deinterlace ) {
		if ( offsettop * besviscal >= 0x2000 ) {
			if ( offsettop > 0 ) {
				bes1wght = ( offsettop * besviscal ) & 0x3FFF;
				bes1wghts = 0;
				bes2wght = ((offsettop<<14) - 0x2000) & 0x3FFF;
				bes2wghts = 0;
			} else {
				bes1wght = ( offsettop * besviscal ) & 0x3FFF;
				bes1wghts = 0;
				bes2wght = 0x2000;
				bes2wghts = 1;
			}
		} else {
			bes1wght = ( offsettop * besviscal ) & 0x3FFF;
			bes1wghts = 0;
			bes2wght = (0x2000 + (offsettop * besviscal));
			bes2wghts = 1;
		}
	} else {
		bes1wght = ( offsettop * besviscal ) & 0x3FFF;
		bes2wght = ( offsettop * besviscal ) & 0x3FFF;
		bes1wghts = 0;
		bes2wghts = 0;
	}


	regs.besv1wght = (bes1wghts << 16) + ((bes1wght & 0x3FFF) << 2);
	regs.besv2wght = (bes2wghts << 16) + ((bes2wght & 0x3FFF) << 2);




	// ====================================================
	//	VERTICAL SOURCE LAST POSITION
	// ====================================================


	if ( (offsettop<<14) > sc_lower ) 
		tmp = ((offsettop<<14) - sc_lower) >> 14;
	else	
		tmp = (sc_lower - (offsettop<<14)) >> 14;


	if ( mga_conf_deinterlace ) {
		besv1srclst = sh - 1 - ( ((croptop<<14) + offsettop*besviscal) >>14 );
		besv2srclst = sh - 1 - ( ((croptop<<14) + tmp*besviscal) >>14 );
	} else {
		besv1srclst = sh - 1 - ( ((croptop<<14) + offsettop*besviscal) >>14 );
		besv2srclst = sh - 1 - ( ((croptop<<14) + offsettop*besviscal) >>14 );
	}

	regs.besv1srclst = besv1srclst & 0x03FF;
	regs.besv2srclst = besv2srclst & 0x03FF;

	// ====================================================
	//	FINISH
	// ====================================================

	mga_vid_write_regs();
	return 0;
}




/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */





void mga_enable(void) {

	long int cc;

	cc = readl(mga_mmio_base + IEN);
	printk(KERN_ALERT "*** !!! IRQREG = %d\n", (int)(cc&0xff));

	writeb( 0x11, mga_mmio_base + CRTCX);
	
	writeb(0x20, mga_mmio_base + CRTCD );  /* clear 0, enable off */
	writeb(0x00, mga_mmio_base + CRTCD );  /* enable on */
	writeb(0x10, mga_mmio_base + CRTCD );  /* clear = 1 */
	
	writel( regs.besglobctl , mga_mmio_base + BESGLOBCTL);

	mga_enabled_flag = 1;
/*

	writel(  0x20 , mga_mmio_base + ICLEAR);
	writel(  0x20 , mga_mmio_base + IEN);
			printk("mga_vid: Video ON\n");
			vid_src_ready = 1;
			if(vid_overlay_on)
			{
				regs.besctl |= 1;
				mga_vid_write_regs();
			}
*/
}




void mga_disable(void) {

	if ( ! mga_enabled_flag ) return;
	mga_enabled_flag = 0;

	writeb( 0x11, mga_mmio_base + CRTCX);
	writeb(0x20, mga_mmio_base + CRTCD );  /* clear 0, enable off */

	regs.besctl &= ~1;
	mga_vid_write_regs();

/*
			printk("mga_vid: Video OFF\n");
			vid_src_ready = 0;   
			regs.besctl &= ~1;
			mga_vid_write_regs();
			break;
	//Close the window just in case
	vid_src_ready = 0;   
	regs.besctl &= ~1;
	mga_vid_write_regs();
	mga_vid_in_use = 0;

	//FIXME put back in!
	//MOD_DEC_USE_COUNT;
*/			
}

void mga_select_buffer(uint_8 buf) {

	long int cc;

	/* cc = readl(mga_mmio_base + BESCTL); */
	/* cc = 0x00010401; */
	cc = regs.besctl;

	switch(buf) {
		case 0: /* A1 */
			mga_current_field = 0;
			break;
		case 1: /* B1 */
			mga_current_field = 1;
			cc = cc | 0x04000000; 
			break;
		case 2: /* A2 */
			mga_current_field = 2;
			cc = cc | 0x02000000;
			break;
		case 3: /* B2 */
			mga_current_field = 3;
			cc = cc | 0x06000000;
			break;
		default:
			break;
	}

	// printk("select buffer  #%d\n", buf);

	writel(cc, mga_mmio_base + BESCTL);
	//writel( regs.besglobctl , mga_mmio_base + BESGLOBCTL);
}

void mga_handle_irq(int irq, void *dev_id, struct pt_regs *pregs) {

	long int cc;
	if ( ! mga_enabled_flag ) return;

	//printk("mga_interrupt #%d\n", irq);

	if ( irq != -1 ) {

		cc = readl(mga_mmio_base + STATUS);
		if ( ! (cc & 0x10) ) return;  /* vsyncpen */
 		debug_irqcnt++;
	} 

//    if ( debug_irqignore ) {
//	debug_irqignore = 0;


	if ( mga_conf_deinterlace ) {
		if ( mga_first_field ) {
			// printk("mga_interrupt first field\n");
			if ( syncfb_interrupt() )
				mga_first_field = 0;
		} else {
			// printk("mga_interrupt second field\n");
			mga_select_buffer( mga_current_field | 2 );
			mga_first_field = 1;
		}
	} else {
		syncfb_interrupt();
	}

//    } else {
//	debug_irqignore = 1;
//    }

	if ( irq != -1 ) {
		writeb( 0x11, mga_mmio_base + CRTCX);
		writeb( 0, mga_mmio_base + CRTCD );
		writeb( 0x10, mga_mmio_base + CRTCD );
	}

//	writel( regs.besglobctl, mga_mmio_base + BESGLOBCTL);


	return;

}


void mga_request_buffer(syncfb_buffer_info_t *buf) {

	buf->offset = mga_buf_offset[buf->id];
	buf->offset_p2 = mga_buf_offset_p2[buf->id];
	buf->offset_p3 = mga_buf_offset_p3[buf->id];
}





static void mga_vid_write_regs(void)
{

	uint_8 colkey_on = 0;
	uint_32 colkey_red, colkey_green, colkey_blue;

	//Make sure internal registers don't get updated until we're done
	//writel( (readl(mga_mmio_base + VCOUNT)-1)<<16,
	//			mga_mmio_base + BESGLOBCTL);

	// color or coordinate keying
	writeb( XKEYOPMODE, mga_mmio_base + PALWTADD);


	writeb( colkey_on, mga_mmio_base + X_DATAREG);
	if ( colkey_on ) 
	{
		uint_32 r=0, g=0, b=0;

		writeb( XMULCTRL, mga_mmio_base + PALWTADD);
		switch (readb (mga_mmio_base + X_DATAREG)) 
		{
			case BPP_8:
				/* Need to look up the color index, just using
				 color 0 for now. */
			break;

			case BPP_15:
				r = colkey_red   >> 3;
				g = colkey_green >> 3;
				b = colkey_blue  >> 3;
			break;

			case BPP_16:
				r = colkey_red   >> 3;
				g = colkey_green >> 2;
				b = colkey_blue  >> 3;
			break;

			case BPP_24:
			case BPP_32_DIR:
			case BPP_32_PAL:
				r = colkey_red;
				g = colkey_green;
				b = colkey_blue;
			break;
		}

		// Disable color keying on alpha channel 
		writeb( XCOLMSK, mga_mmio_base + PALWTADD);
		writeb( 0x00, mga_mmio_base + X_DATAREG);
		writeb( X_COLKEY, mga_mmio_base + PALWTADD);
		writeb( 0x00, mga_mmio_base + X_DATAREG);

		// Set up color key registers
		writeb( XCOLKEY0RED, mga_mmio_base + PALWTADD);
		writeb( r, mga_mmio_base + X_DATAREG);
		writeb( XCOLKEY0GREEN, mga_mmio_base + PALWTADD);
		writeb( g, mga_mmio_base + X_DATAREG);
		writeb( XCOLKEY0BLUE, mga_mmio_base + PALWTADD);
		writeb( b, mga_mmio_base + X_DATAREG);

		// Set up color key mask registers
		writeb( XCOLMSK0RED, mga_mmio_base + PALWTADD);
		writeb( 0xff, mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK0GREEN, mga_mmio_base + PALWTADD);
		writeb( 0xff, mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK0BLUE, mga_mmio_base + PALWTADD);
		writeb( 0xff, mga_mmio_base + X_DATAREG);
	}
 



	// Backend Scaler
	writel( regs.besctl,      mga_mmio_base + BESCTL); 
	if( mga_gx == 4)
		writel( regs.beslumactl,  mga_mmio_base + BESLUMACTL); 
	writel( regs.bespitch,    mga_mmio_base + BESPITCH); 

	writel( regs.besa1org,    mga_mmio_base + BESA1ORG);
	writel( regs.besa2org,    mga_mmio_base + BESA2ORG);
	writel( regs.besb1org,    mga_mmio_base + BESB1ORG);
	writel( regs.besb2org,    mga_mmio_base + BESB2ORG);

	writel( regs.besa1corg,    mga_mmio_base + BESA1CORG);
	writel( regs.besa2corg,    mga_mmio_base + BESA2CORG);
	writel( regs.besb1corg,    mga_mmio_base + BESB1CORG);
	writel( regs.besb2corg,    mga_mmio_base + BESB2CORG);

	if( mga_gx == 4) {
		writel( regs.besa1c3org,    mga_mmio_base + BESA1C3ORG);
		writel( regs.besa2c3org,    mga_mmio_base + BESA2C3ORG);
		writel( regs.besb1c3org,    mga_mmio_base + BESB1C3ORG);
		writel( regs.besb2c3org,    mga_mmio_base + BESB2C3ORG);
	}

	writel( regs.beshcoord,   mga_mmio_base + BESHCOORD);
	writel( regs.beshiscal,   mga_mmio_base + BESHISCAL);
	writel( regs.beshsrcst,   mga_mmio_base + BESHSRCST);
	writel( regs.beshsrcend,  mga_mmio_base + BESHSRCEND);
	writel( regs.beshsrclst,  mga_mmio_base + BESHSRCLST);
	
	writel( regs.besvcoord,   mga_mmio_base + BESVCOORD);
	writel( regs.besviscal,   mga_mmio_base + BESVISCAL);
	writel( regs.besv1srclst, mga_mmio_base + BESV1SRCLST);
	writel( regs.besv1wght,   mga_mmio_base + BESV1WGHT);
	writel( regs.besv2srclst, mga_mmio_base + BESV2SRCLST);
	writel( regs.besv2wght,   mga_mmio_base + BESV2WGHT);
	
	//update the registers somewhere between 1 and 2 frames from now.
	// writel( regs.besglobctl + ((readl(mga_mmio_base + VCOUNT)+2)<<16),
	//		mga_mmio_base + BESGLOBCTL);


	writel( regs.besglobctl , mga_mmio_base + BESGLOBCTL);

	printk(KERN_DEBUG "mga_vid: wrote BES registers\n");
	printk(KERN_DEBUG "mga_vid: BESCTL = 0x%08x\n",
			readl(mga_mmio_base + BESCTL));
	printk(KERN_DEBUG "mga_vid: BESGLOBCTL = 0x%08x\n",
			readl(mga_mmio_base + BESGLOBCTL));
	printk(KERN_DEBUG "mga_vid: BESSTATUS= 0x%08x\n",
			readl(mga_mmio_base + BESSTATUS));
	
	//FIXME remove
	printk(KERN_DEBUG "besa1org/a1c/a1c3 = %08lx %08lx %08lx\n",regs.besa1org, regs.besa1corg, regs.besa1c3org);
	printk(KERN_DEBUG "besb1org/b1c/b1c3 = %08lx %08lx %08lx\n",regs.besb1org, regs.besb1corg, regs.besb1c3org);
	printk(KERN_DEBUG "besa2org/a2c/a2c3 = %08lx %08lx %08lx\n",regs.besa2org, regs.besa2corg, regs.besa2c3org);
	printk(KERN_DEBUG "besb2org/b2c/b2c3 = %08lx %08lx %08lx\n",regs.besb2org, regs.besb2corg, regs.besb2c3org);
	//FIXME remove
}



void mga_cleanup() {

	if ( mga_enabled_flag ) mga_disable();

	if ( mga_irq != -1)
		free_irq(mga_irq, &mga_irq);

	if ( mga_mmio_base )
		iounmap(mga_mmio_base);

}


int mga_mmap(struct file *file, struct vm_area_struct *vma) {
        printk(KERN_DEBUG "syncfb (mga): mapping video memory into userspace\n");
        if(remap_page_range(vma->vm_start, mga_mem_base,
                 vma->vm_end - vma->vm_start, vma->vm_page_prot))
        {
                printk(KERN_ERR "syncfb: error mapping video memory\n");
                return(-EAGAIN);
        }

        return(0);
}


/* returns 0 on success, errcode on failure */
int mga_probe_card(void) {

	int tmp;

	if((mga_pci_dev = pci_find_device(PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G400, NULL)))
	{
		printk(KERN_DEBUG "syncfb (mga): Found MGA G400\n");
		mga_gx = 4;
		mga_caps.palettes = (1<<VIDEO_PALETTE_YUV422) | (1<<VIDEO_PALETTE_YUV420P3) | (1<<VIDEO_PALETTE_YUV420P2) | (1<<VIDEO_PALETTE_RGB565);
		strcpy(mga_caps.name, "MGA G400 Syncfb Device");
	}
	else if((mga_pci_dev = pci_find_device(PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G200_AGP, NULL)))
	{
		printk(KERN_DEBUG "syncfb (mga): Found MGA G200 AGP\n");
		mga_gx = 2;
		mga_caps.palettes = (1<<VIDEO_PALETTE_YUV422) | (1<<VIDEO_PALETTE_YUV420P2);
		strcpy(mga_caps.name, "MGA G200 AGP Syncfb Device");
	}
	else if((mga_pci_dev = pci_find_device(PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G200_PCI, NULL)))
	{
		printk(KERN_DEBUG "syncfb (mga): Found MGA G200 PCI\n");
		mga_gx = 2;
		mga_caps.palettes = (1<<VIDEO_PALETTE_YUV422) | (1<<VIDEO_PALETTE_YUV420P2);
		strcpy(mga_caps.name, "MGA G400 PCI Syncfb Device");
	}
	else
	{
		printk(KERN_ERR "syncfb (mga): No matrox Gx00 cards found\n");
		return -1;
	}

	mga_caps.features = 0;
	mga_caps.features |= SYNCFB_FEATURE_SCALE_H;
	mga_caps.features |= SYNCFB_FEATURE_SCALE_V;
	mga_caps.features |= SYNCFB_FEATURE_DEINTERLACE;
	mga_caps.features |= SYNCFB_FEATURE_BLOCK_REQUEST;
	mga_caps.features |= SYNCFB_FEATURE_FREQDIV2;


/*
	INITIALIZE
*/

	mga_irq = mga_pci_dev->irq;

#if LINUX_VERSION_CODE >= 0x020300
	mga_mmio_base = ioremap_nocache(mga_pci_dev->resource[1].start,0x4000);
	mga_mem_base =  mga_pci_dev->resource[0].start;
#else
	mga_mmio_base = ioremap_nocache(mga_pci_dev->base_address[1] & PCI_BASE_ADDRESS_MEM_MASK,0x4000);
	mga_mem_base =  mga_pci_dev->base_address[0] & PCI_BASE_ADDRESS_MEM_MASK;
#endif

	printk(KERN_DEBUG "syncfb (mga): MMIO at 0x%p IRQ: %d\n", mga_mmio_base, mga_irq);
	printk(KERN_DEBUG "syncfb (mga): Frame at 0x%08lX\n", mga_mem_base);

	/* ltmp = readl(mga_mmio_base + OPTION); */
	if ( mga_gx == 2 ) {  /* G200 */
		/* if ( ltmp & 0xc00 ) mga_mem_size = 16 * 1024 *1024;a */
		mga_mem_size = 8 * 1024 * 1024;
	} else {
		mga_mem_size = 16 * 1024 * 1024;
	}

	mga_caps.memory_size = mga_mem_size;





	if ( mga_irq != -1 ) {
		tmp = request_irq(mga_irq, mga_handle_irq, SA_INTERRUPT | SA_SHIRQ, "Syncfb Time Base", &mga_irq);
		if ( tmp ) {
			printk(KERN_ERR "syncfb (mga): cannot register irq %d (Err: %d)\n", mga_irq, tmp);
			mga_cleanup();
			return -1;
		} else {
			printk(KERN_DEBUG "syncfb (mga): registered irq %d\n", mga_irq);
		}
	} else {
		printk(KERN_ERR "syncfb (mga): No valid irq was found\n");
		mga_cleanup();
		return -1;
	}

	return 0;
}

static int mga_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	switch ( cmd ) {
		case SYNCFB_VBI:
			writeb( 0x11, mga_mmio_base + CRTCX);
			writeb(0x20, mga_mmio_base + CRTCD );  /* clear 0, enable off */
			debug_simulate_interrupt = 1;
			mga_handle_irq(-1, NULL, NULL);
			return 0;
		
		default:
			return -EINVAL;

	}

}



static syncfb_device_t mga_device = {
	mga_capability,
	mga_enable,
	mga_disable,
	mga_cleanup,
	mga_request_buffer,
	mga_select_buffer,
	mga_config,
	mga_ioctl,	/* ioctl */
	mga_mmap	/* mmap */
};

/* extern */
syncfb_device_t *syncfb_get_matrox_device(void) {
	if ( mga_probe_card() == 0 )  /* no error code */
		return &mga_device;

	return NULL;
}



/* SYNCFB_MATROX_SUPPORT */
#endif


