#ifndef __AF_INTERNAL_H_INCLUDED
#define __AF_INTERNAL_H_INCLUDED 1

// Linked list of audio filters
struct af_instance_t {
    Opaque		unusable;
    const af_info_t*	info;
    unsigned		pin; // personal identification number
    MPXP_Rc		(* __FASTCALL__ config_af)(af_instance_t* af, const af_conf_t* arg);
    MPXP_Rc		(* __FASTCALL__ control_af)(af_instance_t* af, int cmd, any_t* arg);
    void		(* __FASTCALL__ uninit)(af_instance_t* af);
    mp_aframe_t		(* __FASTCALL__ play)(af_instance_t* af,const mp_aframe_t& data);
    any_t*		setup; // setup data for this specific instance and filter
    af_conf_t		conf; // configuration for outgoing data stream
    af_instance_t*	next;
    af_instance_t*	prev;
    any_t*		parent;
    double		delay; // Delay caused by the filter [ms]
    frac_t		mul; /* length multiplier: how much does this instance change
				the length of the buffer. */
};
#endif
