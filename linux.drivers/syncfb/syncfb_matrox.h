
/*
 *
 * syncfb_matrox.h
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

#ifdef SYNCFB_MATROX_SUPPORT


#define MGA_VIDMEM_SIZE 16 /* ????? */

#ifndef PCI_DEVICE_ID_MATROX_G200_PCI 
#define PCI_DEVICE_ID_MATROX_G200_PCI 0x0520
#endif

#ifndef PCI_DEVICE_ID_MATROX_G200_AGP 
#define PCI_DEVICE_ID_MATROX_G200_AGP 0x0521
#endif

#ifndef PCI_DEVICE_ID_MATROX_G400 
#define PCI_DEVICE_ID_MATROX_G400 0x0525
#endif


typedef union besctl_u {

	uint_32 value;

	struct bits_besctl {
		unsigned besen		:1;
		unsigned res1		:5;
		unsigned besv1srcstp	:1;
		unsigned besv2srcstp	:1;
		unsigned res2		:2;
		unsigned beshfen	:1;
		unsigned besvfen	:1;
		unsigned beshfixc	:1;
		unsigned res3		:3;
		unsigned bescups	:1;
		unsigned bes420pl	:1;
		unsigned besdith	:1;
		unsigned beshmir	:1;
		unsigned besbwen	:1;
		unsigned besblank	:1;
		unsigned res4		:2;
		unsigned besfselm	:1;
		unsigned besfsel	:2;
		unsigned res5		:5;
	} f;

} besctl_t;



typedef union besglobctl_u {

	uint_32 value;

	struct bits_besglobctl {
		unsigned beshzoom	:1;
		unsigned beshzoomf	:1;
		unsigned res1		:1;
		unsigned bescorder	:1;
		unsigned besreghup	:1;
		unsigned bes3plane	:1;
		unsigned besuyvyfmt	:1;
		unsigned besprocamp	:1;
		unsigned besrgbmode	:2;
		unsigned res2		:5;
		unsigned besvcnt	:12;
		unsigned res3		:4;
	} f;

} besglobctl_t;



typedef struct bes_registers_s
{
	//BES Control
	uint_32 besctl;
	//BES Global control
	uint_32 besglobctl;
	//Luma control (brightness and contrast)
	uint_32 beslumactl;
	//Line pitch
	uint_32 bespitch;

	//Buffer A-1 Chroma 3 plane org
	uint_32 besa1c3org;
	//Buffer A-1 Chroma org
	uint_32 besa1corg;
	//Buffer A-1 Luma org
	uint_32 besa1org;

	//Buffer A-2 Chroma 3 plane org
	uint_32 besa2c3org;
	//Buffer A-2 Chroma org
	uint_32 besa2corg;
	//Buffer A-2 Luma org
	uint_32 besa2org;

	//Buffer B-1 Chroma 3 plane org
	uint_32 besb1c3org;
	//Buffer B-1 Chroma org
	uint_32 besb1corg;
	//Buffer B-1 Luma org
	uint_32 besb1org;

	//Buffer B-2 Chroma 3 plane org
	uint_32 besb2c3org;
	//Buffer B-2 Chroma org
	uint_32 besb2corg;
	//Buffer B-2 Luma org
	uint_32 besb2org;

	//BES Horizontal coord
	uint_32 beshcoord;
	//BES Horizontal inverse scaling [5.14]
	uint_32 beshiscal;
	//BES Horizontal source start [10.14] (for scaling)
	uint_32 beshsrcst;
	//BES Horizontal source ending [10.14] (for scaling) 
	uint_32 beshsrcend;
	//BES Horizontal source last 
	uint_32 beshsrclst;

	
	//BES Vertical coord
	uint_32 besvcoord;
	//BES Vertical inverse scaling [5.14]
	uint_32 besviscal;
	//BES Field 1 vertical source last position
	uint_32 besv1srclst;
	//BES Field 1 weight start
	uint_32 besv1wght;
	//BES Field 2 vertical source last position
	uint_32 besv2srclst;
	//BES Field 2 weight start
	uint_32 besv2wght;

} bes_registers_t;


//All register offsets are converted to word aligned offsets (32 bit)
//because we want all our register accesses to be 32 bits
#define VCOUNT      0x1e20

#define PALWTADD      0x3c00 // Index register for X_DATAREG port
#define X_DATAREG     0x3c0a

#define XMULCTRL      0x19
#define BPP_8         0x00
#define BPP_15        0x01
#define BPP_16        0x02
#define BPP_24        0x03
#define BPP_32_DIR    0x04
#define BPP_32_PAL    0x07

#define XCOLMSK       0x40
#define X_COLKEY      0x42
#define XKEYOPMODE    0x51
#define XCOLMSK0RED   0x52
#define XCOLMSK0GREEN 0x53
#define XCOLMSK0BLUE  0x54
#define XCOLKEY0RED   0x55
#define XCOLKEY0GREEN 0x56
#define XCOLKEY0BLUE  0x57

// Backend Scaler registers
#define BESCTL      0x3d20
#define BESGLOBCTL  0x3dc0
#define BESLUMACTL  0x3d40
#define BESPITCH    0x3d24

#define BESA1ORG    0x3d00
#define BESA2ORG    0x3d04
#define BESB1ORG    0x3d08
#define BESB2ORG    0x3d0C

#define BESA1CORG   0x3d10
#define BESA2CORG   0x3d14
#define BESB1CORG   0x3d18
#define BESB2CORG   0x3d1c

#define BESA1C3ORG  0x3d60
#define BESA2C3ORG  0x3d64
#define BESB1C3ORG  0x3d68
#define BESB2C3ORG  0x3d6c


#define BESHCOORD   0x3d28
#define BESHISCAL   0x3d30
#define BESHSRCEND  0x3d3C
#define BESHSRCLST  0x3d50
#define BESHSRCST   0x3d38
#define BESV1WGHT   0x3d48
#define BESV2WGHT   0x3d4c
#define BESV1SRCLST 0x3d54
#define BESV2SRCLST 0x3d58
#define BESVISCAL   0x3d34
#define BESVCOORD   0x3d2c
#define BESSTATUS   0x3dc4

#define CRTCX	    0x1fd4
#define CRTCD	    0x1fd5
#define	IEN	    0x1e1c
#define ICLEAR	    0x1e18
#define STATUS      0x1e14



/* SYNCFB_MATROX_SUPPORT */
#endif


