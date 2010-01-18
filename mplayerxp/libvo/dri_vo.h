/*
    This interface provides new DRI (Direct rendering interface)
    for video output library.
    This interface still stays in initial state.
    Will be expanded in the future
*/
#ifndef __DRI_VO_INCLUDED
#define __DRI_VO_INCLUDED 1

/*---------- LIB_DRI ----------------*/
/* 
  Note: Each dri voctl call has following format: 
    voctl(call_num,void *);
*/

#define DRI_CAP_TEMP_VIDEO	0x00000000UL	/**< Means: video buffer was allocated in RAM */
#define DRI_CAP_VIDEO_MMAPED	0x00000001UL	/**< Means: surface provides DGA */
#define DRI_CAP_UPSCALER	0x00000010UL	/**< Driver supports upscaling */
#define DRI_CAP_DOWNSCALER	0x00000020UL	/**< Driver supports downscaling */
#define DRI_CAP_HORZSCALER	0x00000040UL	/**< Driver supports horizontal scaling */
#define DRI_CAP_VERTSCALER	0x00000080UL	/**< Driver supports vertical scaling */
#define DRI_CAP_HWOSD		0x00000100UL	/**< Driver supports OSD painting */
#define DRI_CAP_BUSMASTERING	0x80000000UL	/**< Means: final video buffer but allocated in RAM */

typedef struct dri_surface_cap_s
{
    unsigned	caps;		/**< Capabilities of surface (see DRI_CAP_* for detail) */
    unsigned	fourcc;		/**< real fourcc of vo2 surface */
    unsigned	width,height;	/**< specify total dimension of surface */
    unsigned	x,y,w,h;	/**< specify movie position within surface */
    unsigned	strides[4];	/**< drv->app:specify strides of each plane */
}dri_surface_cap_t;

#define DRI_GET_SURFACE_CAPS	16	/**< Query capabilties of surfaces. We assume that all surfaces have the same capabilities */

#define MAX_DRI_BUFFERS 1024		/**< Maximal number of surfaces */
/** Contains surface address */
typedef struct dri_surface_s
{
    unsigned idx;		/**< app->drv:specify number of surface (0 default for single buffering) */
    uint8_t* planes[4];		/**< drv->app:specify planes (include alpha channel) */
}dri_surface_t;

#define DRI_GET_SURFACE		17	/**< Query surface by index */

#endif
