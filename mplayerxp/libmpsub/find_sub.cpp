#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
//**************************************************************************//
//             .SUB
//**************************************************************************//
#ifdef USE_OSD

#include <stdio.h>
#include <limits.h>
#include "libvo2/video_out.h"
#include "libvo2/sub.h"
#include "subreader.h"
#include "mpsub_msg.h"

static int current_sub=0;

//static subtitle* subtitles=NULL;
static unsigned long nosub_range_start=ULONG_MAX;
static unsigned long nosub_range_end=ULONG_MAX;

void find_sub(subtitle* subtitles,unsigned long key,Video_Output*vo){
    int i,j;

    if ( !subtitles ) return;

    if(vo->sub){
      if(key>=vo->sub->start && key<=vo->sub->end) return; // OK!
    } else {
      if(key>nosub_range_start && key<nosub_range_end) return; // OK!
    }
    // sub changed!

    /* Tell the OSD subsystem that the OSD contents will change soon */
    vo_osd_changed(OSDTYPE_SUBTITLE);

    if(key<=0){
      vo->sub=NULL; // no sub here
      return;
    }

    // check next sub.
    if(current_sub>=0 && current_sub+1<sub_num){
      if(key>subtitles[current_sub].end && key<subtitles[current_sub+1].start){
	  // no sub
	  nosub_range_start=subtitles[current_sub].end;
	  nosub_range_end=subtitles[current_sub+1].start;
	  vo->sub=NULL;
	  return;
      }
      // next sub?
      ++current_sub;
      vo->sub=&subtitles[current_sub];
      if(key>=vo->sub->start && key<=vo->sub->end) return; // OK!
    }

    // use logarithmic search:
    i=0;j=sub_num-1;
    while(j>=i){
	current_sub=(i+j+1)/2;
	vo->sub=&subtitles[current_sub];
	if(key<vo->sub->start) j=current_sub-1;
	else if(key>vo->sub->end) i=current_sub+1;
	else return; // found!
    }
//    if(key>=vo->sub->start && key<=vo->sub->end) return; // OK!

    // check where are we...
    if(key<vo->sub->start){
      if(current_sub<=0){
	  // before the first sub
	  nosub_range_start=key-1; // tricky
	  nosub_range_end=vo->sub->start;
	  vo->sub=NULL;
	  return;
      }
      --current_sub;
      if(key>subtitles[current_sub].end && key<subtitles[current_sub+1].start){
	  // no sub
	  nosub_range_start=subtitles[current_sub].end;
	  nosub_range_end=subtitles[current_sub+1].start;
	  vo->sub=NULL;
	  return;
      }
      mpxp_v<<"HEH????  "<<std::endl;
    } else {
      if(key<=vo->sub->end) mpxp_v<<"JAJJ!  "<<std::endl; else
      if(current_sub+1>=sub_num){
	  // at the end?
	  nosub_range_start=vo->sub->end;
	  nosub_range_end=0x7FFFFFFF; // MAXINT
	  vo->sub=NULL;
	  return;
      } else
      if(key>subtitles[current_sub].end && key<subtitles[current_sub+1].start){
	  // no sub
	  nosub_range_start=subtitles[current_sub].end;
	  nosub_range_end=subtitles[current_sub+1].start;
	  vo->sub=NULL;
	  return;
      }
    }

    mpxp_err<<"SUB ERROR: "<<key<<" ? "<<(int)vo->sub->start<<" --- "<<(int)vo->sub->end<<" ["<<current_sub<<"]"<<std::endl;

    vo->sub=NULL; // no sub here
}

#endif
