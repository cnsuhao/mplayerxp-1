#ifndef __VFCAP_H
#define __VFCAP_H 1

/* VFCAP_* values: they are flags, returned by query_format(): */
enum {
    VFCAP_CSP_SUPPORTED		=0x00000001UL, /**< set, if the given colorspace is supported (with or without conversion) */
    VFCAP_CSP_SUPPORTED_BY_HW	=0x00000002UL, /**< set, if the given colorspace is supported _without_ conversion */
    VFCAP_OSD			=0x00000004UL, /**< set if the driver/filter can draw OSD */
    VFCAP_SPU			=0x00000008UL, /**< set if the driver/filter can handle compressed SPU stream */
    VFCAP_HWSCALE_UP		=0x00000010UL, /**< scaling up/down by hardware, or software */
    VFCAP_HWSCALE_DOWN		=0x00000020UL, /**< scaling up/down by hardware, or software */
    VFCAP_SWSCALE		=0x00000040UL,
    VFCAP_FLIP			=0x00000080UL, /**< driver/filter can do vertical flip (upside-down) */
    VFCAP_TIMER			=0x00000100UL, /**< driver/hardware handles timing (blocking) */
    VFCAP_FLIPPED		=0x00000200UL, /**< driver _always_ flip image upside-down (for ve_vfw) */
    VFCAP_ACCEPT_STRIDE		=0x00000400UL, /**< vf filter: accepts stride (put_image) */
    VFCAP_POSTPROC		=0x00000800UL /**< filter does postprocessing (so you shouldn't scale/filter image before it) */
};
#endif
