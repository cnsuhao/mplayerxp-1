#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#ifdef HAVE_JOYSTICK
#include "joystick.h"
#include "input.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifndef JOY_AXIS_DELTA
#define JOY_AXIS_DELTA 500
#endif

#ifndef JS_DEV
#define JS_DEV "/dev/input/js0"
#endif

#include <linux/joystick.h>

#include "in_msg.h"

namespace mpxp {
typedef struct priv_s {
    int axis[10];
    int btns;
    int fd;
}priv_t;


any_t* mp_input_joystick_open(const char* dev) {
    int l=0;
    int inited = 0;
    struct js_event ev;
    priv_t* priv=new(zeromem) priv_t;

    mpxp_info<<"Opening joystick device:"<<(dev?dev:JS_DEV)<<std::endl;

    if((priv->fd=open(dev?dev:JS_DEV,O_RDONLY|O_NONBLOCK))<0) {
	mpxp_err<<"Can't open joystick device: "<<(dev?dev:JS_DEV)<<" : "<<strerror(errno)<<std::endl;
	delete priv;
	return NULL;
    }

    while(! inited) {
	l = 0;
	while((unsigned int)l < sizeof(struct js_event)) {
	    int r = read(priv->fd,((char*)&ev)+l,sizeof(struct js_event)-l);
	    if(r < 0) {
		if(errno == EINTR) continue;
		else if(errno == EAGAIN) {
		    inited = 1;
		    break;
		}
		mpxp_err<<"Error while reading joystick device: "<<strerror(errno)<<std::endl;
		close(priv->fd);
		delete priv;
		return NULL;
	    }
	    l += r;
	}
	if((unsigned int)l < sizeof(struct js_event)) {
	    if(l > 0) mpxp_err<<"Joystick: we loose "<<l<<"bytes of data"<<std::endl;
	    break;
	}
	ev.type &= ~JS_EVENT_INIT;
	if(ev.type == JS_EVENT_BUTTON) priv->btns |= (ev.value << ev.number);
	if(ev.type == JS_EVENT_AXIS)   priv->axis[ev.number] = ev.value;
    }
    return priv;
}

void mp_input_joystick_close(any_t* ctx) {
    priv_t* priv = reinterpret_cast<priv_t*>(ctx);
    close(priv->fd);
    delete priv;
}

int mp_input_joystick_read(any_t* ctx) {
    priv_t& priv = *reinterpret_cast<priv_t*>(ctx);
    struct js_event ev;
    int l=0;

    while((unsigned int)l < sizeof(struct js_event)) {
	int r = read(priv.fd,&ev+l,sizeof(struct js_event)-l);
	if(r <= 0) {
	    if(errno == EINTR) continue;
	    else if(errno == EAGAIN) return MP_INPUT_NOTHING;
	    if(r < 0)	mpxp_err<<"Joystick error while reading joystick device: "<<strerror(errno)<<std::endl;
	    else	mpxp_err<<"Joystick error while reading joystick device: EOF"<<std::endl;
	    return MP_INPUT_DEAD;
	}
	l += r;
    }

    if((unsigned int)l < sizeof(struct js_event)) {
	if(l > 0) mpxp_err<<"Joystick: we loose "<<l<<"bytes of data"<<std::endl;
	return MP_INPUT_NOTHING;
    }

    if(ev.type & JS_EVENT_INIT) {
	mpxp_warn<<"Joystick: warning init event, we have lost sync with driver"<<std::endl;
	ev.type &= ~JS_EVENT_INIT;
	if(ev.type == JS_EVENT_BUTTON) {
	    int s = (priv.btns >> ev.number) & 1;
	    if(s == ev.value) // State is the same : ignore
		return MP_INPUT_NOTHING;
	}
	if(ev.type == JS_EVENT_AXIS) {
	    if( (priv.axis[ev.number] == 1 && ev.value > JOY_AXIS_DELTA) ||
		(priv.axis[ev.number] == -1 && ev.value < -JOY_AXIS_DELTA) ||
		(priv.axis[ev.number] == 0 && ev.value >= -JOY_AXIS_DELTA && ev.value <= JOY_AXIS_DELTA))
		    // State is the same : ignore
		    return MP_INPUT_NOTHING;
	}
    }

    if(ev.type & JS_EVENT_BUTTON) {
	priv.btns &= ~(1 << ev.number);
	priv.btns |= (ev.value << ev.number);
	if(ev.value == 1) return (JOY_BTN0+ev.number)|MP_KEY_DOWN;
	else return JOY_BTN0+ev.number;
    } else if(ev.type & JS_EVENT_AXIS) {
	if(ev.value < -JOY_AXIS_DELTA && priv.axis[ev.number] != -1) {
	    priv.axis[ev.number] = -1;
	    return (JOY_AXIS0_MINUS+(2*ev.number))|MP_KEY_DOWN;
	} else if(ev.value > JOY_AXIS_DELTA && priv.axis[ev.number] != 1) {
	    priv.axis[ev.number] = 1;
	    return (JOY_AXIS0_PLUS+(2*ev.number))|MP_KEY_DOWN;
	} else if(ev.value <= JOY_AXIS_DELTA && ev.value >= -JOY_AXIS_DELTA && priv.axis[ev.number] != 0) {
	    int r = priv.axis[ev.number] == 1 ? JOY_AXIS0_PLUS+(2*ev.number) : JOY_AXIS0_MINUS+(2*ev.number);
	    priv.axis[ev.number] = 0;
	    return r;
	} else
	    return MP_INPUT_NOTHING;
    } else {
	mpxp_err<<"Joystick warning unknow event type "<<ev.type<<std::endl;
	return MP_INPUT_ERROR;
    }
    return MP_INPUT_NOTHING;
}
} // namespace mpxp
#else
namespace mpxp {
any_t* mp_input_joystick_open(const char* dev) { UNUNSED(dev); return NULL; }
void   mp_input_joystick_close(any_t* ctx) { UNUSED(ctx); }
int    mp_input_joystick_read(any_t* ctx) { UNUSED(ctx); return MP_INPUT_NOTHING; }
} // namespace mpxp
#endif
