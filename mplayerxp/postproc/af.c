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
   NULL 
};

// CPU speed
int* af_cpu_speed = NULL;

/* Find a filter in the static list of filters using it's name. This
   function is used internally */
static const af_info_t* __FASTCALL__ af_find(char*name)
{
  int i=0;
  while(filter_list[i]){
    if(!strcmp(filter_list[i]->name,name))
      return filter_list[i];
    i++;
  }
  MSG_ERR("Couldn't find audio filter '%s'\n",name);
  return NULL;
} 

/* Find filter in the dynamic filter list using it's name This
   function is used for finding already initialized filters */
af_instance_t* __FASTCALL__ af_get(af_stream_t* s, char* name)
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
static af_instance_t* __FASTCALL__ af_create(af_stream_t* s, char* name)
{
  char* cmdline = name;

  // Allocate space for the new filter and reset all pointers
  af_instance_t* _new=mp_mallocz(sizeof(af_instance_t));
  if(!_new){
    MSG_ERR(MSGTR_OutOfMemory);
    return NULL;
  }
  _new->parent=s;
  // Check for commandline parameters
  strsep(&cmdline, "=");

  // Find filter from name
  if(NULL == (_new->info=af_find(name))) {
    mp_free(_new);
    return NULL;
  }
  /* Make sure that the filter is not already in the list if it is
     non-reentrant */
  if(_new->info->flags & AF_FLAGS_NOT_REENTRANT){
    if(af_get(s,name)){
      MSG_ERR("[libaf] There can only be one instance of" 
	     " the filter '%s' in each stream\n",name);  
      mp_free(_new);
      return NULL;
    }
  }

  MSG_V("[libaf] Adding filter %s \n",name);

  // Initialize the new filter
  if(CONTROL_OK == _new->info->open(_new) && 
     CONTROL_ERROR < _new->control(_new,AF_CONTROL_POST_CREATE,&s->cfg)){
    if(cmdline){
      if(CONTROL_ERROR<_new->control(_new,AF_CONTROL_COMMAND_LINE,cmdline))
	return _new;
    }
    else
      return _new;
  }

  mp_free(_new);
  MSG_ERR("[libaf] Couldn't create or open audio filter '%s'\n", name);
  return NULL;
}

/* Create and insert a new filter of type name before the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
static af_instance_t* __FASTCALL__ af_prepend(af_stream_t* s, af_instance_t* af, char* name)
{
  // Create the new filter and make sure it is OK
  af_instance_t* new=af_create(s,name);
  MSG_DBG2("af_prepend %s\n",name);
  if(!new)
    return NULL;
  // Update pointers
  new->next=af;
  if(af){
    new->prev=af->prev;
    af->prev=new;
  }
  else
    s->last=new;
  if(new->prev)
    new->prev->next=new;
  else
    s->first=new;
  return new;
}

/* Create and insert a new filter of type name after the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
af_instance_t* af_append(af_stream_t* s, af_instance_t* af, char* name)
{
  // Create the new filter and make sure it is OK
  af_instance_t* new=af_create(s,name);
  MSG_DBG2("af_append %s\n",name);
  if(!new)
    return NULL;
  // Update pointers
  new->prev=af;
  if(af){
    new->next=af->next;
    af->next=new;
  }
  else
    s->first=new;
  if(new->next)
    new->next->prev=new;
  else
    s->last=new;
  return new;
}

// Uninit and remove the filter "af"
void af_remove(af_stream_t* s, af_instance_t* af)
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
  mp_free(af);
}

/* Reinitializes all filters downstream from the filter given in the
   argument the return value is CONTROL_OK if success and CONTROL_ERROR if
   failure */
int af_reinit(af_stream_t* s, af_instance_t* af)
{
  if(!af)
    return CONTROL_ERROR;

  MSG_DBG2("af_reinit()\n");
  do{
    af_data_t in; // Format of the input to current filter
    int rv=0; // Return value

    // Check if this is the first filter 
    if(!af->prev) 
      memcpy(&in,&(s->input),sizeof(af_data_t));
    else
      memcpy(&in,af->prev->data,sizeof(af_data_t));
    // Reset just in case...
    in.audio=NULL;
    in.len=0;
    
    rv = af->control(af,AF_CONTROL_REINIT,&in);
    switch(rv){
    case CONTROL_OK:
      break;
    case CONTROL_FALSE:{ // Configuration filter is needed
      // Do auto insertion only if force is not specified
      if((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE){
	af_instance_t* _new = NULL;
	// Insert channels filter
	if((af->prev?af->prev->data->nch:s->input.nch) != in.nch){
	  // Create channels filter
	  if(NULL == (_new = af_prepend(s,af,"channels")))
	    return CONTROL_ERROR;
	  // Set number of output channels
	  if(CONTROL_OK != (rv = _new->control(_new,AF_CONTROL_CHANNELS,&in.nch)))
	    return rv;
	  // Initialize channels filter
	  if(!_new->prev) memcpy(&in,&(s->input),sizeof(af_data_t));
	  else		  memcpy(&in,_new->prev->data,sizeof(af_data_t));
	  if(CONTROL_OK != (rv = _new->control(_new,AF_CONTROL_REINIT,&in)))
	    return rv;
	}
	// Insert rate filter
	if((af->prev?af->prev->data->rate:s->input.rate) != in.rate){
	  // Create channels filter
	  if(NULL == (_new = af_prepend(s,af,"resample")))
	    return CONTROL_ERROR;
	  // Set number of output channels
	  if(CONTROL_OK != (rv = _new->control(_new,AF_CONTROL_RESAMPLE_RATE,&in.rate)))
	    return rv;
	  // Initialize channels filter
	  if(!_new->prev) memcpy(&in,&(s->input),sizeof(af_data_t));
	  else		  memcpy(&in,_new->prev->data,sizeof(af_data_t));
	  if(CONTROL_OK != (rv = _new->control(_new,AF_CONTROL_REINIT,&in))) {
	    af_instance_t* af2 = af_prepend(s,af,"format");
	    // Init the new filter
	    if(af2) {
		if((CONTROL_OK != af2->control(af2,AF_CONTROL_FORMAT_BPS,&(in.bps)))
		    || (CONTROL_OK != af2->control(af2,AF_CONTROL_FORMAT_FMT,&(in.format))))
		    return -1;
		if(CONTROL_OK != af_reinit(s,af2))
		    return -1;
		rv = _new->control(_new,AF_CONTROL_REINIT,&in);
	    }
	    return rv;
	  }
	}
	// Insert format filter
	if(((af->prev?af->prev->data->format:s->input.format) != in.format) || 
	   ((af->prev?af->prev->data->bps:s->input.bps) != in.bps)){
	  // Create format filter
	  if(NULL == (_new = af_prepend(s,af,"format")))
	    return CONTROL_ERROR;
	  // Set output bits per sample
	  if(CONTROL_OK != (rv = _new->control(_new,AF_CONTROL_FORMAT_BPS,&in.bps)) || 
	     CONTROL_OK != (rv = _new->control(_new,AF_CONTROL_FORMAT_FMT,&in.format)))
	    return rv;
	  // Initialize format filter
	  if(!_new->prev) memcpy(&in,&(s->input),sizeof(af_data_t));
	  else		  memcpy(&in,_new->prev->data,sizeof(af_data_t));
	  if(CONTROL_OK != (rv = _new->control(_new,AF_CONTROL_REINIT,&in)))
	    return rv;
	}
	if(!_new){ // Should _never_ happen
	  MSG_ERR("[libaf] Unable to correct audio format. " 
		 "This error should never occur, please send bugreport.\n");
	  return CONTROL_ERROR;
	}
	af=_new;
      }
      break;
    }
    case CONTROL_DETACH:{ // Filter is redundant and wants to be unloaded
      // Do auto remove only if force is not specified
      if((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE){
	af_instance_t* aft=af->prev;
	af_remove(s,af);
	if(aft)
	  af=aft;
	else
	  af=s->first; // Restart configuration
      }
      break;
    }
    default:
      MSG_ERR("[libaf] Reinitialization did not work, audio" 
	     " filter '%s' returned error code %i for r=%i c=%i fmt=%x bps=%i\n",af->info->name,rv,in.rate,in.nch,in.format,in.bps);
      return CONTROL_ERROR;
    }
    // Check if there are any filters left in the list
    if(NULL == af){
      if(!(af=af_append(s,s->first,"dummy"))) 
	return -1; 
    }
    else
      af=af->next;
  }while(af);
  return CONTROL_OK;
}

// Uninit and remove all filters
void af_uninit(af_stream_t* s)
{
  while(s->first)
    af_remove(s,s->first);
}

/* Initialize the stream "s". This function creates a new filter list
   if necessary according to the values set in input and output. Input
   and output should contain the format of the current movie and the
   formate of the preferred output respectively. The function is
   reentrant i.e. if called with an already initialized stream the
   stream will be reinitialized. If the binary parameter
   "force_output" is set, the output format will be converted to the
   format given in "s", otherwise the output fromat in the last filter
   will be copied "s". The return value is 0 if success and -1 if
   failure */
int af_init(af_stream_t* s, int force_output)
{
  char *af_name,*af_next;
  // Sanity check
  if(!s) return -1;

  // Precaution in case caller is misbehaving
  s->input.audio  = s->output.audio  = NULL;
  s->input.len    = s->output.len    = 0;

  // Figure out how fast the machine is
  if(AF_INIT_AUTO == (AF_INIT_TYPE_MASK & s->cfg.force))
    s->cfg.force = (s->cfg.force & ~AF_INIT_TYPE_MASK) | AF_INIT_TYPE();

  // Check if this is the first call
  if(!s->first){
    // Add all filters in the list (if there are any)
    if(!s->cfg.list){      // To make automatic format conversion work
      if(!af_append(s,s->first,"ao2"))
	return -1;
    }
    else{
      af_name=s->cfg.list;
      while(af_name){
	af_next=strchr(af_name,',');
	if(af_next) { *af_next=0; af_next++; }
	if(!af_append(s,s->last,af_name))
	    return -1;
	af_name=af_next;
      }
    }
  }
  if(strcmp(s->last->info->name,"ao2")!=0) if(!af_append(s,s->last,"ao2")) return -1;

  // Init filters
  if(CONTROL_OK != af_reinit(s,s->first))
    return -1;

  // If force_output isn't set do not compensate for output format
  if(!force_output){
    memcpy(&s->output, s->last->data, sizeof(af_data_t));
    return 0;
  }

  // Check output format
  if((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE){
    af_instance_t* af = NULL; // New filter
    // Check output frequency if not OK fix with resample
    if(s->last->data->rate!=s->output.rate){
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
      // Init the new filter
      if(!af || (CONTROL_OK != af->control(af,AF_CONTROL_RESAMPLE_RATE,
				      &(s->output.rate))))
	return -1;
      // Use lin int if the user wants fast
      if ((AF_INIT_TYPE_MASK & s->cfg.force) == AF_INIT_FAST) {
        char args[32];
	sprintf(args, "%d:0:0", s->output.rate);
	af->control(af, AF_CONTROL_COMMAND_LINE, args);
      }
      if(CONTROL_OK != af_reinit(s,af))
	return -1;
    }

    // Check number of output channels fix if not OK
    // If needed always inserted last -> easy to screw up other filters
    if(s->last->data->nch!=s->output.nch){
      if(!strcmp(s->last->info->name,"format"))
	af = af_prepend(s,s->last,"channels");
      else
	af = af_append(s,s->last,"channels");
      // Init the new filter
      if(!af || (CONTROL_OK != af->control(af,AF_CONTROL_CHANNELS,&(s->output.nch))))
	return -1;
      if(CONTROL_OK != af_reinit(s,af))
	return -1;
    }

    // Check output format fix if not OK
    if((s->last->data->format != s->output.format) || 
       (s->last->data->bps != s->output.bps)){
      if(strcmp(s->last->info->name,"format"))
	af = af_append(s,s->last,"format");
      else
	af = s->last;
      // Init the new filter
      if(!af ||(CONTROL_OK != af->control(af,AF_CONTROL_FORMAT_BPS,&(s->output.bps))) 
	 || (CONTROL_OK != af->control(af,AF_CONTROL_FORMAT_FMT,&(s->output.format))))
	return -1;
      if(CONTROL_OK != af_reinit(s,af))
	return -1;
    }

    // Re init again just in case
    if(CONTROL_OK != af_reinit(s,s->first))
      return -1;

    if((s->last->data->format != s->output.format) || 
       (s->last->data->bps    != s->output.bps)    ||
       (s->last->data->nch    != s->output.nch)    || 
       (s->last->data->rate   != s->output.rate))  {
      // Something is stuffed audio out will not work 
      MSG_ERR("[libaf] Unable to setup filter system can not" 
	     " meet sound-card demands, please send bugreport. \n");
      af_uninit(s);
      return -1;
    }
  }
  return 0;
}

/* Add filter during execution. This function adds the filter "name"
   to the stream s. The filter will be inserted somewhere nice in the
   list of filters. The return value is a pointer to the new filter,
   If the filter couldn't be added the return value is NULL. */
af_instance_t* af_add(af_stream_t* s, char* name){
  af_instance_t* new;
  // Sanity check
  if(!s || !s->first || !name)
    return NULL;
  // Insert the filter somwhere nice
  if(!strcmp(s->first->info->name,"format"))
    new = af_append(s, s->first, name);
  else
    new = af_prepend(s, s->first, name);
  if(!new)
    return NULL;

  // Reinitalize the filter list
  if(CONTROL_OK != af_reinit(s, s->first)){
    mp_free(new);
    return NULL;
  }
  return new;
}

// Filter data chunk through the filters in the list
af_data_t* __FASTCALL__ af_play(af_stream_t* s, af_data_t* data)
{
  af_instance_t* af=s->first; 
  // Iterate through all filters 
  do{
    MSG_DBG2("filtering %s\n",af->info->name);
    data=af->play(af,data,af->next?0:1);
    af=af->next;
  }while(af && data);
  return data;
}

/* Helper function used to calculate the exact buffer length needed
   when buffers are resized. The returned length is >= than what is
   needed */
int __FASTCALL__ af_lencalc(frac_t mul, af_data_t* d){
  register int t = d->bps*d->nch;
  return t*(((d->len/t)*mul.n)/(unsigned)mul.d + 1);
}

/* Calculate how long the output from the filters will be given the
   input length "len". The calculated length is >= the actual
   length. */
int __FASTCALL__ af_outputlen(af_stream_t* s, int len)
{
  int t = s->input.bps*s->input.nch;
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
int __FASTCALL__ af_inputlen(af_stream_t* s, int len)
{
  int t = s->input.bps*s->input.nch;
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

/* Calculate how long the input IN to the filters should be to produce
   a certain output length OUT but with the following three constraints:
   1. IN <= max_insize, where max_insize is the maximum possible input
      block length
   2. OUT <= max_outsize, where max_outsize is the maximum possible
      output block length
   3. If possible OUT >= len. 
   Return -1 in case of error */ 
int __FASTCALL__ af_calc_insize_constrained(af_stream_t* s, int len,
			       int max_outsize,int max_insize)
{
  int t   = s->input.bps*s->input.nch;
  int in  = 0;
  int out = 0;
  af_instance_t* af=s->first; 
  register frac_t mul = {1,1};
  // Iterate through all filters and calculate total multiplication factor
  do{
    mul.n *= af->mul.n;
    mul.d *= af->mul.d;
    af=af->next;
  }while(af);
  // Sanity check 
  if(!mul.n || !mul.d) 
    return -1;

  in = t * (((len/t) * mul.d - 1)/mul.n);
  
  if(in>max_insize) in=t*(max_insize/t);

  // Try to meet constraint nr 3. 
  while((out=t * (((in/t+1)*mul.n - 1)/mul.d)) <= max_outsize && in<=max_insize){
    if( (t * (((in/t)*mul.n))/mul.d) >= len) return in;
    in+=t;
  }

  // Could no meet constraint nr 3.
  while(out > max_outsize || in > max_insize){
    in-=t;
    if(in<t) return -1; // Input parameters are probably incorrect
    out = t * (((in/t)*mul.n + 1)/mul.d);
  }
  return in;
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

/* Helper function called by the macro with the same name this
   function should not be called directly */
int __FASTCALL__ af_resize_local_buffer(af_instance_t* af, af_data_t* data)
{
  // Calculate new length
  register int len = af_lencalc(af->mul,data);
  MSG_V("[libaf] Reallocating memory in module %s, "
	 "old len = %i, new len = %i\n",af->info->name,af->data->len,len);
  // If there is a buffer mp_free it
  if(af->data->audio) mp_free(af->data->audio);
  // Create new buffer and check that it is OK
  af->data->audio = mp_malloc(len);
  if(!af->data->audio){
    MSG_FATAL(MSGTR_OutOfMemory);
    return CONTROL_ERROR;
  }
  af->data->len=len;
  return CONTROL_OK;
}

// send control to all filters, starting with the last until
// one responds with CONTROL_OK
int __FASTCALL__ af_control_any_rev (af_stream_t* s, int cmd, any_t* arg) {
  int res = CONTROL_UNKNOWN;
  af_instance_t* filt = s->last;
  while (filt && res != CONTROL_OK) {
    res = filt->control(filt, cmd, arg);
    filt = filt->prev;
  }
  return (res == CONTROL_OK);
}

int __FASTCALL__ af_query_fmt (af_stream_t* s,int fmt)
{
  af_instance_t* filt = s?s->first:NULL;
  const char *filt_name=filt?filt->info->name:"ao2";
  if(strcmp(filt_name,"ao2")==0) return ao_control(ao_data,AOCONTROL_QUERY_FORMAT,fmt);
  else
  {
    int bps;
    int ifmt=af_format_decode(fmt,&bps);
    if(ifmt==filt->data->format && bps==filt->data->bps) return CONTROL_TRUE;
  }
  return CONTROL_FALSE;
}

int __FASTCALL__ af_query_rate (af_stream_t* s,int rate)
{
  af_instance_t* filt = s?s->first:NULL;
  const char *filt_name=filt?filt->info->name:"ao2";
  if(strcmp(filt_name,"ao2")==0) return ao_control(ao_data,AOCONTROL_QUERY_RATE,rate);
  else
  {
    if(rate==filt->data->rate) return CONTROL_TRUE;
  }
  return CONTROL_FALSE;
}

int __FASTCALL__ af_query_channels (af_stream_t* s,int nch)
{
  af_instance_t* filt = s?s->first:NULL;
  const char *filt_name=filt?filt->info->name:"ao2";
  if(strcmp(filt_name,"ao2")==0) return ao_control(ao_data,AOCONTROL_QUERY_CHANNELS,nch);
  else
  {
    if(nch==filt->data->nch) return CONTROL_TRUE;
  }
  return CONTROL_FALSE;
}

void af_help (void) {
  int i = 0;
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

af_stream_t *af_new(any_t*_parent)
{
    af_stream_t *rval;
    rval = mp_mallocz(sizeof(af_stream_t));
    rval->parent = _parent;
    return rval;
}

void af_showconf(af_instance_t *first)
{
  af_instance_t* af=first;
  // ok!
  MSG_INFO("[libaf] Using audio filters chain:\n");
  // Iterate through all filters
  do{
	MSG_INFO("  ");
	if(af->control(af,AF_CONTROL_SHOWCONF,NULL)!=CONTROL_OK)
	    MSG_INFO("[af_%s %s]\n",af->info->name,af->info->info);
	af=af->next;
  }while(af);
}
