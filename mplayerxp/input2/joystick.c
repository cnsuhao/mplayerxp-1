#include "mp_config.h"

#ifdef HAVE_JOYSTICK
#include "joystick.h"
#include "input.h"
#include "osdep/mplib.h"

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

typedef struct priv_s {
    int axis[10];
    int btns;
    int fd;
}priv_t;

#include "in_msg.h"

any_t* mp_input_joystick_open(const char* dev) {
    int l=0;
    int inited = 0;
    struct js_event ev;
    priv_t* priv=mp_mallocz(sizeof(priv_t));

    MSG_INFO("Opening joystick device %s\n",dev ? dev : JS_DEV);

    if((priv->fd=open(dev?dev:JS_DEV,O_RDONLY|O_NONBLOCK))<0) {
	MSG_ERR("Can't open joystick device %s : %s\n",dev ? dev : JS_DEV,strerror(errno));
	mp_free(priv);
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
		MSG_ERR("Error while reading joystick device : %s\n",strerror(errno));
		close(priv->fd);
		mp_free(priv);
		return NULL;
	    }
	    l += r;
	}
	if((unsigned int)l < sizeof(struct js_event)) {
	    if(l > 0) MSG_ERR("Joystick : we loose %d bytes of data\n",l);
	    break;
	}
	ev.type &= ~JS_EVENT_INIT;
	if(ev.type == JS_EVENT_BUTTON) priv->btns |= (ev.value << ev.number);
	if(ev.type == JS_EVENT_AXIS)   priv->axis[ev.number] = ev.value;
    }
    return priv;
}

void mp_input_joystick_close(any_t* ctx) {
    priv_t* priv = (priv_t*)ctx;
    close(priv->fd);
    mp_free(priv);
}

int mp_input_joystick_read(any_t* ctx) {
    priv_t* priv = (priv_t*)ctx;
    struct js_event ev;
    int l=0;

    while((unsigned int)l < sizeof(struct js_event)) {
	int r = read(priv->fd,&ev+l,sizeof(struct js_event)-l);
	if(r <= 0) {
	    if(errno == EINTR) continue;
	    else if(errno == EAGAIN) return MP_INPUT_NOTHING;
	    if(r < 0)	MSG_ERR("Joystick error while reading joystick device : %s\n",strerror(errno));
	    else	MSG_ERR("Joystick error while reading joystick device : EOF\n");
	    return MP_INPUT_DEAD;
	}
	l += r;
    }

    if((unsigned int)l < sizeof(struct js_event)) {
	if(l > 0) MSG_ERR("Joystick : we loose %d bytes of data\n",l);
	return MP_INPUT_NOTHING;
    }

    if(ev.type & JS_EVENT_INIT) {
	MSG_WARN("Joystick : warning init event, we have lost sync with driver\n");
	ev.type &= ~JS_EVENT_INIT;
	if(ev.type == JS_EVENT_BUTTON) {
	    int s = (priv->btns >> ev.number) & 1;
	    if(s == ev.value) // State is the same : ignore
		return MP_INPUT_NOTHING;
	}
	if(ev.type == JS_EVENT_AXIS) {
	    if( (priv->axis[ev.number] == 1 && ev.value > JOY_AXIS_DELTA) ||
		(priv->axis[ev.number] == -1 && ev.value < -JOY_AXIS_DELTA) ||
		(priv->axis[ev.number] == 0 && ev.value >= -JOY_AXIS_DELTA && ev.value <= JOY_AXIS_DELTA))
		    // State is the same : ignore
		    return MP_INPUT_NOTHING;
	}
    }

    if(ev.type & JS_EVENT_BUTTON) {
	priv->btns &= ~(1 << ev.number);
	priv->btns |= (ev.value << ev.number);
	if(ev.value == 1) return ((JOY_BTN0+ev.number) | MP_KEY_DOWN);
	else return (JOY_BTN0+ev.number);
    } else if(ev.type & JS_EVENT_AXIS) {
	if(ev.value < -JOY_AXIS_DELTA && priv->axis[ev.number] != -1) {
	    priv->axis[ev.number] = -1;
	    return (JOY_AXIS0_MINUS+(2*ev.number)) | MP_KEY_DOWN;
	} else if(ev.value > JOY_AXIS_DELTA && priv->axis[ev.number] != 1) {
	    priv->axis[ev.number] = 1;
	    return (JOY_AXIS0_PLUS+(2*ev.number)) | MP_KEY_DOWN;
	} else if(ev.value <= JOY_AXIS_DELTA && ev.value >= -JOY_AXIS_DELTA && priv->axis[ev.number] != 0) {
	    int r = priv->axis[ev.number] == 1 ? JOY_AXIS0_PLUS+(2*ev.number) : JOY_AXIS0_MINUS+(2*ev.number);
	    priv->axis[ev.number] = 0;
	    return r;
	} else
	    return MP_INPUT_NOTHING;
    } else {
	MSG_ERR("Joystick warning unknow event type %d\n",ev.type);
	return MP_INPUT_ERROR;
    }
    return MP_INPUT_NOTHING;
}
#else
any_t* mp_input_joystick_open(const char* dev) { UNUNSED(dev); return NULL; }
void   mp_input_joystick_close(any_t* ctx) { UNUSED(ctx); }
int    mp_input_joystick_read(any_t* ctx) { UNUSED(ctx); return MP_INPUT_NOTHING; }
#endif
