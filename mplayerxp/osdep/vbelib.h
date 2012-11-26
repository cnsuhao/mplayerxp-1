/*
   VESA VBE 2.0 compatible structures and definitions.
   You can redistribute this file under terms and conditions
   of GNU General Public licence v2.
   Written by Nickols_K <nickols_k@mail.ru>
*/
#ifndef __VESA_VBELIB_INCLUDED__
#define __VESA_VBELIB_INCLUDED__ 1

#include "mp_config.h"

namespace mpxp {
/* Note: every pointer within structures is 32-bit protected mode pointer.
   So you don't need to convert it from real mode. */

    typedef struct tagFarPtr {
	unsigned short off;
	unsigned short seg;
    }FarPtr;

    enum {
	VBE_DAC_8BIT	=(1 << 0),
	VBE_NONVGA_CRTC	=(1 << 1),
	VBE_SNOWED_RAMDAC=(1 << 2),
	VBE_STEREOSCOPIC=(1 << 3),
	VBE_STEREO_EVC	=(1 << 4)
    };

    struct VbeInfoBlock {
	char          VESASignature[4]; /* 'VESA' 4 byte signature */
	short         VESAVersion;      /* VBE version number */
	char *        OemStringPtr;     /* Pointer to OEM string */
	long          Capabilities;     /* Capabilities of video card */
	unsigned short* VideoModePtr;   /* Pointer to supported modes */
	short         TotalMemory;      /* Number of 64kb memory blocks */
	/* VBE 2.0 and above */
	short         OemSoftwareRev;
	char *        OemVendorNamePtr;
	char *        OemProductNamePtr;
	char *        OemProductRevPtr;
	char          reserved[222];
	char          OemData[256];     /* Pad to 512 byte block size */
    }__attribute__ ((packed));

    inline FarPtr VirtToPhys(any_t*ptr) {
	FarPtr retval;
	retval.seg = ((unsigned long)ptr) >> 4;
	retval.off = ((unsigned long)ptr) & 0x0f;
	return retval;
    }

    inline unsigned short VirtToPhysSeg(any_t*ptr) {
	return ((unsigned long)ptr) >> 4;
    }

    inline unsigned short VirtToPhysOff(any_t*ptr) {
	return ((unsigned long)ptr) & 0x0f;
    }

    inline any_t* PhysToVirt(FarPtr ptr) {
	return (any_t*)((ptr.seg << 4) | ptr.off);
    }

    inline any_t* PhysToVirtSO(unsigned short seg,unsigned short off) {
	return (any_t*)((seg << 4) | off);
    }

    enum {
	MODE_ATTR_MODE_SUPPORTED=(1 << 0),
	MODE_ATTR_TTY		=(1 << 2),
	MODE_ATTR_COLOR		=(1 << 3),
	MODE_ATTR_GRAPHICS	=(1 << 4),
	MODE_ATTR_NOT_VGA	=(1 << 5),
	MODE_ATTR_NOT_WINDOWED	=(1 << 6),
	MODE_ATTR_LINEAR	=(1 << 7),
	MODE_ATTR_DOUBLESCAN	=(1 << 8),
	MODE_ATTR_INTERLACE	=(1 << 9),
	MODE_ATTR_TRIPLEBUFFER	=(1 << 10),
	MODE_ATTR_STEREOSCOPIC	=(1 << 11),
	MODE_ATTR_DUALDISPLAY	=(1 << 12),

	MODE_WIN_RELOCATABLE	=(1 << 0),
	MODE_WIN_READABLE	=(1 << 1),
	MODE_WIN_WRITEABLE	=(1 << 2)
    };

    /* SuperVGA mode information block */
    struct VesaModeInfoBlock {
	unsigned short ModeAttributes;      /* 00: Mode attributes */
	unsigned char  WinAAttributes;      /* 02: Window A attributes */
	unsigned char  WinBAttributes;      /* 03: Window B attributes */
	unsigned short WinGranularity;      /* 04: Window granularity in k */
	unsigned short WinSize;             /* 06: Window size in k */
	unsigned short WinASegment;         /* 08: Window A segment */
	unsigned short WinBSegment;         /* 0A: Window B segment */
	FarPtr         WinFuncPtr;          /* 0C: 16-bit far pointer to window function */
	unsigned short BytesPerScanLine;    /* 10: Bytes per scanline */
	/*  VBE 1.2 and above */
	unsigned short XResolution;         /* 12: Horizontal resolution */
	unsigned short YResolution;         /* 14: Vertical resolution */
	unsigned char  XCharSize;           /* 16: Character cell width */
	unsigned char  YCharSize;           /* 17: Character cell height */
	unsigned char  NumberOfPlanes;      /* 18: Number of memory planes */
	unsigned char  BitsPerPixel;        /* 19: Bits per pixel */
	unsigned char  NumberOfBanks;       /* 1A: Number of CGA style banks */
	unsigned char  MemoryModel;         /* 1B: Memory model type */
	unsigned char  BankSize;            /* 1C: Size of CGA style banks */
	unsigned char  NumberOfImagePages;  /* 1D: Number of images pages */
	unsigned char  res1;                /* 1E: Reserved */
	/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
	unsigned char  RedMaskSize;         /* 1F: Size of direct color red mask */
	unsigned char  RedFieldPosition;    /* 20: Bit posn of lsb of red mask */
	unsigned char  GreenMaskSize;       /* 21: Size of direct color green mask */
	unsigned char  GreenFieldPosition;  /* 22: Bit posn of lsb of green mask */
	unsigned char  BlueMaskSize;        /* 23: Size of direct color blue mask */
	unsigned char  BlueFieldPosition;   /* 24: Bit posn of lsb of blue mask */
	unsigned char  RsvdMaskSize;        /* 25: Size of direct color res mask */
	unsigned char  RsvdFieldPosition;   /* 26: Bit posn of lsb of res mask */
	unsigned char  DirectColorModeInfo; /* 27: Direct color mode attributes */
	/* VBE 2.0 and above */
	unsigned long  PhysBasePtr;         /* 28: physical address for flat memory frame buffer. (Should be converted to linear before using) */
	unsigned short res3[3];             /* 2C: Reserved - always set to 0 */
	/* VBE 3.0 and above */
	unsigned short LinBytesPerScanLine;  /* 32: bytes per scan line for linear modes */
	unsigned char  BnkNumberOfImagePages;/* 34: number of images for banked modes */
	unsigned char  LinNumberOfImagePages;/* 35: number of images for linear modes */
	unsigned char  LinRedMaskSize;       /* 36: size of direct color red mask (linear modes) */
	unsigned char  LinRedFieldPosition;  /* 37: bit position of lsb of red mask (linear modes) */
	unsigned char  LinGreenMaskSize;     /* 38: size of direct color green mask (linear modes) */
	unsigned char  LinGreenFieldPosition;/* 39: bit position of lsb of green mask (linear modes) */
	unsigned char  LinBlueMaskSize;      /* 40: size of direct color blue mask (linear modes) */
	unsigned char  LinBlueFieldPosition; /* 41: bit position of lsb of blue mask (linear modes) */
	unsigned char  LinRsvdMaskSize;      /* 42: size of direct color reserved mask (linear modes) */
	unsigned char  LinRsvdFieldPosition; /* 43: bit position of lsb of reserved mask (linear modes) */
	unsigned long  MaxPixelClock;        /* 44: maximum pixel clock (in Hz) for graphics mode */
	char           res4[189];            /* 48: remainder of ModeInfoBlock */
    }__attribute__ ((packed));

    typedef enum {
	memText= 0,
	memCGA = 1,
	memHercules = 2,
	memPL  = 3, /* Planar memory model */
	memPK  = 4, /* Packed pixel memory model */
	mem256 = 5,
	memRGB = 6, /* Direct color RGB memory model */
	memYUV = 7, /* Direct color YUV memory model */
    } memModels;

    struct VesaCRTCInfoBlock {
	unsigned short hTotal;     /* Horizontal total in pixels */
	unsigned short hSyncStart; /* Horizontal sync start in pixels */
	unsigned short hSyncEnd;   /* Horizontal sync end in pixels */
	unsigned short vTotal;     /* Vertical total in lines */
	unsigned short vSyncStart; /* Vertical sync start in lines */
	unsigned short vSyncEnd;   /* Vertical sync end in lines */
	unsigned char  Flags;      /* Flags (Interlaced, Double Scan etc) */
	unsigned long  PixelClock; /* Pixel clock in units of Hz */
	unsigned short RefreshRate;/* Refresh rate in units of 0.01 Hz*/
	unsigned char  Reserved[40];/* remainder of CRTCInfoBlock*/
    }__attribute__ ((packed));

    enum {
	VESA_CRTC_DOUBLESCAN	=0x01,
	VESA_CRTC_INTERLACED	=0x02,
	VESA_CRTC_HSYNC_NEG	=0x04,
	VESA_CRTC_VSYNC_NEG	=0x08,

	VESA_MODE_CRTC_REFRESH	=(1 << 11),
	VESA_MODE_USE_LINEAR	=(1 << 14),
	VESA_MODE_NOT_CLEAR	=(1 << 15)
    };

    /* This will contain accesible 32-bit protmode pointers */
    struct VesaProtModeInterface {
	void (*SetWindowCall)(void);
	void (*SetDisplayStart)(void);
	void (*SetPaletteData)(void);
	unsigned short * iopl_ports;
    };

    /*
    All functions below return:
    0      if succesful
    0xffff if vm86 syscall error occurs
    0x4fxx if VESA error occurs
    */
    enum {
	VBE_OK			=0,
	VBE_VM86_FAIL		=-1,
	VBE_OUT_OF_DOS_MEM	=-2,
	VBE_OUT_OF_MEM		=-3,
	VBE_BROKEN_BIOS		=-4,
	VBE_VESA_ERROR_MASK	=0x004f,
	VBE_VESA_ERRCODE_MASK	=0xff00
    };
    int vbeInit( void );
    int vbeDestroy( void );

    int vbeGetControllerInfo(struct VbeInfoBlock *);
    int vbeGetModeInfo(unsigned mode,struct VesaModeInfoBlock *);
    int vbeSetMode(unsigned mode,struct VesaCRTCInfoBlock *);
    int vbeGetMode(unsigned *mode);
    int vbeSaveState(any_t**data); /* note never copy this data */
    int vbeRestoreState(any_t*data);
    int vbeGetWindow(unsigned *win_num); /* win_A=0 or win_B=1 */
    int vbeSetWindow(unsigned win_num,unsigned win_gran);
    int vbeGetScanLineLength(unsigned *num_pixels,unsigned *num_bytes);
    int vbeGetMaxScanLines(unsigned *num_pixels,unsigned *num_bytes, unsigned *num_lines);
    int vbeSetScanLineLength(unsigned num_pixels);
    int vbeSetScanLineLengthB(unsigned num_bytes);
    int vbeGetDisplayStart(unsigned *pixel_num,unsigned *scan_line);
    int vbeSetDisplayStart(unsigned long offset, int vsync);
    int vbeSetScheduledDisplayStart(unsigned long offset, int vsync);
    /*
    Func 0x08-0x09:
    Support of palette currently is not implemented.
    */
    int vbeGetProtModeInfo(struct VesaProtModeInterface *);

    /* Standard VGA stuff */
    int vbeWriteString(int x, int y, int attr,const char *str);

    /* Misc stuff (For portability only) */
    any_t* vbeMapVideoBuffer(unsigned long phys_addr,unsigned long size);
    void vbeUnmapVideoBuffer(unsigned long linear_addr,unsigned long size);
} // namespace mpxp
#endif
