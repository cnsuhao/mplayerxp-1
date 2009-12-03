/*
 *
 * syncfb_generic.c
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

#ifdef SYNCFB_GENERIC_SUPPORT


static int cnt;

static syncfb_capability_t gen_caps = {
	"Generic Syncfb Device (DUMMY!!!)",
	0,
	0,
	0
};

static syncfb_capability_t *gen_capability() {
	return &gen_caps;
}

static void gen_enable() {
	cnt++;
}

static void gen_disable() {
	cnt++;

}

static void gen_cleanup() {
	cnt++;

}

static void gen_request_buffer(syncfb_buffer_info_t *buf) {
	cnt++;

}


static void gen_select_buffer(uint_8 id) {
	cnt++;

}

static int gen_config(uint_8 set, syncfb_config_t *config) {

	if ( set ) {
		printk(KERN_DEBUG "syncfb (gen): Got configuration data\n");

	} else {
		config->syncfb_mode = 0;
		config->fb_screen_size = 0;
		config->src_width = 0;
		config->src_height = 0;
		config->src_palette = 0;
		config->image_xorg = 0;
		config->image_yorg = 0;
		config->image_width = 0;
		config->image_height = 0;
		config->default_repeat = 0;
	}

	return 0;
}

static syncfb_device_t gen_device = {
	gen_capability,
	gen_enable,
	gen_disable,
	gen_cleanup,
	gen_request_buffer,
	gen_select_buffer,
	gen_config,
	NULL, /* ioctl */
	NULL  /* mmap */
};


/* extern */
syncfb_device_t *syncfb_get_generic_device() {
	return &gen_device;
}


#endif
