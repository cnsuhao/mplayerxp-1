#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/* MplayerXP (C) 2000-2002. by A'rpi/ESP-team (C) 2002. by Nickols_K */
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <map>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#define __USE_ISOC99 1 /* for lrint */
#include <math.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>

#include "version.h"
#include "mplayerxp.h"
#include "xmpcore/sig_hand.h"

#include "postproc/af.h"
#include "postproc/vf.h"
#define HELP_MPXP_DEFINE_STATIC
#include "mpxp_help.h"

#include "libmpstream2/stream.h"
#include "libmpdemux/demuxer.h"

#include "libmpconf/codec-cfg.h"
#include "libplaytree2/playtree.h"

#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/dec_audio.h"

#include "libmpsub/spudec.h"
#include "libmpsub/vobsub.h"

#include "osdep/get_path.h"
#include "osdep/cpudetect.h"
#include "osdep/mm_accel.h"
#include "osdep/timer.h"

#include "nls/nls.h"
#include "postproc/libmenu/menu.h"

#include "input2/input.h"
#include "player_msg.h"
#include "xmpcore/xmp_core.h"
#include "xmpcore/xmp_vplayer.h"
#include "xmpcore/xmp_adecoder.h"
#include "xmpcore/xmp_context.h"
#include "xmpcore/PointerProtector.h"
#include "dump.h"

namespace	usr {
/**************************************************************************
	     Private data
**************************************************************************/
static volatile char antiviral_hole1[__VM_PAGE_SIZE__] __PAGE_ALIGNED__;
/**************************************************************************
	     Config file
**************************************************************************/
enum {
    INITED_VO		=0x00000001,
    INITED_AO		=0x00000002,
    INITED_RESERVED	=0x00000004,
    INITED_LIRC		=0x00000008,
    INITED_SPUDEC	=0x00000010,
    INITED_STREAM	=0x00000020,
    INITED_INPUT	=0x00000040,
    INITED_DEMUXER	=0x00000080,
    INITED_ACODEC	=0x00000100,
    INITED_VCODEC	=0x00000200,
    INITED_VOBSUB	=0x00000400,
    INITED_SUBTITLE	=0x10000000,
    INITED_XMP		=0x80000000,
    INITED_ALL		=0xFFFFFFFF
};

enum {
    PT_NEXT_ENTRY	=1,
    PT_PREV_ENTRY	=-1,
    PT_NEXT_SRC		=2,
    PT_PREV_SRC		=-2,
    PT_UP_NEXT		=3,
    PT_UP_PREV		=-3
};

struct osd_args_t {
    int		visible;
    int		info_factor;
};

struct input_state_t {
    int		need_repaint;
    int		after_dvdmenu;
    int		next_file;
};

struct MPXPSystem : public Opaque {
    public:
	MPXPSystem(const std::map<std::string,std::string>& _envm):inited_flags(0),osd_function(OSD_PLAY),envm(_envm),_libinput(mp_input_open(_envm)) { }
	virtual ~MPXPSystem() {}

	void		uninit_player(unsigned int mask);
	Demuxer*	demuxer() const { return _demuxer; }
	Demuxer*	assign_demuxer(Demuxer* _d) { uninit_demuxer(); _demuxer=_d; if(_d) inited_flags|=INITED_DEMUXER; return _demuxer; }
	libinput_t&	libinput() const { return _libinput; }
	void		uninit_demuxer();
	void		uninit_input();

	int 		libmpdemux_was_interrupted(int eof) const;
	void		seek(osd_args_t *osd,const seek_args_t* seek) const;
	void		init_keyboard_fifo();
	char*		init_output_subsystems();
	int		init_vobsub(const std::string& filename);
	void		init_dvd_nls() const;

	int		handle_playlist(const std::string& filename) const;

	void		print_stream_formats() const;
	void		print_audio_status() const;
	void		read_video_properties() const;
	void		read_subtitles(const std::string& filename,int forced_subs_only,int stream_dump_type);

	void		find_acodec(const char *ao_subdevice);
	int		configure_audio();

	MPXP_Rc		find_vcodec();

	void		run_ahead_engine();

#ifdef USE_OSD
	int		paint_osd(int* osd_visible,int* in_pause);
#endif
	int		handle_input(seek_args_t* seek,osd_args_t* osd,input_state_t* state);

	unsigned	inited_flags;
	MPXP_Rc		vo_inited;
	MPXP_Rc		ao_inited;
	int		osd_show_framedrop;
	int		osd_function;
	PlayTree*	playtree;
	// for multifile support:
	PlayTree_Iter* playtree_iter;
	const std::map<std::string,std::string>& envm;
	unsigned	xp_num_cpu;
    private:
	Opaque		unusable;
	Demuxer*	_demuxer;
	libinput_t&	_libinput;
};

struct MPXPSecureKeys {
public:
    MPXPSecureKeys(unsigned _nkeys):nkeys(_nkeys) { keys = new unsigned [nkeys]; for(unsigned i=0;i<nkeys;i++) keys[i]=rand()%UINT_MAX; }
    ~MPXPSecureKeys() { delete [] keys; }
private:
    unsigned	nkeys;
    unsigned*	keys;
};

MP_Config::MP_Config() {
    memset(&has_video,0,reinterpret_cast<long>(&monitor_pixel_aspect)-reinterpret_cast<long>(&has_video));
    xp=get_number_cpu();
    audio_id=-1;
    video_id=-1;
    dvdsub_id=-1;
    vobsub_id=-1;
    audio_lang=I18N_LANGUAGE;
    dvdsub_lang=I18N_LANGUAGE;
    av_sync_pts=-1;
    frame_reorder=1;
    av_force_pts_fix2=-1;
    loop_times=-1;
    play_n_frames=-1;
    font_factor=0.75;
    sub_auto=1;
    has_audio=1;
    has_video=1;
    has_dvdsub=1;
    osd_level=2;
    playbackspeed_factor=1.0;
    ao_channels=2;
    monitor_pixel_aspect=1;
    msg_filter=0xFFFFFFFF;
    max_trace=10;
}
MP_Config mp_conf;

MPXPContext::MPXPContext()
	    :_engine(new(zeromem) mpxp_engine_t),
	    _audio(new(zeromem) audio_processing_t),
	    _video(new(zeromem) video_processing_t)
{
    seek_time=-1;
    bench=new(zeromem) time_usage_t;
    use_pts_fix2=-1;
    rtc_fd=-1;
}

MPXPContext::~MPXPContext()
{
    delete _engine->MPXPSys;
    delete bench;
}

static volatile char antiviral_hole2[__VM_PAGE_SIZE__] __PAGE_ALIGNED__;
static Opaque	opaque1;
static LocalPtr<MPXPContext> MPXPCtx(new(zeromem) MPXPContext);
static Opaque	opaque2;
static volatile char antiviral_hole3[__VM_PAGE_SIZE__] __PAGE_ALIGNED__;
static Opaque	opaque3;
volatile MPXPSecureKeys* secure_keys;
static Opaque	opaque4;
/**************************************************************************
	     Decoding ahead
**************************************************************************/
static volatile char antiviral_hole4[__VM_PAGE_SIZE__] __PAGE_ALIGNED__;
/**************************************************************************/
MPXPContext& mpxp_context() { return *MPXPCtx; }

static MPXP_Rc mpxp_test_antiviral_protection(int* verbose)
{
    if(*verbose) mpxp_info<<"Your've specified test-av option!\nRight now MPlayerXP should make coredump!"<<std::endl;
    *verbose=antiviral_hole1[0]|antiviral_hole2[0]|antiviral_hole3[0]|antiviral_hole4[0];
    mpxp_err<<"Antiviral protection of MPlayerXP doesn't work!"<<std::endl;
    return MPXP_Virus;
}

static void __attribute__ ((noinline)) mpxp_test_backtrace(void) {
    goto *(reinterpret_cast<any_t*>(get_caller_address()));
    kill(getpid(), SIGILL);
}

unsigned get_number_cpu(void) {
#ifdef _OPENMP
    return omp_get_num_procs();
#else
    /* TODO ? */
    return 1;
#endif
}

static void mpxp_uninit_structs(void) {
    if(mpxp_context().video().output) delete mpxp_context().video().output;
    if(mpxp_context().audio().output) delete mpxp_context().audio().output;
    xmp_uninit();
    mp_uninit_malloc(mp_conf.verbose);
}

void MPXPSystem::uninit_demuxer() {
    if(inited_flags&INITED_DEMUXER) {
	inited_flags&=~INITED_DEMUXER;
	MP_UNIT("free_priv->demuxer");
	delete _demuxer;
	_demuxer=NULL;
    }
}

void MPXPSystem::uninit_input() {
    if(inited_flags&INITED_INPUT) {
	inited_flags&=~INITED_INPUT;
	MP_UNIT("uninit_input");
	mp_input_close(_libinput);
    }
}
void MPXPSystem::uninit_player(unsigned int mask){
    Stream* stream=NULL;
    if(_demuxer) stream=static_cast<Stream*>(_demuxer->stream);

    fflush(stdout);
    fflush(stderr);
    mask=inited_flags&mask;

    MP_UNIT("uninit_xp");
    if(mask&INITED_XMP) {
	inited_flags&=~INITED_XMP;
	MP_UNIT("uninit_xmp");
	xmp_uninit_engine(0);
    }

    if (mask&INITED_SPUDEC){
	inited_flags&=~INITED_SPUDEC;
	MP_UNIT("uninit_spudec");
	spudec_free(mpxp_context().video().output->spudec);
	mpxp_context().video().output->spudec=NULL;
    }

    if (mask&INITED_VOBSUB){
	inited_flags&=~INITED_VOBSUB;
	MP_UNIT("uninit_vobsub");
	vobsub_close(mpxp_context().video().output->vobsub);
	mpxp_context().video().output->vobsub=NULL;
    }

    if(mask&INITED_VCODEC){
	inited_flags&=~INITED_VCODEC;
	MP_UNIT("uninit_vcodec");
	delete mpxp_context().video().decoder;
	mpxp_context().video().decoder=NULL;
    }

    if(mask&INITED_VO){
	inited_flags&=~INITED_VO;
	MP_UNIT("uninit_vo");
	delete mpxp_context().video().output;
	mpxp_context().video().output=NULL;
	vo_inited=MPXP_False;
    }

    if(mask&INITED_ACODEC){
	inited_flags&=~INITED_ACODEC;
	MP_UNIT("uninit_acodec");
	delete mpxp_context().audio().decoder;
	mpxp_context().audio().decoder=NULL;
    }

    if(mask&INITED_AO){
	inited_flags&=~INITED_AO;
	MP_UNIT("uninit_ao");
	delete mpxp_context().audio().output;
	mpxp_context().audio().output=NULL;
	ao_inited=MPXP_False;
    }

    if(mask&INITED_DEMUXER) uninit_demuxer();

    if(mask&INITED_STREAM){
	inited_flags&=~INITED_STREAM;
	MP_UNIT("uninit_stream");
	if(stream) delete stream;
	stream=NULL;
    }

    if(mask&INITED_INPUT) uninit_input();
#ifdef USE_SUB
    if(mask&INITED_SUBTITLE){
	inited_flags&=~INITED_SUBTITLE;
	MP_UNIT("sub_free");
	sub_free( mpxp_context().subtitles );
	mp_conf.sub_name=NULL;
	mpxp_context().video().output->sub=NULL;
	mpxp_context().subtitles=NULL;
    }
#endif
    MP_UNIT(NULL);
}

class soft_exit_exception : public std::exception {
	public:
	    soft_exit_exception(const std::string& why) throw();
	    virtual ~soft_exit_exception() throw();

	    virtual const char*	what() const throw();
	private:
	    std::string why;
};

soft_exit_exception::soft_exit_exception(const std::string& _why) throw() { why=_why; }
soft_exit_exception::~soft_exit_exception() throw() {}
const char* soft_exit_exception::what() const throw() { return why.c_str(); }

void exit_player(const std::string& why)
{
    if(!why.empty()) throw soft_exit_exception(why);
    throw std::exception();
}

void __exit_sighandler()
{
  static int sig_count=0;
  ++sig_count;
//  return;
  if(sig_count==2) return;
  if(sig_count>2){
    // can't stop :(
    kill(getpid(),SIGKILL);
    return;
  }
  exit_player("");
}


void exit_sighandler(void)
{
  xmp_killall_threads(pthread_self());
  __exit_sighandler();
}

// When libmpdemux perform a blocking operation (network connection or cache filling)
// if the operation fail we use this function to check if it was interrupted by the user.
// The function return a new value for eof.
int MPXPSystem::libmpdemux_was_interrupted(int eof) const
{
    mp_cmd_t* cmd;
    if((cmd = mp_input_get_cmd(_libinput,0,0,0)) != NULL) {
	switch(cmd->id) {
	    case MP_CMD_QUIT:
	    case MP_CMD_SOFT_QUIT: // should never happen
		exit_player(MSGTR_Exit_quit);
	    case MP_CMD_PLAY_TREE_STEP: {
		eof = (cmd->args[0].v.i > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
	    } break;
	    case MP_CMD_PLAY_TREE_UP_STEP: {
		eof = (cmd->args[0].v.i > 0) ? PT_UP_NEXT : PT_UP_PREV;
	    } break;
	    case MP_CMD_PLAY_ALT_SRC_STEP: {
		eof = (cmd->args[0].v.i > 0) ?  PT_NEXT_SRC : PT_PREV_SRC;
	    } break;
	}
	mp_cmd_free(cmd);
    }
    return eof;
}

#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
static void get_mmx_optimizations( void )
{
  GetCpuCaps(&gCpuCaps);

  if(mp_conf.x86.simd) {
    if(mp_conf.x86.mmx != -1) gCpuCaps.hasMMX=mp_conf.x86.mmx;
    if(mp_conf.x86.mmx2 != -1) gCpuCaps.hasMMX2=mp_conf.x86.mmx2;
    if(mp_conf.x86._3dnow != -1) gCpuCaps.has3DNow=mp_conf.x86._3dnow;
    if(mp_conf.x86._3dnow2 != -1) gCpuCaps.has3DNowExt=mp_conf.x86._3dnow2;
    if(mp_conf.x86.sse != -1) gCpuCaps.hasSSE=mp_conf.x86.sse;
    if(mp_conf.x86.sse2 != -1) gCpuCaps.hasSSE2=mp_conf.x86.sse2;
    if(mp_conf.x86.sse3 != -1) gCpuCaps.hasSSE2=mp_conf.x86.sse3;
    if(mp_conf.x86.ssse3 != -1) gCpuCaps.hasSSSE3=mp_conf.x86.ssse3;
    if(mp_conf.x86.sse41 != -1) gCpuCaps.hasSSE41=mp_conf.x86.sse41;
    if(mp_conf.x86.sse42 != -1) gCpuCaps.hasSSE42=mp_conf.x86.sse42;
    if(mp_conf.x86.aes != -1) gCpuCaps.hasAES=mp_conf.x86.aes;
    if(mp_conf.x86.avx != -1) gCpuCaps.hasAVX=mp_conf.x86.avx;
    if(mp_conf.x86.fma != -1) gCpuCaps.hasFMA=mp_conf.x86.fma;
  } else {
    gCpuCaps.hasMMX=
    gCpuCaps.hasMMX2=
    gCpuCaps.has3DNow=
    gCpuCaps.has3DNowExt=
    gCpuCaps.hasSSE=
    gCpuCaps.hasSSE2=
    gCpuCaps.hasSSE3=
    gCpuCaps.hasSSSE3=
    gCpuCaps.hasSSE41=
    gCpuCaps.hasSSE42=
    gCpuCaps.hasAES=
    gCpuCaps.hasAVX=
    gCpuCaps.hasFMA=0;
  }
  mpxp_v<<"User corrected CPU flags: MMX="<<gCpuCaps.hasMMX
	<<" MMX2="<<gCpuCaps.hasMMX2
	<<" 3DNow="<<gCpuCaps.has3DNow
	<<" 3DNow2="<<gCpuCaps.has3DNowExt
	<<" SSE="<<gCpuCaps.hasSSE
	<<" SSE2="<<gCpuCaps.hasSSE2
	<<" SSE3="<<gCpuCaps.hasSSE3
	<<" SSSE3="<<gCpuCaps.hasSSSE3
	<<" SSE41="<<gCpuCaps.hasSSE41
	<<" SSE42="<<gCpuCaps.hasSSE42
	<<" AES="<<gCpuCaps.hasAES
	<<" AVX="<<gCpuCaps.hasAVX
	<<" FMA="<<gCpuCaps.hasFMA
	<<std::endl;
  if(gCpuCaps.hasMMX) 		mpxp_context().mplayer_accel |= MM_ACCEL_X86_MMX;
  if(gCpuCaps.hasMMX2) 		mpxp_context().mplayer_accel |= MM_ACCEL_X86_MMXEXT;
  if(gCpuCaps.hasSSE) 		mpxp_context().mplayer_accel |= MM_ACCEL_X86_SSE;
  if(gCpuCaps.has3DNow) 	mpxp_context().mplayer_accel |= MM_ACCEL_X86_3DNOW;
  if(gCpuCaps.has3DNowExt) 	mpxp_context().mplayer_accel |= MM_ACCEL_X86_3DNOWEXT;
  mpxp_v<<"mpxp_context().mplayer_accel="<<mpxp_context().mplayer_accel<<std::endl;
}
#endif


static MPXP_Rc init_player(const std::map<std::string,std::string>& envm)
{
    if(mp_conf.video_driver && strcmp(mp_conf.video_driver,"help")==0) {
	mpxp_context().video().output->print_help();
	mpxp_uninit_structs();
	return MPXP_False;
    }
    if(mp_conf.audio_driver && strcmp(mp_conf.audio_driver,"help")==0) {
	mpxp_context().audio().output->print_help();
	mpxp_uninit_structs();
	return MPXP_False;
    }
    if(mp_conf.video_family && strcmp(mp_conf.video_family,"help")==0) {
	VD_Interface::print_help();
	mpxp_uninit_structs();
	return MPXP_False;
    }
    if(mp_conf.audio_family && strcmp(mp_conf.audio_family,"help")==0) {
	AD_Interface::print_help();
	mpxp_uninit_structs();
	return MPXP_False;
    }
    if(vf_cfg.list && strcmp(vf_cfg.list,"help")==0) {
	vf_help();
	mpxp_uninit_structs();
	return MPXP_False;
    }
    if(af_cfg.list && strcmp(af_cfg.list,"help")==0) {
	af_help();
	mpxp_uninit_structs();
	return MPXP_False;
    }

    if(mp_conf.audio_codec && strcmp(mp_conf.audio_codec,"help")==0) {
	mpxp_uninit_structs();
	return MPXP_False;
    }
    if(mp_conf.video_codec && strcmp(mp_conf.video_codec,"help")==0) {
	mpxp_uninit_structs();
	return MPXP_False;
    }
    return MPXP_Ok;
}

void show_help(void) {
    // no file/vcd/dvd -> show HELP:
    for(unsigned j=0;help_text[j];j++) mpxp_info<<help_text[j]<<std::endl;
    Stream::print_drivers();
    mpxp_info<<std::endl;
    mpxp_info<<"Example: mplayerxp -ao alsa:hw:0 -vo x11 your.avi"<<std::endl;
    mpxp_info<<"Use --long-help option for full help"<<std::endl;
}

void show_long_help(const M_Config& cfg,const std::map<std::string,std::string>& envm) {
    MPXPSystem& MPXPSys=*mpxp_context().engine().MPXPSys;
    cfg.show_options();
    mp_input_print_binds(MPXPSys.libinput());
    Stream::print_drivers();
    Video_Output::print_help();
    Audio_Output::print_help();
    vf_help();
    af_help();
    VD_Interface::print_help();
    AD_Interface::print_help();
}

#ifdef USE_OSD

//================= Update OSD ====================
void update_osd( float v_pts )
{
    MPXPSystem& MPXPSys=*mpxp_context().engine().MPXPSys;
    static int osd_last_pts=-303;
//================= Update OSD ====================
  if(mp_conf.osd_level>=2){
      int pts=(mp_conf.osd_level==3&&MPXPSys.demuxer()->movi_length!=UINT_MAX)?MPXPSys.demuxer()->movi_length-v_pts:v_pts;
      int addon=(mp_conf.osd_level==3&&MPXPSys.demuxer()->movi_length!=UINT_MAX)?-1:1;
      std::ostringstream oss;
      if(pts==osd_last_pts-addon)
      {
	if(mp_conf.osd_level==3&&MPXPSys.demuxer()->movi_length!=UINT_MAX) ++pts;
	else --pts;
      }
      else osd_last_pts=pts;
      if (MPXPSys.osd_show_framedrop) {
	  oss<<"Framedrop: "<<(mp_conf.frame_dropping>1?"hard":mp_conf.frame_dropping?"vo":"none");
	  MPXPSys.osd_show_framedrop--;
      } else
#ifdef ENABLE_DEC_AHEAD_DEBUG
	  if(mp_conf.verbose) oss<<MPXPSys.osd_function<<" "<<std::setfill('0')<<std::setw(2)<<(pts/3600)
				    <<std::setfill('0')<<std::setw(2)<<(pts/60)
				    <<std::setfill('0')<<std::setw(2)<<(pts%60);
	  else  oss<<MPXPSys.osd_function<<" "<<std::setfill('0')<<std::setw(2)<<(pts/3600)
		    <<" "<<std::setfill('0')<<std::setw(2)<<((pts/60)%60)
		    <<" "<<std::setfill('0')<<std::setw(2)<<(pts%60);
#else
	    oss<<MPXPSys.osd_function<<" "<<std::setfill('0')<<std::setw(2)<<(pts/3600)
		<<" "<<std::setfill('0')<<std::setw(2)<<((pts/60)%60)
		<<" "<<std::setfill('0')<<std::setw(2)<<(pts%60);
#endif
      if(mpxp_context().video().output->osd_text!=oss.str()) {
	    mpxp_context().video().output->osd_text=oss.str();
	    vo_osd_changed(OSDTYPE_OSD);
      }
  } else {
      if(!mpxp_context().video().output->osd_text.empty()) {
      mpxp_context().video().output->osd_text.clear();
	  vo_osd_changed(OSDTYPE_OSD);
      }
  }
}
#endif

void MPXPSystem::seek( osd_args_t *osd,const seek_args_t* _seek) const
{
    sh_audio_t* sh_audio=reinterpret_cast<sh_audio_t*>(_demuxer->audio->sh);
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(_demuxer->video->sh);
    Demuxer_Stream *d_dvdsub=_demuxer->sub;
    int seek_rval=1;
    mpxp_context().engine().xp_core->audio->eof=0;
    if(_seek->secs || _seek->flags&DEMUX_SEEK_SET) {
	seek_rval=demux_seek_r(*_demuxer,_seek);
	mpxp_context().mpxp_after_seek=25; /* 1 sec delay */
    }
    if(seek_rval){
	mpxp_context().seek_time = GetTimerMS();

	// success:
	/* FIXME there should be real seeking for vobsub */
	if (mpxp_context().video().output->vobsub) vobsub_reset(mpxp_context().video().output->vobsub);
	if (mpxp_context().video().output->spudec) spudec_reset(mpxp_context().video().output->spudec);

	if(sh_audio){
	    sh_audio->chapter_change=0;
	    sh_audio->a_pts=HUGE;
	}
	fflush(stdout);

	if(sh_video){
	    MP_UNIT("seek_video_reset");
	    mpxp_context().video().decoder->resync_stream();
	    mpxp_context().video().output->reset();
	    sh_video->chapter_change=-1;
	}

	if(sh_audio){
	    MP_UNIT("seek_audio_reset");
	    mpxp_context().audio().decoder->resync_stream();
	    mpxp_context().audio().output->reset(); // stop audio, throwing away buffered data
	}

	if (mpxp_context().video().output->vobsub) {
	    MP_UNIT("seek_vobsub_reset");
	    vobsub_seek_r(mpxp_context().video().output->vobsub, _seek);
	}

#ifdef USE_OSD
	// Set OSD:
	if(mp_conf.osd_level){
	    int len=((_demuxer->movi_end-_demuxer->movi_start)>>8);
	    if (len>0){
		if(osd) osd->visible=sh_video->fps<=60?sh_video->fps:25;
		mpxp_context().video().output->osd_progbar_type=0;
		mpxp_context().video().output->osd_progbar_value=(_demuxer->filepos-_demuxer->movi_start)/len;
		vo_osd_changed(OSDTYPE_PROGBAR);
	    }
	}
#endif
	if(sh_video) {
	    max_pts_correction=0.1;
	    if(osd) osd->visible=sh_video->fps<=60?sh_video->fps:25; // to rewert to PLAY pointer after 1 sec
	    mpxp_context().bench->audio=0; mpxp_context().bench->audio_decode=0; mpxp_context().bench->video=0; mpxp_context().bench->vout=0;
	    if(mpxp_context().video().output->spudec) {
		unsigned char* packet=NULL;
		while(ds_get_packet_sub_r(*d_dvdsub,&packet)>0) ; // Empty stream
		spudec_reset(mpxp_context().video().output->spudec);
	    }
	}
    }

    if(sh_video) dae_wait_decoder_outrun(mpxp_context().engine().xp_core->video);
}

void mpxp_reset_vcache(void)
{
    MPXPSystem& MPXPSys=*mpxp_context().engine().MPXPSys;
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(MPXPSys.demuxer()->video->sh);
    seek_args_t _seek = { 0, DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS };
    if(sh_video) MPXPSys.seek(NULL,&_seek);
    return;
}

void mpxp_resync_audio_stream(void)
{
    mpxp_context().audio().decoder->resync_stream();
}

static void init_benchmark(void)
{
    mpxp_context().bench->max_audio=0; mpxp_context().bench->max_video=0; mpxp_context().bench->max_vout=0;
    mpxp_context().bench->min_audio=HUGE; mpxp_context().bench->min_video=HUGE; mpxp_context().bench->min_vout=HUGE;

    mpxp_context().bench->min_audio_decode=HUGE;
    mpxp_context().bench->max_audio_decode=0;

    mpxp_context().bench->max_demux=0;
    mpxp_context().bench->demux=0;
    mpxp_context().bench->min_demux=HUGE;

    mpxp_context().bench->cur_video=0;
    mpxp_context().bench->cur_vout=0;
    mpxp_context().bench->cur_audio=0;
}

static void show_benchmark(void)
{
    double tot=(mpxp_context().bench->video+mpxp_context().bench->vout+mpxp_context().bench->audio+mpxp_context().bench->audio_decode+mpxp_context().bench->demux+mpxp_context().bench->c2);
    double total_time_usage;

    mpxp_context().bench->total_start=GetTimer()-mpxp_context().bench->total_start;
    total_time_usage = (float)mpxp_context().bench->total_start*0.000001;

    mpxp_info<<std::endl<<std::setprecision(3)
	    <<"AVE BENCHMARKs: VC:"<<mpxp_context().bench->video<<"s"
	    <<" VO:"<<mpxp_context().bench->vout<<"s"
	    <<" A:"<<mpxp_context().bench->audio+mpxp_context().bench->audio_decode<<"s"
	    <<" D:"<<mpxp_context().bench->demux<<"s="<<mpxp_context().bench->c2<<"s"
	    <<" C:"<<tot<<"s"<<std::endl;
    if(total_time_usage>0.0)
	mpxp_info<<std::setprecision(4)
	    <<"AVE BENCHMARK%: VC:"<<100.0*mpxp_context().bench->video/total_time_usage<<"%"
	    <<" VO:"<<100.0*mpxp_context().bench->vout/total_time_usage<<"%"
	    <<" A:"<<100.0*(mpxp_context().bench->audio+mpxp_context().bench->audio_decode)/total_time_usage<<"%"
	    <<" D:"<<100.0*mpxp_context().bench->demux/total_time_usage<<"%"
	    <<" C:"<<100.0*mpxp_context().bench->c2/total_time_usage<<"%"
	    <<" = "<<100.0*tot/total_time_usage<<"%"<<std::endl;
    unsigned nframes=mpxp_context().engine().xp_core->video->num_played_frames;
    mpxp_info<<std::endl<<"REAL RESULTS: from "<<nframes<<"was dropped="<<mpxp_context().engine().xp_core->video->num_dropped_frames<<std::endl;
}

static void show_benchmark_status(void)
{
    MPXPSystem& MPXPSys=*mpxp_context().engine().MPXPSys;
    sh_audio_t* sh_audio=reinterpret_cast<sh_audio_t*>(MPXPSys.demuxer()->audio->sh);
    float rev_time=(sh_audio->timer>0.5)?100.0*(mpxp_context().bench->audio+mpxp_context().bench->audio_decode)/(double)sh_audio->timer:0;
    if(xmp_test_model(XMP_Run_AudioPlayback))
	mpxp_status<<std::setprecision(1)
		<<"A:"<<sh_audio->timer-mpxp_context().audio().output->get_delay()
		<<" "<<rev_time
		<<"\r";
    else
	mpxp_status<<std::setprecision(1)
		<<"A:"<<sh_audio->timer-mpxp_context().audio().output->get_delay()
		<<" "<<rev_time
		<<" B:"<<get_delay_audio_buffer()
		<<"\r";
    mpxp_status.flush();
}

void MPXPSystem::init_keyboard_fifo()
{
#ifdef HAVE_TERMCAP
    load_termcap(NULL); // load key-codes
#endif
}

void mplayer_put_key(int code){
    MPXPSystem& MPXPSys=*mpxp_context().engine().MPXPSys;
    mp_cmd_t* cmd;
    cmd=mp_input_get_cmd_from_keys(MPXPSys.libinput(),1,&code);
    mp_input_queue_cmd(MPXPSys.libinput(),cmd);
}


static void mpxp_init_osd(const std::map<std::string,std::string>& envm) {
// check font
#ifdef USE_OSD
    if(mp_conf.font_name){
	mpxp_context().video().output->font=read_font_desc(mp_conf.font_name,mp_conf.font_factor,mp_conf.verbose>1);
	if(!mpxp_context().video().output->font)
	    mpxp_err<<MSGTR_CantLoadFont<<": "<<mp_conf.font_name<<std::endl;
    } else {
	// try default:
	mpxp_context().video().output->font=read_font_desc(get_path(envm,"font/font.desc").c_str(),mp_conf.font_factor,mp_conf.verbose>1);
	if(!mpxp_context().video().output->font)
	    mpxp_context().video().output->font=read_font_desc(DATADIR"/font/font.desc",mp_conf.font_factor,mp_conf.verbose>1);
    }
#endif
    /* Configure menu here */
    {
	std::string menu_cfg;
	menu_cfg = get_path(envm,"menu.conf");
	if(menu_init(NULL, menu_cfg.c_str()))
	    mpxp_info<<"Menu initialized: "<<menu_cfg<<std::endl;
	else {
	    menu_cfg="/etc/menu.conf";
	    if(menu_init(NULL, menu_cfg.c_str()))
		mpxp_info<<"Menu initialized: "<<menu_cfg<<std::endl;
	    else
		mpxp_warn<<"Menu init failed"<<std::endl;
	}
    }
    MP_UNIT("init_osd");
    vo_init_osd();
}

char* MPXPSystem::init_output_subsystems() {
    char* rs=NULL;
    unsigned i;
    // check video_out driver name:
    MP_UNIT("vo_init");
    if(vo_inited==MPXP_False) {
	if(!mpxp_context().video().output) mpxp_context().video().output=new(zeromem) Video_Output;
	vo_inited = mpxp_context().video().output->init(mp_conf.video_driver?mp_conf.video_driver:"");
    }

    if(vo_inited==MPXP_False){
	mpxp_fatal<<MSGTR_InvalidVOdriver<<": "<<(mp_conf.video_driver?mp_conf.video_driver:"?")<<std::endl;
	throw std::runtime_error(MSGTR_Fatal_error);
    }

// check audio_out driver name:
    MP_UNIT("ao_init");
    if (mp_conf.audio_driver)
	if ((i=strcspn(mp_conf.audio_driver, ":")) > 0)
	{
	    size_t i2 = strlen(mp_conf.audio_driver);

	    if (mp_conf.audio_driver[i] == ':')
	    {
		rs = new char [i2-i];
		if (rs != NULL)  strncpy(rs, (char *)(mp_conf.audio_driver+i+1), i2-i);
		mp_conf.audio_driver[i] = '\0';
	    }
	}
    return rs;
}

int MPXPSystem::init_vobsub(const std::string& filename) {
    int forced_subs_only=0;
    MP_UNIT("vobsub");
    if (mp_conf.vobsub_name){
      mpxp_context().video().output->vobsub=vobsub_open(mp_conf.vobsub_name,mp_conf.spudec_ifo,1,&mpxp_context().video().output->spudec);
      if(mpxp_context().video().output->vobsub==NULL)
	mpxp_err<<MSGTR_CantLoadSub<<": "<<mp_conf.vobsub_name<<std::endl;
      else {
	inited_flags|=INITED_VOBSUB;
	vobsub_set_from_lang(mpxp_context().video().output->vobsub, mp_conf.dvdsub_lang);
	// check if vobsub requested only to display forced subtitles
	forced_subs_only=vobsub_get_forced_subs_flag(mpxp_context().video().output->vobsub);
      }
    }else if(mp_conf.sub_auto && !filename.empty() && filename.length()>=5){
      /* try to autodetect vobsub from movie filename ::atmos */
      std::string buf = filename.substr(0,filename.length()-4);
      mpxp_context().video().output->vobsub=vobsub_open(buf,mp_conf.spudec_ifo,0,&mpxp_context().video().output->spudec);
    }
    if(mpxp_context().video().output->vobsub) {
      mp_conf.sub_auto=0; // don't do autosub for textsubs if vobsub found
      inited_flags|=INITED_VOBSUB;
    }
    return forced_subs_only;
}

int MPXPSystem::handle_playlist(const std::string& filename) const {
    Stream* stream=static_cast<Stream*>(_demuxer->stream);
    int eof=0;
    PlayTree* entry;
    // Handle playlist
    MP_UNIT("handle_playlist");
    mpxp_v<<"Parsing playlist "<<filename<<"..."<<std::endl;
    entry = PlayTree::parse_playtree(_libinput,stream);
    if(!entry) {
      entry = playtree_iter->get_tree();
      if(playtree_iter->step(1,0) != PLAY_TREE_ITER_ENTRY) {
	eof = PT_NEXT_ENTRY;
	return eof;
      }
      if(playtree_iter->get_tree() == entry ) { // Loop with a single file
	if(playtree_iter->up_step(1,0) != PLAY_TREE_ITER_ENTRY) {
	  eof = PT_NEXT_ENTRY;
	  return eof;
	}
      }
      entry->remove(1,1);
      eof = PT_NEXT_SRC;
      return eof;
    }
    playtree_iter->get_tree()->insert_entry(entry);
    entry = playtree_iter->get_tree();
    if(playtree_iter->step(1,0) != PLAY_TREE_ITER_ENTRY) {
      eof = PT_NEXT_ENTRY;
      return eof;
    }
    entry->remove(1,1);
    eof = PT_NEXT_SRC;
    return eof;
}

void MPXPSystem::init_dvd_nls() const {
/* Add NLS support here */
    Stream* stream=static_cast<Stream*>(_demuxer->stream);
    char *lang;
    if(!mp_conf.audio_lang) mp_conf.audio_lang=nls_get_screen_cp(envm);
    MP_UNIT("dvd lang->id");
    if(mp_conf.audio_lang) {
	lang=new char [std::max(strlen(mp_conf.audio_lang)+1,size_t(4))];
	strcpy(lang,mp_conf.audio_lang);
	if(mp_conf.audio_id==-1 && stream->ctrl(SCTRL_LNG_GET_AID,lang)==MPXP_Ok) {
	    mp_conf.audio_id=*(int *)lang;
	}
	delete lang;
    }
    if(mp_conf.dvdsub_lang) {
	lang=new char [std::max(strlen(mp_conf.dvdsub_lang)+1,size_t(4))];
	strcpy(lang,mp_conf.dvdsub_lang);
	if(mp_conf.dvdsub_id==-1 && stream->ctrl(SCTRL_LNG_GET_SID,lang)==MPXP_Ok) {
	    mp_conf.dvdsub_id=*(int *)lang;
	}
	delete lang;
    }
}

void MPXPSystem::print_stream_formats() const {
    sh_audio_t* sh_audio=reinterpret_cast<sh_audio_t*>(_demuxer->audio->sh);
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(_demuxer->video->sh);
    int fmt;
    mpxp_info<<"[Stream]:";
    if(sh_video) {
	mpxp_info<<"Video=";
	if(sh_video->bih)fmt=sh_video->bih->biCompression;
	else		 fmt=sh_video->fourcc;
	fourcc(mpxp_info,fmt);
    }
    if(sh_audio) {
	mpxp_info<<" Audio=";
	fmt=sh_audio->wtag;
	fourcc(mpxp_info,fmt);
    }
    mpxp_info<<std::endl;
}

void MPXPSystem::read_video_properties() const {
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(_demuxer->video->sh);
    Demuxer_Stream *d_video=_demuxer->video;
    MP_UNIT("video_read_properties");
    if(!sh_video->read_properties()) {
	mpxp_err<<"Video: can't read properties"<<std::endl;
	d_video->sh=NULL;
	sh_video=reinterpret_cast<sh_video_t*>(d_video->sh);
    } else {
	mpxp_v<<"[V] filefmt:"<<_demuxer->file_format
	    <<"fourcc:0x"<<std::hex<<sh_video->fourcc
	    <<" size:"<<sh_video->src_w<<"x"<<sh_video->src_h
	    <<" fps:"<<std::setprecision(2)<<sh_video->fps
	    <<" ftime:="<<1/sh_video->fps<<std::endl;
    /* need to set fps here for output encoders to pick it up in their init */
	if(mp_conf.force_fps){
	    sh_video->fps=mp_conf.force_fps;
	}

	if(!sh_video->fps && !mp_conf.force_fps){
	    mpxp_err<<MSGTR_FPSnotspecified<<std::endl;
	    d_video->sh=NULL;
	    sh_video=reinterpret_cast<sh_video_t*>(d_video->sh);
	}
    }
}

void MPXPSystem::read_subtitles(const std::string& filename,int forced_subs_only,int stream_dump_type) {
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(_demuxer->video->sh);
    Stream* stream=static_cast<Stream*>(_demuxer->stream);
    if (mp_conf.spudec_ifo) {
	unsigned int palette[16], width, height;
	MP_UNIT("spudec_init_vobsub");
	std::string buf="";
	if (vobsub_parse_ifo(NULL,mp_conf.spudec_ifo, palette, &width, &height, 1, -1, buf) >= 0)
	    mpxp_context().video().output->spudec=spudec_new_scaled(palette, sh_video->src_w, sh_video->src_h);
    }

    if (mpxp_context().video().output->spudec==NULL) {
	unsigned *pal;
	MP_UNIT("spudec_init");
	if(stream->ctrl(SCTRL_VID_GET_PALETTE,&pal)==MPXP_Ok)
	    mpxp_context().video().output->spudec=spudec_new_scaled(pal,sh_video->src_w, sh_video->src_h);
    }

    if (mpxp_context().video().output->spudec==NULL) {
	MP_UNIT("spudec_init_normal");
	mpxp_context().video().output->spudec=spudec_new_scaled(NULL, sh_video->src_w, sh_video->src_h);
	spudec_set_font_factor(mpxp_context().video().output->spudec,mp_conf.font_factor);
    }

    if (mpxp_context().video().output->spudec!=NULL) {
	inited_flags|=INITED_SPUDEC;
	// Apply current settings for forced subs
	spudec_set_forced_subs_only(mpxp_context().video().output->spudec,forced_subs_only);
    }

#ifdef USE_SUB
// after reading video params we should load subtitles because
// we know fps so now we can adjust subtitles time to ~6 seconds AST
// check .sub
    MP_UNIT("read_subtitles_file");
    if(mp_conf.sub_name){
	mpxp_context().subtitles=sub_read_file(mp_conf.sub_name, sh_video->fps);
	if(!mpxp_context().subtitles) mpxp_err<<MSGTR_CantLoadSub<<": "<<mp_conf.sub_name<<std::endl;
    } else if(mp_conf.sub_auto) { // auto load sub file ...
	mpxp_context().subtitles=sub_read_file( !filename.empty() ? sub_filename(get_path(envm,"sub/").c_str(), filename.c_str() )
				      : "default.sub", sh_video->fps );
    }
    if(mpxp_context().subtitles) {
	inited_flags|=INITED_SUBTITLE;
	if(stream_dump_type>1) list_sub_file(mpxp_context().subtitles);
    }
#endif
}

void MPXPSystem::find_acodec(const char *ao_subdevice) {
    int found=0;
    AD_Interface* mpca=NULL;
    sh_audio_t* sh_audio=reinterpret_cast<sh_audio_t*>(_demuxer->audio->sh);
    Demuxer_Stream *d_audio=_demuxer->audio;
    sh_audio->codec=NULL;
    try {
	mpca=new(zeromem) AD_Interface(*sh_audio); // try auto-probe first
	mpxp_context().audio().decoder=mpca;
	found=1;
    } catch( const missing_driver_exception& e) {
	found=0;
	mpxp_err<<MSGTR_CantFindAudioCodec<<std::endl;
	fourcc(mpxp_err,sh_audio->wtag);
	mpxp_hint<<get_path(envm,"win32codecs.conf")<<":"<<MSGTR_TryUpgradeCodecsConfOrRTFM<<std::endl;
	d_audio->sh=NULL;
	sh_audio=reinterpret_cast<sh_audio_t*>(d_audio->sh);
    }
    if(found) {
	if(!(mpxp_context().audio().output=new(zeromem) Audio_Output(ao_subdevice?ao_subdevice:""))) {
	    mpxp_err<<MSGTR_CannotInitAO<<std::endl;
	    d_audio->sh=NULL;
	    sh_audio=reinterpret_cast<sh_audio_t*>(d_audio->sh);
	}
	if(ao_subdevice) delete ao_subdevice;
	ao_inited=mpxp_context().audio().output->_register(mp_conf.audio_driver?mp_conf.audio_driver:"",0);
	if (ao_inited!=MPXP_Ok){
	    mpxp_fatal<<MSGTR_InvalidAOdriver<<": "<<mp_conf.audio_driver<<std::endl;
	    throw std::runtime_error(MSGTR_Fatal_error);
	}
    }
}

MPXP_Rc MPXPSystem::find_vcodec() {
    Demuxer_Stream *d_video=_demuxer->video;
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(_demuxer->video->sh);
    MPXP_Rc rc=MPXP_Ok;
    MP_UNIT("init_video_codec");
    sh_video->inited=0;
    mpxp_context().video().output->flags=VOFLAG_NONE;
    /* configure flags */
    if(vo_conf.fullscreen)	mpxp_context().video().output->FS_SET();
    if(vo_conf.softzoom)	mpxp_context().video().output->ZOOM_SET();
    if(vo_conf.flip>0)		mpxp_context().video().output->FLIP_SET();
    if(vo_conf.vidmode)		mpxp_context().video().output->VM_SET();
    try {
	mpxp_context().video().decoder=new(zeromem) VD_Interface(*sh_video,mp_conf.video_codec?mp_conf.video_codec:"",mp_conf.video_family?mp_conf.video_family:"",-1,_libinput);
	sh_video->inited=1;
	inited_flags|=INITED_VCODEC;
    } catch(const missing_driver_exception& e) {
	mpxp_err<<MSGTR_CantFindVideoCodec;
	fourcc(mpxp_err,sh_video->fourcc);
	mpxp_err<<std::endl;
	mpxp_hint<<get_path(envm,"win32codecs.conf")<<":"<<MSGTR_TryUpgradeCodecsConfOrRTFM<<std::endl;
	d_video->sh = NULL;
	sh_video = reinterpret_cast<sh_video_t*>(d_video->sh);
	rc=MPXP_False;
    }

    if(sh_video)
    mpxp_v<<(mp_conf.video_codec?"Forcing":"Detected")
	<<" video codec: ["<<std::string(sh_video->codec->codec_name)
	<<"] vfm:"<<std::string(sh_video->codec->driver_name)
	<<" ("<<std::string(sh_video->codec->s_info)<<std::endl;
    return rc;
}

int MPXPSystem::configure_audio() {
    sh_audio_t* sh_audio=reinterpret_cast<sh_audio_t*>(_demuxer->audio->sh);
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(_demuxer->video->sh);
    Demuxer_Stream *d_audio=_demuxer->audio;
    int rc=0;
    const ao_info_t *info=mpxp_context().audio().output->get_info();
    MP_UNIT("setup_audio");
    mpxp_v<<"AO: ["<<info->short_name<<"] "
	<<(mp_conf.force_srate?mp_conf.force_srate:sh_audio->rate)<<"Hz "
	<<(sh_audio->nch>7?"surround71":
	sh_audio->nch>6?"surround61":
	sh_audio->nch>5?"surround51":
	sh_audio->nch>4?"surround41":
	sh_audio->nch>3?"surround40":
	sh_audio->nch>2?"stereo2.1":
	sh_audio->nch>1?"Stereo":"Mono")
	<<ao_format_name(sh_audio->afmt)<<std::endl;
    mpxp_v<<"AO: Description: "<<info->name<<std::endl;
    mpxp_v<<"AO: Author: "<<info->author<<std::endl;
    if(strlen(info->comment) > 0) mpxp_v<<"AO: Comment: "<<info->comment<<std::endl;

    MP_UNIT("af_preinit");
    unsigned samplerate,channels,format;
    samplerate=mp_conf.force_srate?mp_conf.force_srate:sh_audio->rate;
    channels=mp_conf.ao_channels?mp_conf.ao_channels:sh_audio->nch;
    format=sh_audio->afmt;

    if(mpxp_context().audio().decoder->preinit_filters(
	    // input:
	    (int)(sh_audio->rate),
	    sh_audio->nch, sh_audio->afmt,
	    // output:
	    samplerate, channels, format)!=MPXP_Ok){
	    mpxp_err<<"Audio filter chain preinit failed"<<std::endl;
    } else {
	mpxp_v<<"AF_pre: "<<samplerate<<"Hz "<<channels<<"ch ("
		<<ao_format_name(format)<<
		") afmt="<<std::hex<<std::setfill('0')<<std::setw(8)<<format
		<<" sh_audio_min="<<sh_audio->audio_out_minsize<<std::endl;
    }

    if(MPXP_Ok!=mpxp_context().audio().output->configure(
		    samplerate,
		    channels,
		    format)) {
	mpxp_err<<"Can't configure audio device"<<std::endl;
	d_audio->sh=NULL;
	sh_audio=reinterpret_cast<sh_audio_t*>(d_audio->sh);
	if(sh_video == NULL) rc=-1;
    } else {
	inited_flags|=INITED_AO;
	MP_UNIT("af_init");
	if(mpxp_context().audio().decoder->init_filters(sh_audio->rate,
		sh_audio->nch, mpaf_format_e(sh_audio->afmt),
		mpxp_context().audio().output->samplerate(),
		mpxp_context().audio().output->channels(),
		mpaf_format_e(mpxp_context().audio().output->format()),
		mpxp_context().audio().output->outburst()*4,
		mpxp_context().audio().output->buffersize())!=MPXP_Ok) {
		    mpxp_err<<"No matching audio filter found!"<<std::endl;
	    }
    }
    return rc;
}

void MPXPSystem::run_ahead_engine() {
    sh_audio_t* sh_audio=reinterpret_cast<sh_audio_t*>(_demuxer->audio->sh);
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(_demuxer->video->sh);
    MP_UNIT("init_xp");
    if(sh_video && mpxp_context().engine().xp_core->num_v_buffs < 3) {/* we need at least 3 buffers to suppress screen judering */
	mpxp_fatal<<"Not enough buffers for DECODING AHEAD!"<<std::endl;
	mpxp_fatal<<"Need 3 buffers but exist only "
		<<mpxp_context().engine().xp_core->num_v_buffs<<std::endl;
	throw std::runtime_error("Try other '-vo' driver.");
    }
    if(xmp_init_engine(sh_video,sh_audio)!=0)
	throw std::runtime_error("Can't initialize decoding ahead!");
    if(xmp_run_decoders()!=0)
	throw std::runtime_error("Can't run decoding ahead!");
    if(sh_video)
	mpxp_ok<<"Using DECODING AHEAD mplayer's core with "<<mpxp_context().engine().xp_core->num_v_buffs<<" video buffers"<<std::endl;
    else
	mpxp_ok<<"Using DECODING AHEAD mplayer's core with "<<mpxp_context().engine().xp_core->num_a_buffs<<" audio buffers"<<std::endl;
/* reset counters */
    if(sh_video) mpxp_context().engine().xp_core->video->num_dropped_frames=0;
    inited_flags|=INITED_XMP;
}

void MPXPSystem::print_audio_status() const {
    sh_audio_t* sh_audio=reinterpret_cast<sh_audio_t*>(_demuxer->audio->sh);
    /* PAINT audio OSD */
    unsigned ipts,rpts;
    unsigned h,m,s,rh,rm,rs;
    static unsigned ph=0,pm=0,ps=0;
    ipts=(unsigned)(sh_audio->timer-mpxp_context().audio().output->get_delay());
    rpts=_demuxer->movi_length-ipts;
    h = ipts/3600;
    m = (ipts/60)%60;
    s = ipts%60;
    if(_demuxer->movi_length!=UINT_MAX) {
	rh = rpts/3600;
	rm = (rpts/60)%60;
	rs = rpts%60;
    } else rh=rm=rs=0;
    if(h != ph || m != pm || s != ps) {
	mpxp_status<<">"<<std::setfill('0')<<std::setw(2)<<h<<":"
			<<std::setfill('0')<<std::setw(2)<<m<<":"
			<<std::setfill('0')<<std::setw(2)<<s<<"("
			<<std::setfill('0')<<std::setw(2)<<rh<<":"
			<<std::setfill('0')<<std::setw(2)<<rm<<":"
			<<std::setfill('0')<<std::setw(2)<<rs<<")\r";
	mpxp_status.flush();
	ph = h;
	pm = m;
	ps = s;
    }
}

#ifdef USE_OSD
int MPXPSystem::paint_osd(int* osd_visible,int* in_pause) {
    Stream* stream=static_cast<Stream*>(_demuxer->stream);
    sh_audio_t* sh_audio=reinterpret_cast<sh_audio_t*>(_demuxer->audio->sh);
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(_demuxer->video->sh);
    int rc=0;
    if(*osd_visible) {
	if (!--(*osd_visible)) {
	    mpxp_context().video().output->osd_progbar_type=-1; // disable
	    vo_osd_changed(OSDTYPE_PROGBAR);
	    if (!((osd_function == OSD_PAUSE)||(osd_function==OSD_DVDMENU)))
		osd_function = OSD_PLAY;
	}
    }
    if(osd_function==OSD_DVDMENU) {
	rect_highlight_t hl;
	if(stream->ctrl(SCTRL_VID_GET_HILIGHT,&hl)==MPXP_Ok) {
	    osd_set_nav_box (hl.sx, hl.sy, hl.ex, hl.ey);
	    mpxp_v<<"Set nav box: "<<hl.sx<<" "<<hl.sy<<" "<<hl.ex<<" "<<hl.ey<<std::endl;
	    vo_osd_changed (OSDTYPE_DVDNAV);
	}
    }
    if(osd_function==OSD_PAUSE||osd_function==OSD_DVDMENU) {
	mp_cmd_t* cmd;
	if (vo_inited && sh_video) {
	    if(mp_conf.osd_level>1 && !*in_pause) {
		*in_pause = 1;
		return -1;
	    }
	    mpxp_context().video().output->pause();
	}
	if(mp_conf.verbose) {
	    mpxp_status<<std::endl<<"------ PAUSED -------\r";
	    mpxp_status.flush();
	}

	if (ao_inited==MPXP_Ok && sh_audio) {
	    if(xmp_test_model(XMP_Run_AudioPlayer)) {
		mpxp_context().engine().xp_core->in_pause=1;
		while( !dec_ahead_can_aseek ) yield_timeslice();
	    }
	    mpxp_context().audio().output->pause();	// pause audio, keep data if possible
	}

	while( (cmd = mp_input_get_cmd(_libinput,20,1,1)) == NULL) {
	    if(sh_video && vo_inited) mpxp_context().video().output->check_events();
	    yield_timeslice();
	}

	if (cmd && cmd->id == MP_CMD_PAUSE) {
	    cmd = mp_input_get_cmd(_libinput,0,1,0);
	    mp_cmd_free(cmd);
	}

	if(osd_function==OSD_PAUSE) osd_function=OSD_PLAY;
	if (ao_inited==MPXP_Ok && sh_audio) {
	    mpxp_context().audio().output->resume();	// resume audio
	    if(xmp_test_model(XMP_Run_AudioPlayer)) {
		mpxp_context().engine().xp_core->in_pause=0;
		__MP_SYNCHRONIZE(audio_play_mutex,pthread_cond_signal(&audio_play_cond));
	    }
	}
	if (vo_inited && sh_video)
	    mpxp_context().video().output->resume();	// resume video
	*in_pause=0;
	(void)GetRelativeTime();	// keep TF around FT in next cycle
    }
    return rc;
}
#endif

int MPXPSystem::handle_input(seek_args_t* _seek,osd_args_t* osd,input_state_t* state) {
    Stream* stream=static_cast<Stream*>(_demuxer->stream);
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(_demuxer->video->sh);
    int v_bright=0;
    int v_cont=0;
    int v_hue=0;
    int v_saturation=0;
/*
For future:
  int v_red_intensity=0;
  int v_green_intensity=0;
  int v_blue_intensity=0;
*/
    int eof=0;
    mp_cmd_t* cmd;
    while( (cmd = mp_input_get_cmd(_libinput,0,0,0)) != NULL) {
	switch(cmd->id) {
	    case MP_CMD_SEEK : {
		int v,i_abs;
		v = cmd->args[0].v.i;
		i_abs = (cmd->nargs > 1) ? cmd->args[1].v.i : 0;
		if(i_abs) {
		    _seek->flags = DEMUX_SEEK_SET|DEMUX_SEEK_PERCENTS;
		    if(sh_video) osd_function= (v > dae_played_frame(mpxp_context().engine().xp_core->video).v_pts) ? OSD_FFW : OSD_REW;
		    _seek->secs = v/100.;
		} else {
		    _seek->flags = DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS;
		    if(sh_video) osd_function= (v > 0) ? OSD_FFW : OSD_REW;
		    _seek->secs+= v;
		}
	    } break;
	    case MP_CMD_SPEED_INCR :
	    case MP_CMD_SPEED_MULT :
	    case MP_CMD_SPEED_SET :
		mpxp_warn<<"Speed adjusting is not implemented yet!"<<std::endl;
		break;
	    case MP_CMD_SWITCH_AUDIO :
		mpxp_info<<"ID_AUDIO_TRACK="<<demuxer_switch_audio_r(*_demuxer, _demuxer->audio->id+1)<<std::endl;
		break;
	    case MP_CMD_SWITCH_VIDEO :
		mpxp_info<<"ID_VIDEO_TRACK="<<demuxer_switch_video_r(*_demuxer, _demuxer->video->id+1)<<std::endl;
		break;
	    case MP_CMD_SWITCH_SUB :
		mpxp_info<<"ID_SUB_TRACK="<<demuxer_switch_subtitle_r(*_demuxer, _demuxer->sub->id+1)<<std::endl;
		break;
	    case MP_CMD_FRAME_STEP :
	    case MP_CMD_PAUSE :
		osd_function=OSD_PAUSE;
		break;
	    case MP_CMD_SOFT_QUIT :
		exit_player(MSGTR_Exit_quit);
	    case MP_CMD_QUIT :
		 exit_player(MSGTR_Exit_quit);
	    case MP_CMD_PLAY_TREE_STEP : {
		int n = cmd->args[0].v.i > 0 ? 1 : -1;
		PlayTree_Iter* it = new PlayTree_Iter(*playtree_iter);

		if(it->step(n,0) == PLAY_TREE_ITER_ENTRY)
		    eof = (n > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
		delete it;
	    } break;
	    case MP_CMD_PLAY_TREE_UP_STEP : {
		int n = cmd->args[0].v.i > 0 ? 1 : -1;
		PlayTree_Iter* it = new PlayTree_Iter(*playtree_iter);
		if(it->up_step(n,0) == PLAY_TREE_ITER_ENTRY)
		    eof = (n > 0) ? PT_UP_NEXT : PT_UP_PREV;
		delete it;
	    } break;
	    case MP_CMD_PLAY_ALT_SRC_STEP :
		if(playtree_iter->get_num_files() > 1) {
		    int v = cmd->args[0].v.i;
		    if(v > 0 && playtree_iter->get_file() < playtree_iter->get_num_files())
			eof = PT_NEXT_SRC;
		    else if(v < 0 && playtree_iter->get_file() > 1)
			eof = PT_PREV_SRC;
		}
		break;
	    case MP_CMD_OSD :
		if(sh_video) {
		    int v = cmd->args[0].v.i;
		    if(v < 0)	mp_conf.osd_level=(mp_conf.osd_level+1)%4;
		    else	mp_conf.osd_level= v > 3 ? 3 : v;
		}
		break;
	    case MP_CMD_MUTE:
		mpxp_context().audio().output->mixer_mute();
		break;
	    case MP_CMD_VOLUME :  {
		int v = cmd->args[0].v.i;
		if(v > 0)	mpxp_context().audio().output->mixer_incvolume();
		else		mpxp_context().audio().output->mixer_decvolume();
#ifdef USE_OSD
		if(mp_conf.osd_level){
		    osd->visible=sh_video->fps; // 1 sec
		    mpxp_context().video().output->osd_progbar_type=OSD_VOLUME;
		    mpxp_context().video().output->osd_progbar_value=(mpxp_context().audio().output->mixer_getbothvolume()*256.0)/100.0;
		    vo_osd_changed(OSDTYPE_PROGBAR);
		}
#endif
	    } break;
	    case MP_CMD_CONTRAST :  {
		int v = cmd->args[0].v.i, i_abs = cmd->args[1].v.i;
		if(i_abs)	v_cont=v;
		else		v_cont+=v;
		if(v_cont > 100) v_cont=100;
		if(v_cont < -100) v_cont = -100;
		if(mpxp_context().video().decoder->set_colors(VO_EC_CONTRAST,v_cont)==MPXP_Ok) {
#ifdef USE_OSD
		    if(mp_conf.osd_level){
			osd->visible=sh_video->fps; // 1 sec
			mpxp_context().video().output->osd_progbar_type=OSD_CONTRAST;
			mpxp_context().video().output->osd_progbar_value=((v_cont)<<8)/100;
			mpxp_context().video().output->osd_progbar_value = ((v_cont+100)<<8)/200;
			vo_osd_changed(OSDTYPE_PROGBAR);
		    }
#endif
		}
	    } break;
	    case MP_CMD_BRIGHTNESS :  {
		int v = cmd->args[0].v.i, i_abs = cmd->args[1].v.i;
		if(i_abs)	v_bright=v;
		else		v_bright+=v;
		if(v_bright > 100) v_bright = 100;
		if(v_bright < -100) v_bright = -100;
		if(mpxp_context().video().decoder->set_colors(VO_EC_BRIGHTNESS,v_bright)==MPXP_Ok) {
#ifdef USE_OSD
		    if(mp_conf.osd_level){
			osd->visible=sh_video->fps; // 1 sec
			mpxp_context().video().output->osd_progbar_type=OSD_BRIGHTNESS;
			mpxp_context().video().output->osd_progbar_value=((v_bright)<<8)/100;
			mpxp_context().video().output->osd_progbar_value = ((v_bright+100)<<8)/200;
			vo_osd_changed(OSDTYPE_PROGBAR);
		    }
#endif
		}
	    } break;
	    case MP_CMD_HUE :  {
		int v = cmd->args[0].v.i, i_abs = cmd->args[1].v.i;
		if(i_abs)	v_hue=v;
		else		v_hue+=v;
		if(v_hue > 100) v_hue = 100;
		if(v_hue < -100) v_hue = -100;
		if(mpxp_context().video().decoder->set_colors(VO_EC_HUE,v_hue)==MPXP_Ok) {
#ifdef USE_OSD
		    if(mp_conf.osd_level){
			osd->visible=sh_video->fps; // 1 sec
			mpxp_context().video().output->osd_progbar_type=OSD_HUE;
			mpxp_context().video().output->osd_progbar_value=((v_hue)<<8)/100;
			mpxp_context().video().output->osd_progbar_value = ((v_hue+100)<<8)/200;
			vo_osd_changed(OSDTYPE_PROGBAR);
		    }
#endif
		}
	    } break;
	    case MP_CMD_SATURATION :  {
		int v = cmd->args[0].v.i, i_abs = cmd->args[1].v.i;
		if(i_abs)	v_saturation=v;
		else		v_saturation+=v;
		if(v_saturation > 100) v_saturation = 100;
		if(v_saturation < -100) v_saturation = -100;
		if(mpxp_context().video().decoder->set_colors(VO_EC_SATURATION,v_saturation)==MPXP_Ok) {
#ifdef USE_OSD
		    if(mp_conf.osd_level){
			osd->visible=sh_video->fps; // 1 sec
			mpxp_context().video().output->osd_progbar_type=OSD_SATURATION;
			mpxp_context().video().output->osd_progbar_value=((v_saturation)<<8)/100;
			mpxp_context().video().output->osd_progbar_value = ((v_saturation+100)<<8)/200;
			vo_osd_changed(OSDTYPE_PROGBAR);
		    }
#endif
		}
	    } break;
	    case MP_CMD_FRAMEDROPPING : {
		int v = cmd->args[0].v.i;
		if(v < 0)	mp_conf.frame_dropping = (mp_conf.frame_dropping+1)%3;
		else		mp_conf.frame_dropping = v > 2 ? 2 : v;
		osd_show_framedrop = osd->info_factor;
	    } break;
	    case MP_CMD_TV_STEP_CHANNEL:
		if(cmd->args[0].v.i > 0)cmd->id=MP_CMD_TV_STEP_CHANNEL_UP;
		else			cmd->id=MP_CMD_TV_STEP_CHANNEL_DOWN;
	    case MP_CMD_TV_STEP_NORM:
	    case MP_CMD_TV_STEP_CHANNEL_LIST:
		stream->ctrl(SCRTL_MPXP_CMD,(any_t*)cmd->id);
		break;
	    case MP_CMD_DVDNAV:
		if(stream->ctrl(SCRTL_MPXP_CMD,(any_t*)cmd->args[0].v.i)==MPXP_Ok) {
		    if(cmd->args[0].v.i!=MP_CMD_DVDNAV_SELECT) {
			    stream->type(Stream::Type_Menu);
			    state->need_repaint=1;
		    }
		    osd_function=OSD_DVDMENU;
		    if(cmd->args[0].v.i==MP_CMD_DVDNAV_SELECT) {
			osd_function=0;
			state->need_repaint=1;
			state->after_dvdmenu=1;
			state->next_file=1;
			return eof;
		    }
		}
		break;
	    case MP_CMD_VO_FULLSCREEN:
		mpxp_context().video().output->fullscreen();
		break;
	    case MP_CMD_VO_SCREENSHOT:
		mpxp_context().video().output->screenshot(dae_curr_vplayed(mpxp_context().engine().xp_core));
		break;
	    case MP_CMD_SUB_POS: {
		int v;
		v = cmd->args[0].v.i;

		sub_data.pos+=v;
		if(sub_data.pos >100) sub_data.pos=100;
		if(sub_data.pos <0) sub_data.pos=0;
		vo_osd_changed(OSDTYPE_SUBTITLE);
	    } break;
	    default :
		mpxp_err<<"Received unknow cmd "<<cmd->name<<std::endl;
	}
	mp_cmd_free(cmd);
    }
    return eof;
}

PlayTree_Iter& mpxp_get_playtree_iter() { return *(mpxp_context().engine().MPXPSys->playtree_iter); }
const std::map<std::string,std::string>& mpxp_get_environment() { return mpxp_context().engine().MPXPSys->envm; }
/******************************************\
* MAIN MPLAYERXP FUNCTION !!!              *
\******************************************/
int MPlayerXP(const std::vector<std::string>& argv, const std::map<std::string,std::string>& envm){
//    mpxp_test_backtrace();
    Stream* stream=NULL;
    int stream_dump_type=0;
    input_state_t input_state = { 0, 0, 0 };
    const char *ao_subdevice;
    std::string filename=""; //"MI2-Trailer.avi";
    int file_format=Demuxer::Type_UNKNOWN;

// movie info:
    int eof=0;
    osd_args_t osd = { 100, 9 };
    int forced_subs_only=0;
    seek_args_t seek_args = { 0, DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS };

    // Yes, it really must be placed in stack or in very secret place
    PointerProtector<MPXPSecureKeys> ptr_protector;
    secure_keys=ptr_protector.protect(new(zeromem) MPXPSecureKeys(10));

    init_signal_handling();

    xmp_init();
    xmp_register_main(exit_sighandler);

    mpxp_print_init(mp_conf.verbose+MSGL_STATUS);
    for(unsigned j=0;banner_text[j];j++)  mpxp_info<<banner_text[j]<<std::endl;

    /* currently it's lowest point of MPXPSystem initialization */
    mpxp_context().engine().MPXPSys = new(zeromem) MPXPSystem(envm);
    MPXPSystem& MPXPSys=*mpxp_context().engine().MPXPSys;
    MPXPSys.init_keyboard_fifo();

    MPXPSys.playtree = new(zeromem) PlayTree();

    M_Config& m_config=*new(zeromem) M_Config(MPXPSys.playtree,MPXPSys.libinput());
    mpxp_context().mconfig = &m_config;
    m_config.register_options(mplayerxp_opts);
    // TODO : add something to let modules register their options
    mp_register_options(m_config);
    m_config.parse_cfgfiles(envm);

    if(m_config.parse_command_line(argv,envm)!=MPXP_Ok)
	throw std::runtime_error("Error parse command line"); // error parsing cmdline

    if(!mp_conf.xp) {
	mpxp_err<<"Note!  Single-thread mode is not longer supported by MPlayerXP"<<std::endl;
	throw std::runtime_error("Error: detected option: -core.xp=0");
    }
    if(mp_conf.test_av) {
	int verb=1;
	if(mpxp_test_antiviral_protection(&verb)==MPXP_Virus)
	    throw std::runtime_error("Bad test of antiviral protection");
    }

    MPXPSys.xp_num_cpu=get_number_cpu();
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
    get_mmx_optimizations();
#endif
    if(mp_conf.shuffle_playback) MPXPSys.playtree->set_flags(MPXPSys.playtree->get_flags()|PLAY_TREE_RND);
    else			 MPXPSys.playtree->set_flags(MPXPSys.playtree->get_flags()&~PLAY_TREE_RND);

//    MPXPSys.playtree = play_tree_cleanup(MPXPSys.playtree);
    if(MPXPSys.playtree) {
      MPXPSys.playtree_iter = new PlayTree_Iter(MPXPSys.playtree,m_config);
      if(MPXPSys.playtree_iter) {
	if(MPXPSys.playtree_iter->step(0,0) != PLAY_TREE_ITER_ENTRY) {
	  delete MPXPSys.playtree_iter;
	  MPXPSys.playtree_iter = NULL;
	}
	else filename = MPXPSys.playtree_iter->get_playable_source_name(1);
      }
    }

    mpxp_context().engine().xp_core->num_a_buffs = vo_conf.xp_buffs;

    if(init_player(envm)!=MPXP_Ok) return EXIT_SUCCESS;

    if(filename.empty()){
	show_help();
	exit_player(MSGTR_Exit_quit);
    }

    // Many users forget to include command line in bugreports...
    if(mp_conf.verbose){
	std::map<std::string,std::string>::const_iterator it;
	mpxp_info<<"Environment:"<<std::endl;
	for(it=envm.begin();it!=envm.end();it++)
	    mpxp_info<<(*it).first<<" => "<<(*it).second<<std::endl;
	size_t i,sz=argv.size();
	mpxp_info<<"CommandLine:";
	for(i=1;i<sz;i++) mpxp_info<<" '"<<argv[i]<<"'";
	mpxp_info<<std::endl;
    }

    mpxp_context().video().output=new(zeromem) Video_Output;
//------ load global data first ------
    mpxp_init_osd(envm);
// ========== Init keyboard FIFO (connection to libvo) ============

    MP_UNIT(NULL);


// ******************* Now, let's see the per-file stuff ********************
play_next_file:

    ao_subdevice=MPXPSys.init_output_subsystems();
    if(!filename.empty()) mpxp_ok<<MSGTR_Playing<<" "<<filename<<std::endl;

    forced_subs_only=MPXPSys.init_vobsub(filename);

    MP_UNIT("mplayer");
    if(!input_state.after_dvdmenu && MPXPSys.demuxer()) {
	delete MPXPSys.demuxer()->stream;
	MPXPSys.demuxer()->stream=NULL;
	MPXPSys.inited_flags&=~INITED_STREAM;
	MPXPSys.uninit_demuxer();
    }
    if(MPXPSys.demuxer()) {
	MPXPSys.demuxer()->audio=NULL;
	MPXPSys.demuxer()->video=NULL;
	MPXPSys.demuxer()->sub=NULL;
	MPXPSys.demuxer()->audio->sh=NULL;
	MPXPSys.demuxer()->video->sh=NULL;
    }
//============ Open & Sync STREAM --- fork cache2 ====================
    stream_dump_type=0;
    if(mp_conf.stream_dump)
	if((stream_dump_type=dump_parse(mp_conf.stream_dump))==0) {
	    mpxp_err<<"Wrong dump parameters! Unable to continue"<<std::endl;
	    throw std::runtime_error(MSGTR_Fatal_error);
	}

    if(stream_dump_type) mp_conf.s_cache_size=0;
    MP_UNIT("open_stream");
    // CACHE2: initial prefill: 20%  later: 5%  (should be set by -cacheopts)
    if(!input_state.after_dvdmenu)
	stream=(mp_conf.s_cache_size && !stream_dump_type)?
		new(zeromem) Cached_Stream(MPXPSys.libinput(),mp_conf.s_cache_size*1024,mp_conf.s_cache_size*1024/5,mp_conf.s_cache_size*1024/20):
		new(zeromem) Stream;
    if(stream->open(MPXPSys.libinput(),filename,&file_format)!=MPXP_Ok) { // error...
	mpxp_err<<"Can't open: "<<filename<<std::endl;
	eof = MPXPSys.libmpdemux_was_interrupted(PT_NEXT_ENTRY);
	goto goto_next_file;
    }
    MPXPSys.inited_flags|=INITED_STREAM;

    if(stream->type() & Stream::Type_Text) {
	eof=MPXPSys.handle_playlist(filename);
	goto goto_next_file;
    }

    MP_UNIT(NULL);

    // DUMP STREAMS:
    if(stream_dump_type==1) dump_stream(stream);

//============ Open MPXPSys.demuxer()S --- DETECT file type =======================
    if(mp_conf.playbackspeed_factor!=1.0) mp_conf.has_audio=0;
    mpxp_context().engine().xp_core->initial_apts=HUGE;
    if(!mp_conf.has_audio) mp_conf.audio_id=-2;  // do NOT read audio packets...
    if(!mp_conf.has_video) mp_conf.video_id=-2;  // do NOT read video packets...
    if(!mp_conf.has_dvdsub) mp_conf.dvdsub_id=-2;// do NOT read subtitle packets...

    MP_UNIT("demux_open");

    if(!input_state.after_dvdmenu) MPXPSys.assign_demuxer(Demuxer::open(stream,MPXPSys.libinput(),mp_conf.audio_id,mp_conf.video_id,mp_conf.dvdsub_id));
    if(!MPXPSys.demuxer()) goto goto_next_file;
    input_state.after_dvdmenu=0;

    Demuxer_Stream *d_video;
    Demuxer_Stream *d_audio;
    Demuxer_Stream *d_dvdsub;
    d_audio=MPXPSys.demuxer()->audio;
    d_video=MPXPSys.demuxer()->video;
    d_dvdsub=MPXPSys.demuxer()->sub;

/* Add NLS support here */
    MPXPSys.init_dvd_nls();

    if(mp_conf.seek_to_byte) stream->skip(mp_conf.seek_to_byte);

    sh_audio_t* sh_audio;
    sh_video_t* sh_video;

    sh_audio=reinterpret_cast<sh_audio_t*>(MPXPSys.demuxer()->audio->sh);
    sh_video=reinterpret_cast<sh_video_t*>(MPXPSys.demuxer()->video->sh);

    MPXPSys.print_stream_formats();

    if(sh_video) MPXPSys.read_video_properties();

    fflush(stdout);

    if(!sh_video && !sh_audio) {
	mpxp_fatal<<"No stream found"<<std::endl;
	goto goto_next_file;
    }

//================== Read SUBTITLES (DVD & TEXT) ==========================
    if(sh_video) MPXPSys.read_subtitles(filename,forced_subs_only,stream_dump_type);

//================== Init AUDIO (codec) ==========================
    MP_UNIT("init_audio_codec");

    if(sh_audio) MPXPSys.find_acodec(ao_subdevice);
    sh_audio=reinterpret_cast<sh_audio_t*>(MPXPSys.demuxer()->audio->sh);

    if(stream_dump_type>1) {
	dump_mux_init(MPXPSys.demuxer(),MPXPSys.libinput());
	goto dump_file;
    }

    /* is it non duplicate block fro find_acodec() ??? */
    if(sh_audio){
	mpxp_v<<"Initializing audio codec..."<<std::endl;
	if(!mpxp_context().audio().decoder) {
	    try {
		mpxp_context().audio().decoder=new(zeromem) AD_Interface(*sh_audio);
	    } catch(const missing_driver_exception& e) {
		mpxp_err<<MSGTR_CouldntInitAudioCodec<<std::endl;
		d_audio->sh=NULL;
		sh_audio=reinterpret_cast<sh_audio_t*>(d_audio->sh);
	    }
	}
	if(sh_audio) {
	    mpxp_v<<"AUDIO: srate="<<sh_audio->rate
	    <<" chans="<<sh_audio->nch
	    <<" bps="<<afmt2bps(sh_audio->afmt)
	    <<" sfmt=0x"<<std::hex<<sh_audio->afmt
	    <<" ratio: "<<sh_audio->i_bps<<"->"<<sh_audio->af_bps<<std::endl;
	}
    }

    if(sh_audio)   MPXPSys.inited_flags|=INITED_ACODEC;

    if(stream_dump_type>1) {
	dump_file:
	dump_mux(MPXPSys.demuxer(),mp_conf.av_sync_pts,mp_conf.seek_to_sec,mp_conf.play_n_frames);
	goto goto_next_file;
    }
/*================== Init VIDEO (codec & libvo) ==========================*/
    if(!sh_video) goto main;

    if((MPXPSys.find_vcodec())!=MPXP_Ok) {
	sh_video=reinterpret_cast<sh_video_t*>(MPXPSys.demuxer()->video->sh);
	if(!sh_audio) goto goto_next_file;
	goto main;
    }

    mpxp_context().engine().xp_core->num_v_buffs=mpxp_context().video().output->get_num_frames(); /* that really known after init_vcodecs */

    if(mp_conf.autoq>0){
	/* Auto quality option enabled*/
	MPXP_Rc rc;
	unsigned quality;
	rc=mpxp_context().video().decoder->get_quality_max(quality);
	if(rc==MPXP_Ok) mpxp_context().output_quality=quality;
	if(mp_conf.autoq>mpxp_context().output_quality) mp_conf.autoq=mpxp_context().output_quality;
	else mpxp_context().output_quality=mp_conf.autoq;
	mpxp_v<<"AutoQ: setting quality to "<<mpxp_context().output_quality<<std::endl;
	mpxp_context().video().decoder->set_quality(mpxp_context().output_quality);
    }

// ========== Init display (sh_video->src_w*sh_video->src_h/out_fmt) ============

    MPXPSys.inited_flags|=INITED_VO;
    mpxp_v<<"INFO: Video OUT driver init OK!"<<std::endl;
    MP_UNIT("init_libvo");
    fflush(stdout);

//================== MAIN: ==========================
main:
    if(!sh_video) mp_conf.osd_level = 0;
    else if(sh_video->fps<60) osd.info_factor=sh_video->fps/2; /* 0.5 sec */

//================ SETUP AUDIO ==========================

    if(sh_audio) if((MPXPSys.configure_audio())!=0) goto goto_next_file;

    MP_UNIT("av_init");

    if(mp_conf.av_force_pts_fix2==1 ||
	(mp_conf.av_force_pts_fix2==-1 && mp_conf.av_sync_pts &&
	(d_video->demuxer->file_format == Demuxer::Type_MPEG_ES ||
	d_video->demuxer->file_format == Demuxer::Type_MPEG4_ES ||
	d_video->demuxer->file_format == Demuxer::Type_H264_ES ||
	d_video->demuxer->file_format == Demuxer::Type_MPEG_PS ||
	d_video->demuxer->file_format == Demuxer::Type_MPEG_TS)))
	    mpxp_context().use_pts_fix2=1;
    else
	    mpxp_context().use_pts_fix2=0;

    if(sh_video) sh_video->chapter_change=0;

    if(sh_audio) { // <- ??? always true
	sh_audio->chapter_change=0;
	sh_audio->a_pts=HUGE;
    } else {
	mpxp_info<<MSGTR_NoSound<<std::endl;
	d_audio->free_packs(); // mp_free buffered chunks
	d_audio->id=-2;         // do not read audio chunks
	if(MPXPSys.ao_inited==MPXP_Ok) MPXPSys.uninit_player(INITED_AO); // close device
    }

    if(!sh_video){
	mpxp_info<<"Video: no video!!!"<<std::endl;
	d_video->free_packs();
	d_video->id=-2;
	if(MPXPSys.vo_inited) MPXPSys.uninit_player(INITED_VO);
    }

    if(!sh_audio && !sh_video) throw std::runtime_error("Nothing to do");

    if(mp_conf.force_fps && sh_video) {
	sh_video->fps=mp_conf.force_fps;
	mpxp_info<<MSGTR_FPSforced<<sh_video->fps<<"(fitme: "<<1.0f/sh_video->fps<<")"<<std::endl;
    }

    /* Init timers and benchmarking */
    mpxp_context().rtc_fd=InitTimer();
    if(!mp_conf.nortc && mpxp_context().rtc_fd>0) { close(mpxp_context().rtc_fd); mpxp_context().rtc_fd=-1; }
    mpxp_v<<"Using "<<(mpxp_context().rtc_fd>0?"rtc":mp_conf.softsleep?"software":"usleep()")<<" timing"<<std::endl;

    mpxp_context().bench->total_start=GetTimer();
    mpxp_context().bench->audio=0; mpxp_context().bench->audio_decode=0; mpxp_context().bench->video=0;
    mpxp_context().bench->audio_decode_correction=0;

    if(mp_conf.benchmark) init_benchmark();

    /* display clip info */
    MPXPSys.demuxer()->info().print(filename);

// TODO: rewrite test backtrace in .asm
//    mpxp_test_backtrace();
    MPXPSys.run_ahead_engine();

    fflush(stdout);
    fflush(stderr);
/*
   let thread will decode ahead!
   We may print something in block window ;)
 */
    mpxp_context().seek_time = GetTimerMS();

    if(sh_video) dae_wait_decoder_outrun(mpxp_context().engine().xp_core->video);

// TODO: rewrite test backtrace in .asm
//    mpxp_test_backtrace();
    if(xmp_run_players()!=0) throw std::runtime_error("Can't run xp players!");
    mpxp_ok<<"Using the next "<<mpxp_context().engine().xp_core->num_threads<<" threads:"<<std::endl;
    unsigned idx;
    for(idx=0;idx<mpxp_context().engine().xp_core->num_threads;idx++)
	mpxp_ok<<"["<<idx<<"] "
	<<mpxp_context().engine().xp_core->mpxp_threads[idx]->name
	<<" (id="<<mpxp_context().engine().xp_core->mpxp_threads[idx]->pid
	<<" pth_id="<<mpxp_context().engine().xp_core->mpxp_threads[idx]->pth_id
	<<")"<<std::endl;
//==================== START PLAYING =======================

    mpxp_ok<<MSGTR_Playing<<"..."<<std::endl;

    mpxp_print_flush();
    while(!eof){
	int in_pause=0;

/*========================== UPDATE TIMERS ============================*/
	MP_UNIT("Update timers");
	if(sh_audio) eof = mpxp_context().engine().xp_core->audio->eof;
	if(sh_video) eof|=dae_played_eof(mpxp_context().engine().xp_core->video);
	if(eof) break;
	if(!sh_video) {
	    if(mp_conf.benchmark && mp_conf.verbose) show_benchmark_status();
	    else MPXPSys.print_audio_status();
	}
	usleep(250000);
	if(sh_video) mpxp_context().video().output->check_events();
#ifdef USE_OSD
	while((MPXPSys.paint_osd(&osd.visible,&in_pause))!=0);
#endif

//================= Keyboard events, SEEKing ====================

	memset(&input_state,0,sizeof(input_state_t));
	eof|=MPXPSys.handle_input(&seek_args,&osd,&input_state);
	if(input_state.next_file) goto goto_next_file;

	if (mp_conf.seek_to_sec) {
	    int a,b;
	    float d;
	    char c;
	    int ok=1;
	    std::istringstream iss(mp_conf.seek_to_sec);

	    iss>>a; iss>>c;
	    if(!iss.good() || c!=':') ok=0;
	    iss>>b; iss>>c;
	    if(!iss.good() || c!=':') ok=0;
	    iss>>d;
	    if(!iss.good()) ok=0;
	    if (ok) seek_args.secs += 3600*a +60*b +d ;
	    else {
		ok=1;
		iss.str(mp_conf.seek_to_sec);
		iss>>a; iss>>c;
		if(!iss.good() || c!=':') ok=0;
		iss>>b;
		if(!iss.good()) ok=0;
		if (ok)	seek_args.secs += 60*a +d;
		else {
		    iss.str(mp_conf.seek_to_sec);
		    iss>>d;
		    if (iss.good()) seek_args.secs += d;
		}
	    }
	    mp_conf.seek_to_sec = NULL;
	}
  /* Looping. */
	if(eof && mp_conf.loop_times>=0) {
	    mpxp_v<<"loop_times = "<<mp_conf.loop_times<<", eof = "<<eof<<std::endl;

	    if(mp_conf.loop_times>1) mp_conf.loop_times--; else
	    if(mp_conf.loop_times==1) mp_conf.loop_times=-1;

	    eof=0;
	    mpxp_context().engine().xp_core->audio->eof=0;
	    seek_args.flags=DEMUX_SEEK_SET|DEMUX_SEEK_PERCENTS;
	    seek_args.secs=0; // seek to start of movie (0%)
	}

	if(seek_args.secs || (seek_args.flags&DEMUX_SEEK_SET)) {
	    MP_UNIT("seek");

	    xmp_halt_threads(0);

	    if(seek_args.secs && sh_video) {
	    xmp_frame_t shvap = dae_played_frame(mpxp_context().engine().xp_core->video);
	    xmp_frame_t shvad = dae_prev_decoded_frame(mpxp_context().engine().xp_core->video);
		seek_args.secs -= (mpxp_context().engine().xp_core->bad_pts?shvad.v_pts:d_video->pts)-shvap.v_pts;
	    }

	    MPXPSys.seek(&osd,&seek_args);

	    mpxp_context().engine().xp_core->audio->eof=0;
	    seek_args.secs=0;
	    seek_args.flags=DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS;

	    xmp_restart_threads(main_id);
/* Disable threads for DVD menus */
	    MP_UNIT(NULL);
	}
#ifdef USE_OSD
	update_osd(d_video->pts);
#endif
    } // while(!eof)

    mpxp_v<<"EOF code: "<<eof<<std::endl;

goto_next_file:  // don't jump here after ao/vo/getch initialization!

    if(mp_conf.benchmark) show_benchmark();

    if(MPXPSys.playtree_iter != NULL && !input_state.after_dvdmenu) {
	if(eof == PT_NEXT_ENTRY || eof == PT_PREV_ENTRY) {
	    eof = eof == PT_NEXT_ENTRY ? 1 : -1;
	    if(MPXPSys.playtree_iter->step(eof,0) == PLAY_TREE_ITER_ENTRY) {
		MPXPSys.uninit_player(INITED_ALL-(INITED_LIRC+INITED_INPUT+INITED_VO));
		eof = 1;
	    } else {
		delete MPXPSys.playtree_iter;
		MPXPSys.playtree_iter = NULL;
	    }
	} else if (eof == PT_UP_NEXT || eof == PT_UP_PREV) {
	    eof = eof == PT_UP_NEXT ? 1 : -1;
	    if(MPXPSys.playtree_iter->up_step(eof,0) == PLAY_TREE_ITER_ENTRY) {
		MPXPSys.uninit_player(INITED_ALL-(INITED_LIRC+INITED_INPUT+INITED_VO));
		eof = 1;
	    } else {
		delete MPXPSys.playtree_iter;
		MPXPSys.playtree_iter = NULL;
	    }
	} else { // NEXT PREV SRC
	    MPXPSys.uninit_player(INITED_ALL-(INITED_LIRC+INITED_INPUT+INITED_VO+INITED_DEMUXER));
	    eof = eof == PT_PREV_SRC ? -1 : 1;
	}
    }
    MPXPSys.uninit_player(INITED_VO);

    if(eof == 0) eof = 1;

    if(!input_state.after_dvdmenu)
	while(MPXPSys.playtree_iter != NULL) {
	    filename = MPXPSys.playtree_iter->get_playable_source_name(eof);
	    if(filename.empty()) {
		if( MPXPSys.playtree_iter->step(eof,0) != PLAY_TREE_ITER_ENTRY) {
		    delete MPXPSys.playtree_iter;
		    MPXPSys.playtree_iter = NULL;
		}
	    } else break;
	}

    if( MPXPSys.playtree_iter != NULL ){
	int flg;
	flg=INITED_ALL;
	if(input_state.after_dvdmenu) flg &=~(INITED_STREAM|INITED_DEMUXER);
	MPXPSys.uninit_player(flg&(~(INITED_INPUT|INITED_VO|INITED_SPUDEC))); /* TODO: |(~INITED_AO)|(~INITED_VO) */
	eof = 0;
	goto play_next_file;
    }

    if(stream_dump_type>1) dump_mux_close(MPXPSys.demuxer());

    delete ptr_protector.unprotect(secure_keys);
    return EXIT_SUCCESS;
}
} // namespace	usr

int main(int argc,char* args[], char *envp[])
{
    int rc;
    try {
	/* init malloc */
	size_t pos;
	mp_conf.malloc_debug=0;
	mp_malloc_e flg=MPA_FLG_RANDOMIZER;
	for(int i=0;i<argc;i++) {
	    std::string s=args[i];
	    if(s.substr(0,18)=="-core.malloc-debug") {
		if((pos=s.find('='))!=std::string::npos) {
		    mp_conf.malloc_debug=::atoi(s.substr(pos+1).c_str());
		}
		switch(mp_conf.malloc_debug) {
		    default:
		    case 0: flg=MPA_FLG_RANDOMIZER; break;
		    case 1: flg=MPA_FLG_BOUNDS_CHECK; break;
		    case 2: flg=MPA_FLG_BEFORE_CHECK; break;
		    case 3: flg=MPA_FLG_BACKTRACE; break;
		}
		break;
	    }
	}
	mp_init_malloc(args[0],1000,10,flg);
	std::vector<std::string> argv;
	std::string str,stmp;
	for(int i=0;i<argc;i++) {
	    str=args[i];
	    argv.push_back(str);
	}
	args[argc] = (char*)make_false_pointer((any_t*)antiviral_hole1);
	std::map<std::string,std::string> envm;
	unsigned j=0;
	while(envp[j]) {
	    str=envp[j++];
	    pos=str.find('=');
	    if(pos==std::string::npos) throw "Broken environment variable: "+str;
	    stmp=str.substr(pos+1);
	    str=str.substr(0,pos);
	    envm[str]=stmp;
	}
	envp[j+1] = NULL;
	/* init antiviral protection */
	rc=mp_mprotect((any_t*)antiviral_hole1,sizeof(antiviral_hole1),MP_DENY_ALL);
	rc|=mp_mprotect((any_t*)antiviral_hole2,sizeof(antiviral_hole2),MP_DENY_ALL);
	rc|=mp_mprotect((any_t*)antiviral_hole3,sizeof(antiviral_hole3),MP_DENY_ALL);
	rc|=mp_mprotect((any_t*)antiviral_hole4,sizeof(antiviral_hole4),MP_DENY_ALL);
	if(rc) {
		mpxp_err<<"*** Error! Cannot initialize antiviral protection: '"<<strerror(errno)<<"' ***!"<<std::endl;
		return EXIT_FAILURE;
	}
	mpxp_ok<<"*** Antiviral protection was inited ***!!!"<<std::endl;
	/* init structs */
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
	memset(&mp_conf.x86,-1,sizeof(x86_features_t));
#endif
	/* call player */
	rc=MPlayerXP(argv,envm);
	mpxp_ok<<"[main_module] finished"<<std::endl;
    } catch(const soft_exit_exception& e) {
	mpxp_hint<<"[main_module] "<<MSGTR_Exiting<<"...("<<e.what()<<")"<<std::endl;
	rc=EXIT_SUCCESS;
    } catch(const std::exception& e) {
	std::cout<<"[main_module] Exception '"<<e.what()<<"'caught in module: MPlayerXP"<<std::endl;
	rc=EXIT_FAILURE;
    } catch(const std::string& e) {
	std::cerr<<"[main_module] thrown signal '"<<e<<"' caught in module: MPlayerXP"<<std::endl;
	rc=EXIT_FAILURE;
    } catch(...) {
	std::cerr<<"[main_module] unknown exception caught in module: MPlayerXP"<<std::endl;
	rc=EXIT_FAILURE;
    }
    mpxp_context().engine().MPXPSys->uninit_player(INITED_ALL);
    MP_UNIT("exit_player");
    if(mpxp_context().mconfig) delete mpxp_context().mconfig;
    mpxp_print_uninit();
    mpxp_uninit_structs();
    fflush(stdout);
    fflush(stderr);

    return rc;
}
