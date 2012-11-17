#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp_config.h"
#include "af.h"
#include "help_mp.h"
#include "libao2/audio_out.h"
#include "osdep/mplib.h"
#include "pp_msg.h"

extern ao_data_t* ao_data;

// Static list of filters
extern const af_info_t af_info_ao;
extern const af_info_t af_info_center;
extern const af_info_t af_info_channels;
extern const af_info_t af_info_comp;
extern const af_info_t af_info_crystality;
extern const af_info_t af_info_dummy;
extern const af_info_t af_info_delay;
extern const af_info_t af_info_dyn;
extern const af_info_t af_info_echo3d;
extern const af_info_t af_info_equalizer;
extern const af_info_t af_info_eq;
extern const af_info_t af_info_export;
extern const af_info_t af_info_extrastereo;
extern const af_info_t af_info_format;
extern const af_info_t af_info_gate;
extern const af_info_t af_info_hrtf;
extern const af_info_t af_info_lp;
extern const af_info_t af_info_resample;
extern const af_info_t af_info_pan;
extern const af_info_t af_info_sub;
extern const af_info_t af_info_surround;
extern const af_info_t af_info_volnorm;
extern const af_info_t af_info_volume;
extern const af_info_t af_info_ffenc;
extern const af_info_t af_info_raw;
extern const af_info_t af_info_karaoke;
extern const af_info_t af_info_sinesuppress;
extern const af_info_t af_info_scaletempo;
extern const af_info_t af_info_null;

static const af_info_t* filter_list[]={
   &af_info_ao,
   &af_info_center,
   &af_info_channels,
   &af_info_comp,
   &af_info_crystality,
   &af_info_dummy,
   &af_info_delay,
   &af_info_dyn,
   &af_info_echo3d,
   &af_info_equalizer,
   &af_info_eq,
#ifdef HAVE_SYS_MMAN_H
   &af_info_export,
#endif
   &af_info_extrastereo,
   &af_info_format,
   &af_info_gate,
   &af_info_hrtf,
   &af_info_karaoke,
   &af_info_lp,
   &af_info_pan,
   &af_info_resample,
   &af_info_sinesuppress,
   &af_info_sub,
   &af_info_surround,
   &af_info_volnorm,
   &af_info_volume,
   &af_info_ffenc,
   &af_info_scaletempo,
   &af_info_raw,
   &af_info_null,
   NULL
};

// CPU speed
int* af_cpu_speed = NULL;

/* Find a filter in the static list of filters using it's name. This
   function is used internally */
static const af_info_t* __FASTCALL__ af_find(const char* name)
{
  int i=0;
  while(filter_list[i]!=&af_info_null){
    if(!strcmp(filter_list[i]->name,name))
      return filter_list[i];
    i++;
  }
  MSG_ERR("Couldn't find audio filter '%s'\n",name);
  return NULL;
}

/* Find filter in the dynamic filter list using it's name This
   function is used for finding already initialized filters */
static af_instance_t* __FASTCALL__ af_get(const af_stream_t* s,const char* name)
{
  af_instance_t* af=s->first;
  // Find the filter
  while(af != NULL){
    if(!strcmp(af->info->name,name))
      return af;
    af=af->next;
  }
  return NULL;
}

/*/ Function for creating a new filter of type name. The name may
  contain the commandline parameters for the filter */
static af_instance_t* __FASTCALL__ af_create(af_stream_t* s,const char* name)
{
  char* cmdline = mp_strdup(name);

  // Allocate space for the new filter and reset all pointers
  af_instance_t* _new=new(zeromem) af_instance_t;
  if(!_new){
    MSG_ERR(MSGTR_OutOfMemory);
    return NULL;
  }
  SECURE_NAME9(rnd_fill)(_new->antiviral_hole,offsetof(af_instance_t,pin)-offsetof(af_instance_t,antiviral_hole));
  _new->pin=AF_PIN;
  _new->parent=s;
  // Check for commandline parameters
  strsep(&cmdline, "=");

  // Find filter from name
  if(NULL == (_new->info=af_find(name))) {
    delete _new;
    delete cmdline;
    return NULL;
  }
  /* Make sure that the filter is not already in the list if it is
     non-reentrant */
  if(_new->info->flags & AF_FLAGS_NOT_REENTRANT){
    if(af_get(s,name)){
      MSG_ERR("[libaf] There can only be one instance of"
	     " the filter '%s' in each stream\n",name);
	delete _new;
	delete cmdline;
	return NULL;
    }
  }

  MSG_V("[libaf] Adding filter %s \n",name);

  // Initialize the new filter
  if(MPXP_Ok == _new->info->open(_new) &&
     MPXP_Error < _new->control(_new,AF_CONTROL_POST_CREATE,&s->cfg)){
    if(cmdline){
      if(MPXP_Error<_new->control(_new,AF_CONTROL_COMMAND_LINE,cmdline)) {
	delete cmdline;
	return _new;
      }
    }
    else {
	delete cmdline;
        return _new;
    }
  }

  delete cmdline;
  delete _new;
  MSG_ERR("[libaf] Couldn't create or open audio filter '%s'\n", name);
  return NULL;
}

/* Create and insert a new filter of type name before the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
static af_instance_t* __FASTCALL__ af_prepend(af_stream_t* s, af_instance_t* af,const char* name)
{
  // Create the _new filter and make sure it is OK
  af_instance_t* _new=af_create(s,name);
  MSG_DBG2("af_prepend %s\n",name);
  if(!_new)
    return NULL;
  // Update pointers
  _new->next=af;
  if(af){
    _new->prev=af->prev;
    af->prev=_new;
  }
  else
    s->last=_new;
  if(_new->prev)
    _new->prev->next=_new;
  else
    s->first=_new;
  return _new;
}

/* Create and insert a _new filter of type name after the filter in the
   argument. This function can be called during runtime, the return
   value is the _new filter */
static af_instance_t* af_append(af_stream_t* s, af_instance_t* af,const char* name)
{
  // Create the _new filter and make sure it is OK
  af_instance_t* _new=af_create(s,name);
  MSG_DBG2("af_append %s\n",name);
  if(!_new)
    return NULL;
  // Update pointers
  _new->prev=af;
  if(af){
    _new->next=af->next;
    af->next=_new;
  }
  else
    s->first=_new;
  if(_new->next)
    _new->next->prev=_new;
  else
    s->last=_new;
  return _new;
}

// Uninit and remove the filter "af"
static void af_remove(af_stream_t* s, af_instance_t* af)
{
  if(!af) return;

  // Print friendly message
  MSG_V("[libaf] Removing filter %s \n",af->info->name);

  // Notify filter before changing anything
  af->control(af,AF_CONTROL_PRE_DESTROY,0);

  // Detach pointers
  if(af->prev)
    af->prev->next=af->next;
  else
    s->first=af->next;
  if(af->next)
    af->next->prev=af->prev;
  else
    s->last=af->prev;

  // Uninitialize af and mp_free memory
  af->uninit(af);
  delete af;
}

/* Reinitializes all filters downstream from the filter given in the
   argument the return value is MPXP_Ok if success and MPXP_Error if
   failure */
static int af_reinit(af_stream_t* s, af_instance_t* af)
{
    if(!af) return MPXP_Error;

    MSG_DBG2("af_reinit()\n");
    do {
	af_conf_t in; // Format of the input to current filter
	int rv=0; // Return value

	// Check if this is the first filter
	if(!af->prev)	memcpy(&in,&(s->input),sizeof(af_conf_t));
	else		memcpy(&in,&af->prev->conf,sizeof(af_conf_t));

	rv = af->config(af,&in);
	memcpy(&in,&af->conf,sizeof(af_conf_t));
	switch(rv){
	    case MPXP_Ok: break;
	    case MPXP_False:{ // Configuration filter is needed
		// Do auto insertion only if force is not specified
		if((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE){
		    af_instance_t* _new = NULL;
		    // Insert channels filter
		    if((af->prev?af->prev->conf.nch:s->input.nch) != in.nch){
			if(NULL == (_new = af_prepend(s,af,"channels"))) return MPXP_Error;
			if(MPXP_Ok != (rv = _new->control(_new,AF_CONTROL_CHANNELS,&in.nch))) return rv;
			// Initialize channels filter
			if(!_new->prev) memcpy(&in,&(s->input),sizeof(af_conf_t));
			else		memcpy(&in,&_new->prev->conf,sizeof(af_conf_t));
			if(MPXP_Ok != (rv = _new->config(_new,&in))) return rv;
		    }
		    // Insert rate filter
		    if((af->prev?af->prev->conf.rate:s->input.rate) != in.rate){
			if(NULL == (_new = af_prepend(s,af,"resample"))) return MPXP_Error;
			if(MPXP_Ok != (rv = _new->control(_new,AF_CONTROL_RESAMPLE_RATE,&in.rate))) return rv;
			// Initialize channels filter
			if(!_new->prev) memcpy(&in,&(s->input),sizeof(af_conf_t));
			else		memcpy(&in,&_new->prev->conf,sizeof(af_conf_t));
			if(MPXP_Ok != (rv = _new->config(_new,&in))) {
			    af_instance_t* af2 = af_prepend(s,af,"format");
			    // Init the _new filter
			    if(af2) {
				if((MPXP_Ok != af2->control(af2,AF_CONTROL_FORMAT,&in.format))) return -1;
				if(MPXP_Ok != af_reinit(s,af2)) return MPXP_Error;
				rv = _new->config(_new,&in);
			    }
			    return rv;
			}
		    }
		    // Insert format filter
		    if(((af->prev?af->prev->conf.format:s->input.format) != in.format)) {
			if(NULL == (_new = af_prepend(s,af,"format"))) return MPXP_Error;
			if(MPXP_Ok != (rv = _new->control(_new,AF_CONTROL_FORMAT,&in.format))) return rv;
			// Initialize format filter
			if(!_new->prev) memcpy(&in,&(s->input),sizeof(af_conf_t));
			else		memcpy(&in,&_new->prev->conf,sizeof(af_conf_t));
			if(MPXP_Ok != (rv = _new->config(_new,&in))) return rv;
		    }
		    if(!_new){ // Should _never_ happen
			MSG_ERR("[libaf] Unable to correct audio format. "
			    "This error should never occur, please send bugreport.\n");
			return MPXP_Error;
		    }
		    af=_new;
		}
		break;
	    }
	    case MPXP_Detach:{ // Filter is redundant and wants to be unloaded
		// Do auto remove only if force is not specified
		if((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE){
		    af_instance_t* aft=af->prev;
		    af_remove(s,af);
		    if(aft) af=aft;
		    else    af=s->first; // Restart configuration
		}
		break;
	    }
	    default:
		MSG_ERR("[libaf] Reinitialization did not work, audio"
			" filter '%s' returned error code %i for r=%i c=%i fmt=%x\n",af->info->name,rv,in.rate,in.nch,in.format);
		return MPXP_Error;
	}
	// Check if there are any filters left in the list
	if(NULL == af){
	    if(!(af=af_append(s,s->first,"dummy")))
		return -1;
	} else af=af->next;
    }while(af);
    return MPXP_Ok;
}

// Uninit and remove all filters
void af_uninit(af_stream_t* s)
{
  while(s->first)
    af_remove(s,s->first);
}

/* Initialize the stream "s". This function creates a _new filter list
   if necessary according to the values set in input and output. Input
   and output should contain the format of the current movie and the
   formate of the preferred output respectively. The function is
   reentrant i.e. if called with an already initialized stream the
   stream will be reinitialized. If the binary parameter
   "force_output" is set, the output format will be converted to the
   format given in "s", otherwise the output fromat in the last filter
   will be copied "s". The return value is 0 if success and -1 if
   failure */
MPXP_Rc RND_RENAME7(af_init)(af_stream_t* s, int force_output)
{
  char *af_name,*af_next;
  // Sanity check
  if(!s) return MPXP_False;

  // Figure out how fast the machine is
  if(AF_INIT_AUTO == (AF_INIT_TYPE_MASK & s->cfg.force))
    s->cfg.force = (s->cfg.force & ~AF_INIT_TYPE_MASK) | AF_INIT_TYPE();

  // Check if this is the first call
  if(!s->first){
    // Add all filters in the list (if there are any)
    if(!s->cfg.list){      // To make automatic format conversion work
      if(!af_append(s,s->first,"ao2"))
	return MPXP_False;
    }
    else{
      af_name=s->cfg.list;
      while(af_name){
	af_next=strchr(af_name,',');
	if(af_next) { *af_next=0; af_next++; }
	if(!af_append(s,s->last,af_name))
	    return MPXP_False;
	af_name=af_next;
      }
    }
  }
  if(strcmp(s->last->info->name,"ao2")!=0) if(!af_append(s,s->last,"ao2")) return MPXP_False;

  // Init filters
  if(MPXP_Ok != af_reinit(s,s->first))
    return MPXP_False;

  // If force_output isn't set do not compensate for output format
  if(!force_output){
    memcpy(&s->output, &s->last->conf, sizeof(af_conf_t));
    return MPXP_Ok;
  }

  // Check output format
  if((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE){
    af_instance_t* af = NULL; // New filter
    // Check output frequency if not OK fix with resample
    if(s->last->conf.rate!=s->output.rate){
      if(NULL==(af=af_get(s,"resample"))){
	if((AF_INIT_TYPE_MASK & s->cfg.force) == AF_INIT_SLOW){
	  if(!strcmp(s->first->info->name,"format"))
	    af = af_append(s,s->first,"resample");
	  else
	    af = af_prepend(s,s->first,"resample");
	}
	else{
	  if(!strcmp(s->last->info->name,"format"))
	    af = af_prepend(s,s->last,"resample");
	  else
	    af = af_append(s,s->last,"resample");
	}
      }
      // Init the _new filter
      if(!af || (MPXP_Ok != af->control(af,AF_CONTROL_RESAMPLE_RATE,
				      &(s->output.rate))))
	return MPXP_False;
      // Use lin int if the user wants fast
      if ((AF_INIT_TYPE_MASK & s->cfg.force) == AF_INIT_FAST) {
	char args[32];
	sprintf(args, "%d:0:0", s->output.rate);
	af->control(af, AF_CONTROL_COMMAND_LINE, args);
      }
      if(MPXP_Ok != af_reinit(s,af))
	return MPXP_False;
    }

    // Check number of output channels fix if not OK
    // If needed always inserted last -> easy to screw up other filters
    if(s->last->conf.nch!=s->output.nch){
      if(!strcmp(s->last->info->name,"format"))
	af = af_prepend(s,s->last,"channels");
      else
	af = af_append(s,s->last,"channels");
      // Init the _new filter
      if(!af || (MPXP_Ok != af->control(af,AF_CONTROL_CHANNELS,&(s->output.nch))))
	return MPXP_False;
      if(MPXP_Ok != af_reinit(s,af))
	return MPXP_False;
    }

    // Check output format fix if not OK
    if((s->last->conf.format != s->output.format)){
      if(strcmp(s->last->info->name,"format"))
	af = af_append(s,s->last,"format");
      else
	af = s->last;
      // Init the _new filter
      if(!af ||(MPXP_Ok != af->control(af,AF_CONTROL_FORMAT,&s->output.format)))
	return MPXP_False;
      if(MPXP_Ok != af_reinit(s,af))
	return MPXP_False;
    }

    // Re init again just in case
    if(MPXP_Ok != af_reinit(s,s->first))
      return MPXP_False;

    if((s->last->conf.format != s->output.format) ||
       (s->last->conf.nch    != s->output.nch)    ||
       (s->last->conf.rate   != s->output.rate))  {
      // Something is stuffed audio out will not work
      MSG_ERR("[libaf] Unable to setup filter system can not"
	     " meet sound-card demands, please send bugreport. \n");
      af_uninit(s);
      return MPXP_False;
    }
  }
  return MPXP_Ok;
}

/* Add filter during execution. This function adds the filter "name"
   to the stream s. The filter will be inserted somewhere nice in the
   list of filters. The return value is a pointer to the _new filter,
   If the filter couldn't be added the return value is NULL. */
static af_instance_t* af_add(af_stream_t* s,char* name){
  af_instance_t* _new;
  // Sanity check
  if(!s || !s->first || !name)
    return NULL;
  // Insert the filter somwhere nice
  if(!strcmp(s->first->info->name,"format"))
    _new = af_append(s, s->first, name);
  else
    _new = af_prepend(s, s->first, name);
  if(!_new)
    return NULL;

  // Reinitalize the filter list
  if(MPXP_Ok != af_reinit(s, s->first)){
    delete _new;
    return NULL;
  }
  return _new;
}

// Filter data chunk through the filters in the list
mp_aframe_t* __FASTCALL__ RND_RENAME8(af_play)(af_stream_t* s,const mp_aframe_t* data)
{
    mp_aframe_t* in = const_cast<mp_aframe_t*>(data);
    mp_aframe_t* out;
    af_instance_t* af=s->first;
    // Iterate through all filters
    do{
	MSG_DBG2("filtering %s\n",af->info->name);
	out=af->play(af,in);
	if(out!=in && in!=data) free_mp_aframe(in);
	in=out;
	af=af->next;
    }while(af && out);
    return out;
}

/* Helper function used to calculate the exact buffer length needed
   when buffers are resized. The returned length is >= than what is
   needed */
unsigned __FASTCALL__ af_lencalc(frac_t mul,const mp_aframe_t* d){
  unsigned t = (d->format&MPAF_BPS_MASK)*d->nch;
  return t*(((d->len/t)*mul.n)/(unsigned)mul.d + 1);
}

/* Calculate how long the output from the filters will be given the
   input length "len". The calculated length is >= the actual
   length. */
int __FASTCALL__ af_outputlen(const af_stream_t* s, int len)
{
  unsigned t = (s->input.format&MPAF_BPS_MASK)*s->input.nch;
  af_instance_t* af=s->first;
  register frac_t mul = {1,1};
  // Iterate through all filters
  do{
    mul.n *= af->mul.n;
    mul.d *= af->mul.d;
    af=af->next;
  }while(af);
  return t * (((len/t)*mul.n + 1)/mul.d);
}

/* Calculate how long the input to the filters should be to produce a
   certain output length, i.e. the return value of this function is
   the input length required to produce the output length "len". The
   calculated length is <= the actual length */
int __FASTCALL__ af_inputlen(const af_stream_t* s, int len)
{
  unsigned t = (s->input.format&MPAF_BPS_MASK)*s->input.nch;
  af_instance_t* af=s->first;
  register frac_t mul = {1,1};
  // Iterate through all filters
  do{
    mul.n *= af->mul.n;
    mul.d *= af->mul.d;
    af=af->next;
  }while(af);
  return t * (((len/t) * mul.d - 1)/mul.n);
}

/* Calculate the total delay [ms] caused by the filters */
double __FASTCALL__ af_calc_delay(af_stream_t* s)
{
  af_instance_t* af=s->first;
  register double delay = 0.0;
  // Iterate through all filters
  while(af){
    delay += af->delay;
    af=af->next;
  }
  return delay;
}

// send control to all filters, starting with the last until
// one responds with MPXP_Ok
MPXP_Rc __FASTCALL__ af_control_any_rev (af_stream_t* s, int cmd, any_t* arg) {
  MPXP_Rc res = MPXP_Unknown;
  af_instance_t* filt = s->last;
  while (filt && res != MPXP_Ok) {
    res = filt->control(filt, cmd, arg);
    filt = filt->prev;
  }
  return res;
}

MPXP_Rc __FASTCALL__ af_query_fmt (const af_stream_t* s,mpaf_format_e fmt)
{
    af_instance_t* filt = s?s->first:NULL;
    const char *filt_name=filt?filt->info->name:"ao2";
    if(strcmp(filt_name,"ao2")==0) return RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_FORMAT,fmt);
    else if(mpaf_format_decode(fmt)==filt->conf.format) return MPXP_True;
    return MPXP_False;
}

MPXP_Rc __FASTCALL__ af_query_rate (const af_stream_t* s,unsigned rate)
{
    af_instance_t* filt = s?s->first:NULL;
    const char *filt_name=filt?filt->info->name:"ao2";
    if(strcmp(filt_name,"ao2")==0) return RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_RATE,rate);
    else if(rate==filt->conf.rate) return MPXP_True;
    return MPXP_False;
}

MPXP_Rc __FASTCALL__ af_query_channels (const af_stream_t* s,unsigned nch)
{
    af_instance_t* filt = s?s->first:NULL;
    const char *filt_name=filt?filt->info->name:"ao2";
    if(strcmp(filt_name,"ao2")==0) return RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_CHANNELS,nch);
    else if(nch==filt->conf.nch) return MPXP_True;
    return MPXP_False;
}

void af_help (void) {
    unsigned i = 0;
    MSG_INFO( "Available audio filters:\n");
    while (filter_list[i]) {
	if (filter_list[i]->comment && filter_list[i]->comment[0])
	    MSG_INFO( "\t%-10s: %s (%s)\n", filter_list[i]->name, filter_list[i]->info, filter_list[i]->comment);
	else
	    MSG_INFO( "\t%-10s: %s\n", filter_list[i]->name, filter_list[i]->info);
	i++;
    }
    MSG_INFO("\n");
}

af_stream_t *RND_RENAME6(af_new)(any_t*_parent)
{
    af_stream_t *rval;
    rval = new(zeromem) af_stream_t;
    rval->parent = _parent;
    SECURE_NAME9(rnd_fill)(rval->antiviral_hole,offsetof(af_stream_t,first)-offsetof(af_stream_t,antiviral_hole));
    return rval;
}

void af_showconf(af_instance_t *first)
{
    af_instance_t* af=first;
    // ok!
    MSG_INFO("[libaf] Using audio filters chain:\n");
    // Iterate through all filters
    check_pin("afilter",af->pin,AF_PIN);
    do{
	MSG_INFO("  ");
	if(af->control(af,AF_CONTROL_SHOWCONF,NULL)!=MPXP_Ok)
	    MSG_INFO("[af_%s %s]\n",af->info->name,af->info->info);
	af=af->next;
    }while(af);
}
