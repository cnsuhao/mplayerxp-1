#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/* Stuff for correct aspect scaling. */
#include "aspect.h"

//#define ASPECT_DEBUG

#ifdef ASPECT_DEBUG
#include <stdio.h>
#endif
#include "vo_msg.h"

namespace mpxp {

Aspect::Aspect(float mon_pix_aspect) { monitor_pixel_aspect = mon_pix_aspect; }
Aspect::~Aspect() { }

void Aspect::save(uint32_t _orgw, uint32_t _orgh,
		uint32_t _prew, uint32_t _preh,
		uint32_t _scrw, uint32_t _scrh) {
    orgw = _orgw;
    orgh = _orgh;
    prew = _prew;
    preh = _preh;
    screenw = _scrw;
    screenh = _scrh;
    monitor_aspect = monitor_pixel_aspect * screenw / screenh;
}

void Aspect::save_image(uint32_t _orgw, uint32_t _orgh,
				uint32_t _prew, uint32_t _preh) {
    orgw = _orgw;
    orgh = _orgh;
    prew = _prew;
    preh = _preh;
}

void Aspect::save_screen(uint32_t _scrw, uint32_t _scrh) {
    screenw = _scrw;
    screenh = _scrh;
    monitor_aspect = monitor_pixel_aspect * screenw / screenh;
}

/* aspect is called with the source resolution and the
 * resolution, that the scaled image should fit into
 */
void Aspect::calc(uint32_t& srcw, uint32_t& srch, zoom_e zoom) {
    uint32_t tmpw;

#ifdef ASPECT_DEBUG
    MSG_DBG2("aspect(0) fitin: %dx%d zoom: %d \n",screenw,screenh,zoom);
    MSG_DBG2("aspect(1) wh: %dx%d (org: %dx%d)\n",srcw,srch,prew,preh);
#endif
    if(zoom){
	if(prew >= preh) {
	/* Landscape mode */
	    srcw = screenw;
	    srch = (uint32_t)(((float)screenw / (float)prew * (float)preh) *
		    ((float)screenh / ((float)screenw / monitor_aspect)));
	} else {
	/* Portrait mode */
	    srch = screenh;
	    srcw = (uint32_t)(((float)screenh / (float)preh * (float)prew) *
		    ((float)screenw / ((float)screenh * monitor_aspect)));
	}
    } else {
	srcw = prew;
	srch = (uint32_t)((float)preh *
		((float)screenh / ((float)screenw / monitor_aspect)));
    }
    srch+= srch%2; // round
#ifdef ASPECT_DEBUG
    MSG_DBG2("aspect(2) wh: %dx%d (org: %dx%d)\n",srcw,srch,prew,preh);
#endif
    if(srch>screenh || srch<orgh){
	if(zoom)
	    tmpw = (uint32_t)(((float)screenh / (float)preh * (float)prew) *
		    ((float)screenw / ((float)screenh / (1.0/monitor_aspect))));
	else
        tmpw = (uint32_t)((float)prew *
		    ((float)screenw / ((float)screenh / (1.0/monitor_aspect))));
	if(tmpw<=screenw && tmpw>=orgw) {
	    srch = zoom?screenh:preh;
	    srcw = tmpw;
	    srcw+= srcw%2; // round
	}
    }
#ifdef ASPECT_DEBUG
    MSG_DBG2("aspect(3) wh: %dx%d (org: %dx%d)\n",srcw,srch,prew,preh);
#endif
}
} // namesapce mpxp
