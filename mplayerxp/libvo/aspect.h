#ifndef __ASPECT_H
#define __ASPECT_H
/* Stuff for correct aspect scaling. */
#include <stdint.h>
#include "mp_config.h"

class Aspect : public Opaque {
    public:
	Aspect(float moitor_pixel_aspect);
	virtual ~Aspect();

	void __FASTCALL__ save(uint32_t orgw, uint32_t orgh,
				uint32_t prew, uint32_t preh,
				uint32_t scrw, uint32_t scrh);
	void __FASTCALL__ save_image(uint32_t orgw, uint32_t orgh,
				uint32_t prew, uint32_t preh);
	void __FASTCALL__ save_screen(uint32_t scrw, uint32_t scrh);
	enum zoom_e {
	    NOZOOM=0,
	    ZOOM
	};
	void __FASTCALL__ calc(uint32_t& srcw, uint32_t& srch, zoom_e zoom);
    private:
	uint32_t	orgw; // real width
	uint32_t	orgh; // real height
	uint32_t	prew; // prescaled width
	uint32_t	preh; // prescaled height
	uint32_t	screenw; // horizontal resolution
	uint32_t	screenh; // vertical resolution
	float		monitor_aspect;
	float		monitor_pixel_aspect;
};

#endif

