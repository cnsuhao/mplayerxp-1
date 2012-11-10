/* MplayerXP (C) 2000-2002. by A'rpi/ESP-team (C) 2002. by Nickols_K */

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
#include "mp_config.h"
#include "xmpcore/sig_hand.h"
#include "mplayerxp.h"
#include "osdep/mplib.h"
#include "postproc/swscale.h"
#include "postproc/af.h"
#include "postproc/vf.h"
#define HELP_MP_DEFINE_STATIC
#include "help_mp.h"

#include "libmpdemux/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libmpdemux/parse_es.h"

#include "libmpconf/cfgparser.h"
#include "libmpconf/codec-cfg.h"
#include "libmpconf/m_struct.h"

#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/dec_audio.h"

/* Common FIFO functions, and keyboard/event FIFO code */
#include "fifo.h"

#ifdef USE_SUB
#include "libmpsub/subreader.h"
#endif
#include "libmpsub/spudec.h"
#include "libmpsub/vobsub.h"

#include "libvo/video_out.h"

#include "libvo/sub.h"
#include "libao2/audio_out.h"
#include "libao2/afmt.h"

#include "osdep/getch2.h"
#include "osdep/keycodes.h"
#include "osdep/timer.h"
#include "osdep/shmem.h"
#include "osdep/get_path.h"
#include "osdep/cpudetect.h"
#include "osdep/mm_accel.h"

#include "input2/input.h"
#include "dump.h"
#include "nls/nls.h"
#include "postproc/libmenu/menu.h"
#include "mixer.h"

#include "xmpcore/xmp_core.h"
#include "xmpcore/xmp_vplayer.h"
#include "xmpcore/xmp_adecoder.h"

#define MSGT_CLASS MSGT_CPLAYER
#include "mp_msg.h"

/**************************************************************************
             Private data
**************************************************************************/
static volatile char antiviral_hole1[__VM_PAGE_SIZE__] __PAGE_ALIGNED__;
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
typedef struct x86_features_s {
    int simd;
    int mmx;
    int mmx2;
    int _3dnow;
    int _3dnow2;
    int sse;
    int sse2;
    int sse3;
    int ssse3;
    int sse41;
    int sse42;
    int aes;
    int avx;
    int fma;
}x86_features_t;
static x86_features_t x86;
#endif

#define ABS(x) (((x)>=0)?(x):(-(x)))

#define INITED_VO	0x00000001
#define INITED_AO	0x00000002
#define INITED_GETCH2	0x00000004
#define INITED_LIRC	0x00000008
#define INITED_SPUDEC	0x00000010
#define INITED_STREAM	0x00000020
#define INITED_INPUT	0x00000040
#define INITED_DEMUXER  0x00000080
#define INITED_ACODEC	0x00000100
#define INITED_VCODEC	0x00000200
#define INITED_VOBSUB	0x00000400
#define INITED_SUBTITLE 0x10000000
#define INITED_XMP	0x80000000
#define INITED_ALL	0xFFFFFFFF

#define PT_NEXT_ENTRY 1
#define PT_PREV_ENTRY -1
#define PT_NEXT_SRC 2
#define PT_PREV_SRC -2
#define PT_UP_NEXT 3
#define PT_UP_PREV -3

typedef struct priv_s {
    unsigned	inited_flags;
    int		vo_inited;
    int		ao_inited;
    int		osd_show_framedrop;
    int		osd_function;
// priv data
    demuxer_t*	demuxer;
    play_tree_t*playtree;
    any_t*	libinput;
}priv_t;

mp_conf_t mp_conf;
static volatile char antiviral_hole2[__VM_PAGE_SIZE__] __PAGE_ALIGNED__;
mp_data_t* mp_data=NULL;
xp_core_t* xp_core=NULL;

/**************************************************************************
             Decoding ahead
**************************************************************************/
static volatile char antiviral_hole3[__VM_PAGE_SIZE__] __PAGE_ALIGNED__;
ao_data_t* ao_data=NULL;
vo_data_t* vo_data=NULL;

/**************************************************************************
             Config file
**************************************************************************/
static int cfg_inc_verbose(struct config *conf){ UNUSED(conf); ++mp_conf.verbose; return 0;}

static MPXP_Rc cfg_include(struct config *conf, char *filename){
    UNUSED(conf);
    return m_config_parse_config_file(mp_data->mconfig, filename);
}
#include "cfg-mplayerxp.h"

/**************************************************************************/
extern void mp_register_options(m_config_t* cfg);

static int mpxp_init_antiviral_protection(int verbose)
{
    int rc;
    rc=mp_mprotect(antiviral_hole1,sizeof(antiviral_hole1),MP_DENY_ALL);
    rc|=mp_mprotect(antiviral_hole2,sizeof(antiviral_hole2),MP_DENY_ALL);
    rc|=mp_mprotect(antiviral_hole3,sizeof(antiviral_hole3),MP_DENY_ALL);
    if(verbose) {
	if(rc)
	    MSG_ERR("*** Error! Cannot initialize antiviral protection: '%s' ***!\n",strerror(errno));
	else
	    MSG_OK("*** Antiviral protection was inited ***!!!\n");
    }
    return rc;
}

static MPXP_Rc mpxp_test_antiviral_protection(int* verbose)
{
    if(*verbose) MSG_INFO("Your've specified test-av option!\nRight now MPlayerXP should make coredump!\n");
    *verbose=antiviral_hole1[0]|antiviral_hole2[0]|antiviral_hole3[0];
    MSG_ERR("Antiviral protection of MPlayerXP doesn't work!");
    return MPXP_Virus;
}

unsigned xp_num_cpu;
static unsigned get_number_cpu(void) {
#ifdef _OPENMP
    return omp_get_num_procs();
#else
    /* TODO ? */
    return 1;
#endif
}

static void mpxp_init_structs(void) {
    mp_data=mp_mallocz(sizeof(mp_data_t));
    mp_data->seek_time=-1;
    mp_data->bench=mp_mallocz(sizeof(time_usage_t));
    mp_data->use_pts_fix2=-1;
    mp_data->rtc_fd=-1;
    mp_data->priv=mp_mallocz(sizeof(priv_t));
    priv_t*priv=mp_data->priv;
    priv->osd_function=OSD_PLAY;
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
    memset(&x86,-1,sizeof(x86_features_t));
#endif
    memset(&mp_conf,0,sizeof(mp_conf_t));
    mp_conf.xp=get_number_cpu();
    mp_conf.audio_id=-1;
    mp_conf.video_id=-1;
    mp_conf.dvdsub_id=-1;
    mp_conf.vobsub_id=-1;
    mp_conf.audio_lang=I18N_LANGUAGE;
    mp_conf.dvdsub_lang=I18N_LANGUAGE;
    mp_conf.av_sync_pts=-1;
    mp_conf.frame_reorder=1;
    mp_conf.av_force_pts_fix2=-1;
    mp_conf.loop_times=-1;
    mp_conf.play_n_frames=-1;
    mp_conf.font_factor=0.75;
    mp_conf.sub_auto=1;
    mp_conf.has_audio=1;
    mp_conf.has_video=1;
    mp_conf.has_dvdsub=1;
    mp_conf.osd_level=2;
    mp_conf.playbackspeed_factor=1.0;
    mp_conf.ao_channels=2;
    mp_conf.monitor_pixel_aspect=1;
    mp_conf.msg_filter=0xFFFFFFFF;
}

static void mpxp_uninit_structs(void) {
    free_codec_cfg();
    mp_free(mp_data->bench);
    mp_free(mp_data->priv);
    mp_free(mp_data);
    if(vo_data) mp_free(vo_data);
    if(ao_data) mp_free(ao_data);
    mp_data=NULL;
    xmp_uninit();
    mp_uninit_malloc(mp_conf.verbose);
}

void uninit_player(unsigned int mask){
    priv_t*priv=mp_data->priv;
    stream_t* stream=NULL;
    sh_audio_t* sh_audio=NULL;
    sh_video_t* sh_video=NULL;
    if(priv->demuxer) {
	stream=priv->demuxer->stream;
	sh_audio=priv->demuxer->audio->sh;
	sh_video=priv->demuxer->video->sh;
    }
    fflush(stdout);
    fflush(stderr);
    mask=priv->inited_flags&mask;

    MP_UNIT("uninit_xp");
    if(mask&INITED_XMP) {
	priv->inited_flags&=~INITED_XMP;
	MP_UNIT("uninit_xmp");
	xmp_uninit_engine(0);
    }

    if (mask&INITED_SPUDEC){
	priv->inited_flags&=~INITED_SPUDEC;
	MP_UNIT("uninit_spudec");
	spudec_free(vo_data->spudec);
	vo_data->spudec=NULL;
    }

    if (mask&INITED_VOBSUB){
	priv->inited_flags&=~INITED_VOBSUB;
	MP_UNIT("uninit_vobsub");
	vobsub_close(vo_data->vobsub);
	vo_data->vobsub=NULL;
    }

    if(mask&INITED_VCODEC){
	priv->inited_flags&=~INITED_VCODEC;
	MP_UNIT("uninit_vcodec");
	mpcv_uninit(sh_video);
	sh_video=NULL;
    }

    if(mask&INITED_VO){
	priv->inited_flags&=~INITED_VO;
	MP_UNIT("uninit_vo");
	vo_uninit(vo_data);
	vo_data=NULL;
    }

    if(mask&INITED_ACODEC){
	priv->inited_flags&=~INITED_ACODEC;
	MP_UNIT("uninit_acodec");
	mpca_uninit(sh_audio);
	sh_audio=NULL;
    }

    if(mask&INITED_AO){
	priv->inited_flags&=~INITED_AO;
	MP_UNIT("uninit_ao");
	ao_uninit(ao_data);
	ao_data=NULL;
    }

    if(mask&INITED_GETCH2){
	priv->inited_flags&=~INITED_GETCH2;
	MP_UNIT("uninit_getch2");
    // restore terminal:
	getch2_disable();
    }

    if(mask&INITED_DEMUXER){
	priv->inited_flags&=~INITED_DEMUXER;
	MP_UNIT("free_priv->demuxer");
	free_demuxer(priv->demuxer);
    }

    if(mask&INITED_STREAM){
	priv->inited_flags&=~INITED_STREAM;
	MP_UNIT("uninit_stream");
	if(stream) free_stream(stream);
	stream=NULL;
    }

    if(mask&INITED_INPUT){
	priv->inited_flags&=~INITED_INPUT;
	MP_UNIT("uninit_input");
	mp_input_close(priv->libinput);
    }

#ifdef USE_SUB
    if(mask&INITED_SUBTITLE){
	priv->inited_flags&=~INITED_SUBTITLE;
	MP_UNIT("sub_free");
	mp_input_close(priv->libinput);
	sub_free( mp_data->subtitles );
	mp_conf.sub_name=NULL;
	vo_data->sub=NULL;
	mp_data->subtitles=NULL;
    }
#endif
    MP_UNIT(NULL);
}

void exit_player(char* how){

    fflush(stdout);
    fflush(stderr);
    uninit_player(INITED_ALL);

    MP_UNIT("exit_player");

    sws_uninit();
    if(how) MSG_HINT(MSGTR_Exiting,how);
    MSG_DBG2("max framesize was %d bytes\n",max_framesize);
    if(mp_data->mconfig) m_config_free(mp_data->mconfig);
    mp_msg_uninit();
    mpxp_uninit_structs();
    if(how) exit(0);
    return; /* Still try coredump!!!*/
}

static void soft_exit_player(void)
{
    priv_t*priv=mp_data->priv;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    fflush(stdout);
    fflush(stderr);
    uninit_player(INITED_DEMUXER|INITED_STREAM);
    if(sh_audio) while(get_len_audio_buffer()) usleep(0);
    if(sh_video) {
	for(;;) {
	    if(dae_played_eof(xp_core->video)) break;
	    usleep(0);
	}
    }
    uninit_player((INITED_ALL)&(~(INITED_STREAM|INITED_DEMUXER)));

    MP_UNIT("exit_player");
    MSG_HINT(MSGTR_Exiting,MSGTR_Exit_quit);
    MSG_DBG2("max framesize was %d bytes\n",max_framesize);
    if(mp_data->mconfig) m_config_free(mp_data->mconfig);
    mp_msg_uninit();
    mpxp_uninit_structs();
    exit(0);
}

void __exit_sighandler(void)
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
  exit_player(NULL);
}


void exit_sighandler(void)
{
  xmp_killall_threads(pthread_self());
  __exit_sighandler();
}

static char* default_config=
"# Write your default config options here!\n"
"\n"
//"nosound=nein"
"\n";

void parse_cfgfiles( m_config_t* conf )
{
    char *conffile;
    int conffile_fd;
    if ((conffile = get_path("")) == NULL) {
	MSG_WARN(MSGTR_NoHomeDir);
    } else {
	mkdir(conffile, 0777);
	mp_free(conffile);
	if ((conffile = get_path("config")) == NULL) {
	    MSG_ERR(MSGTR_GetpathProblem);
	    conffile=(char*)mp_malloc(strlen("config")+1);
	    if(conffile)
		strcpy(conffile,"config");
	}
	if ((conffile_fd = open(conffile, O_CREAT | O_EXCL | O_WRONLY, 0666)) != -1) {
	    MSG_INFO(MSGTR_CreatingCfgFile, conffile);
	    write(conffile_fd, default_config, strlen(default_config));
	    close(conffile_fd);
	}
	if (m_config_parse_config_file(conf, conffile) != MPXP_Ok) exit(1);
	mp_free(conffile);
    }
}

// When libmpdemux perform a blocking operation (network connection or cache filling)
// if the operation fail we use this function to check if it was interrupted by the user.
// The function return a new value for eof.
static int libmpdemux_was_interrupted(int eof)
{
    priv_t*priv=mp_data->priv;
    mp_cmd_t* cmd;
    if((cmd = mp_input_get_cmd(priv->libinput,0,0,0)) != NULL) {
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

  if(x86.simd) {
    if(x86.mmx != -1) gCpuCaps.hasMMX=x86.mmx;
    if(x86.mmx2 != -1) gCpuCaps.hasMMX2=x86.mmx2;
    if(x86._3dnow != -1) gCpuCaps.has3DNow=x86._3dnow;
    if(x86._3dnow2 != -1) gCpuCaps.has3DNowExt=x86._3dnow2;
    if(x86.sse != -1) gCpuCaps.hasSSE=x86.sse;
    if(x86.sse2 != -1) gCpuCaps.hasSSE2=x86.sse2;
    if(x86.sse3 != -1) gCpuCaps.hasSSE2=x86.sse3;
    if(x86.ssse3 != -1) gCpuCaps.hasSSSE3=x86.ssse3;
    if(x86.sse41 != -1) gCpuCaps.hasSSE41=x86.sse41;
    if(x86.sse42 != -1) gCpuCaps.hasSSE42=x86.sse42;
    if(x86.aes != -1) gCpuCaps.hasAES=x86.aes;
    if(x86.avx != -1) gCpuCaps.hasAVX=x86.avx;
    if(x86.fma != -1) gCpuCaps.hasFMA=x86.fma;
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
  MSG_V("User corrected CPU flags: MMX=%d MMX2=%d 3DNow=%d 3DNow2=%d SSE=%d SSE2=%d SSE3=%d SSSE3=%d SSE41=%d SSE42=%d AES=%d AVX=%d FMA=%d\n",
	gCpuCaps.hasMMX,
	gCpuCaps.hasMMX2,
	gCpuCaps.has3DNow,
	gCpuCaps.has3DNowExt,
	gCpuCaps.hasSSE,
	gCpuCaps.hasSSE2,
	gCpuCaps.hasSSE3,
	gCpuCaps.hasSSSE3,
	gCpuCaps.hasSSE41,
	gCpuCaps.hasSSE42,
	gCpuCaps.hasAES,
	gCpuCaps.hasAVX,
	gCpuCaps.hasFMA);
  if(gCpuCaps.hasMMX) 		mp_data->mplayer_accel |= MM_ACCEL_X86_MMX;
  if(gCpuCaps.hasMMX2) 		mp_data->mplayer_accel |= MM_ACCEL_X86_MMXEXT;
  if(gCpuCaps.hasSSE) 		mp_data->mplayer_accel |= MM_ACCEL_X86_SSE;
  if(gCpuCaps.has3DNow) 	mp_data->mplayer_accel |= MM_ACCEL_X86_3DNOW;
  if(gCpuCaps.has3DNowExt) 	mp_data->mplayer_accel |= MM_ACCEL_X86_3DNOWEXT;
  MSG_V("mp_data->mplayer_accel=%i\n",mp_data->mplayer_accel);
}
#endif


static void init_player( void )
{
    if(mp_conf.video_driver && strcmp(mp_conf.video_driver,"help")==0)
    {
	vo_print_help(vo_data);
	mpxp_uninit_structs();
	exit(0);
    }
    if(mp_conf.audio_driver && strcmp(mp_conf.audio_driver,"help")==0)
    {
	ao_print_help();
	mpxp_uninit_structs();
	exit(0);
    }
    if(mp_conf.video_family && strcmp(mp_conf.video_family,"help")==0){
	vfm_help();
	mpxp_uninit_structs();
	exit(0);
    }
    if(mp_conf.audio_family && strcmp(mp_conf.audio_family,"help")==0){
	afm_help();
	mpxp_uninit_structs();
	exit(0);
    }
    if(vf_cfg.list && strcmp(vf_cfg.list,"help")==0){
	vf_help();
	mpxp_uninit_structs();
	exit(0);
    }
    if(af_cfg.list && strcmp(af_cfg.list,"help")==0){
	af_help();
	mpxp_uninit_structs();
	exit(0);
    }

    /* check codec.conf*/
    if(!parse_codec_cfg(get_path("codecs.conf"))){
      if(!parse_codec_cfg(CONFDIR"/codecs.conf")){
	MSG_HINT(MSGTR_CopyCodecsConf);
	mpxp_uninit_structs();
	exit(0);
      }
    }

    if(mp_conf.audio_codec && strcmp(mp_conf.audio_codec,"help")==0){
	list_codecs(1);
	mpxp_uninit_structs();
	exit(0);
    }
    if(mp_conf.video_codec && strcmp(mp_conf.video_codec,"help")==0){
	list_codecs(0);
	mpxp_uninit_structs();
	exit(0);
    }
}

void show_help(void) {
    // no file/vcd/dvd -> show HELP:
    MSG_INFO("%s",help_text);
    print_stream_drivers();
    MSG_INFO("\nUse --long-help option for full help\n");
}

void show_long_help(void) {
    priv_t*priv=mp_data->priv;
    m_config_show_options(mp_data->mconfig);
    mp_input_print_binds(priv->libinput);
    print_stream_drivers();
    vo_print_help(vo_data);
    ao_print_help();
    vf_help();
    af_help();
    vfm_help();
    afm_help();
    /* check codec.conf*/
    if(!parse_codec_cfg(get_path("codecs.conf"))){
      if(!parse_codec_cfg(CONFDIR"/codecs.conf")){
	MSG_HINT(MSGTR_CopyCodecsConf);
	mpxp_uninit_structs();
	exit(0);
      }
    }
    list_codecs(0);
    list_codecs(1);
}

#ifdef USE_OSD

//================= Update OSD ====================
void update_osd( float v_pts )
{
    priv_t*priv=mp_data->priv;
    static char osd_text_buffer[64];
    static int osd_last_pts=-303;
//================= Update OSD ====================
  if(mp_conf.osd_level>=2){
      int pts=(mp_conf.osd_level==3&&priv->demuxer->movi_length!=UINT_MAX)?priv->demuxer->movi_length-v_pts:v_pts;
      int addon=(mp_conf.osd_level==3&&priv->demuxer->movi_length!=UINT_MAX)?-1:1;
      char osd_text_tmp[64];
      if(pts==osd_last_pts-addon)
      {
        if(mp_conf.osd_level==3&&priv->demuxer->movi_length!=UINT_MAX) ++pts;
	else --pts;
      }
      else osd_last_pts=pts;
      vo_data->osd_text=osd_text_buffer;
      if (priv->osd_show_framedrop) {
	  sprintf(osd_text_tmp, "Framedrop: %s",mp_conf.frame_dropping>1?"hard":mp_conf.frame_dropping?"vo":"none");
	  priv->osd_show_framedrop--;
      } else
#ifdef ENABLE_DEC_AHEAD_DEBUG
	  if(mp_conf.verbose) sprintf(osd_text_tmp,"%c %02d:%02d:%02d",priv->osd_function,pts/3600,(pts/60)%60,pts%60);
	  else sprintf(osd_text_tmp,"%c %02d:%02d:%02d",priv->osd_function,pts/3600,(pts/60)%60,pts%60);
#else
          sprintf(osd_text_tmp,"%c %02d:%02d:%02d",priv->osd_function,pts/3600,(pts/60)%60,pts%60);
#endif
      if(strcmp(vo_data->osd_text, osd_text_tmp)) {
	      strcpy(vo_data->osd_text, osd_text_tmp);
	      vo_osd_changed(OSDTYPE_OSD);
      }
  } else {
      if(vo_data->osd_text) {
      vo_data->osd_text=NULL;
	  vo_osd_changed(OSDTYPE_OSD);
      }
  }
}
#endif
typedef struct osd_args_s {
    int		visible;
    int		info_factor;
}osd_args_t;

void mpxp_seek( osd_args_t *osd,const seek_args_t* seek)
{
    priv_t*priv=mp_data->priv;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    demux_stream_t *d_dvdsub=priv->demuxer->sub;
    int seek_rval=1;
    xp_core->audio->eof=0;
    if(seek->secs || seek->flags&DEMUX_SEEK_SET) {
	seek_rval=demux_seek_r(priv->demuxer,seek);
	mp_data->mpxp_after_seek=25; /* 1 sec delay */
    }
    if(seek_rval){
	mp_data->seek_time = GetTimerMS();

	// success:
	/* FIXME there should be real seeking for vobsub */
	if (vo_data->vobsub) vobsub_reset(vo_data->vobsub);
	if (vo_data->spudec) spudec_reset(vo_data->spudec);

	if(sh_audio){
	    sh_audio->chapter_change=0;
	    sh_audio->a_pts=HUGE;
	}
	fflush(stdout);

	if(sh_video){
	    MP_UNIT("seek_video_reset");
	    mpcv_resync_stream(sh_video);
	    vo_reset(vo_data);
	    sh_video->chapter_change=-1;
	}

	if(sh_audio){
	    MP_UNIT("seek_audio_reset");
	    mpca_resync_stream(sh_audio);
	    ao_reset(ao_data); // stop audio, throwing away buffered data
	}

	if (vo_data->vobsub) {
	    MP_UNIT("seek_vobsub_reset");
	    vobsub_seek_r(vo_data->vobsub, seek);
	}

#ifdef USE_OSD
	// Set OSD:
	if(mp_conf.osd_level){
	    int len=((priv->demuxer->movi_end-priv->demuxer->movi_start)>>8);
	    if (len>0){
		if(osd) osd->visible=sh_video->fps<=60?sh_video->fps:25;
		vo_data->osd_progbar_type=0;
		vo_data->osd_progbar_value=(priv->demuxer->filepos-priv->demuxer->movi_start)/len;
		vo_osd_changed(OSDTYPE_PROGBAR);
	    }
	}
#endif
	if(sh_video) {
	    max_pts_correction=0.1;
	    if(osd) osd->visible=sh_video->fps<=60?sh_video->fps:25; // to rewert to PLAY pointer after 1 sec
	    mp_data->bench->audio=0; mp_data->bench->audio_decode=0; mp_data->bench->video=0; mp_data->bench->vout=0;
	    if(vo_data->spudec) {
		unsigned char* packet=NULL;
		while(ds_get_packet_sub_r(d_dvdsub,&packet)>0) ; // Empty stream
		spudec_reset(vo_data->spudec);
	    }
	}
    }

    if(sh_video) dae_wait_decoder_outrun(xp_core->video);
}

void mpxp_reset_vcache(void)
{
    priv_t*priv=mp_data->priv;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    seek_args_t seek = { 0, DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS };
    if(sh_video) mpxp_seek(NULL,&seek);
    return;
}

void mpxp_resync_audio_stream(void)
{
    priv_t*priv=mp_data->priv;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    mpca_resync_stream(sh_audio);
}

static void __FASTCALL__ mpxp_stream_event_handler(struct stream_s *s,const stream_packet_t *sp)
{
    s->driver->control(s,SCRTL_EVT_HANDLE,(any_t*)sp);
}

static void init_benchmark(void)
{
    mp_data->bench->max_audio=0; mp_data->bench->max_video=0; mp_data->bench->max_vout=0;
    mp_data->bench->min_audio=HUGE; mp_data->bench->min_video=HUGE; mp_data->bench->min_vout=HUGE;

    mp_data->bench->min_audio_decode=HUGE;
    mp_data->bench->max_audio_decode=0;

    mp_data->bench->max_demux=0;
    mp_data->bench->demux=0;
    mp_data->bench->min_demux=HUGE;

    mp_data->bench->cur_video=0;
    mp_data->bench->cur_vout=0;
    mp_data->bench->cur_audio=0;
}

static void show_benchmark(void)
{
    double tot=(mp_data->bench->video+mp_data->bench->vout+mp_data->bench->audio+mp_data->bench->audio_decode+mp_data->bench->demux+mp_data->bench->c2);
    double total_time_usage;

    mp_data->bench->total_start=GetTimer()-mp_data->bench->total_start;
    total_time_usage = (float)mp_data->bench->total_start*0.000001;

    MSG_INFO("\nAVE BENCHMARKs: VC:%8.3fs VO:%8.3fs A:%8.3fs D:%8.3fs = %8.4fs C:%8.3fs\n",
	 mp_data->bench->video,mp_data->bench->vout,mp_data->bench->audio+mp_data->bench->audio_decode,
	 mp_data->bench->demux,mp_data->bench->c2,tot);
    if(total_time_usage>0.0)
	MSG_INFO("AVE BENCHMARK%%: VC:%8.4f%% VO:%8.4f%% A:%8.4f%% D:%8.4f%% C:%8.4f%% = %8.4f%%\n",
	   100.0*mp_data->bench->video/total_time_usage,
	   100.0*mp_data->bench->vout/total_time_usage,
	   100.0*(mp_data->bench->audio+mp_data->bench->audio_decode)/total_time_usage,
	   100.0*mp_data->bench->demux/total_time_usage,
	   100.0*mp_data->bench->c2/total_time_usage,
	   100.0*tot/total_time_usage);
    unsigned nframes=xp_core->video->num_played_frames;
    MSG_INFO("\nREAL RESULTS: from %u was dropped=%u\n"
	,nframes,xp_core->video->num_dropped_frames);
}

static void show_benchmark_status(void)
{
    priv_t*priv=mp_data->priv;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    if(xmp_test_model(XMP_Run_AudioPlayback))
		MSG_STATUS("A:%6.1f %4.1f%%\r"
			,sh_audio->timer-ao_get_delay(ao_data)
			,(sh_audio->timer>0.5)?100.0*(mp_data->bench->audio+mp_data->bench->audio_decode)/(double)sh_audio->timer:0
			);
    else
	MSG_STATUS("A:%6.1f %4.1f%%  B:%4.1f\r"
		,sh_audio->timer-ao_get_delay(ao_data)
		,(sh_audio->timer>0.5)?100.0*(mp_data->bench->audio+mp_data->bench->audio_decode)/(double)sh_audio->timer:0
		,get_delay_audio_buffer()
		);
}

// for multifile support:
play_tree_iter_t* playtree_iter = NULL;

static void mpxp_init_keyboard_fifo(void)
{
    priv_t*priv=mp_data->priv;
#ifdef HAVE_TERMCAP
    load_termcap(NULL); // load key-codes
#endif
    fifo_make_pipe(&keyb_fifo_get,&keyb_fifo_put);
    /* Init input system */
    MP_UNIT("init_input");
    priv->libinput=mp_input_open();
    if(keyb_fifo_get > 0)
	mp_input_add_key_fd(priv->libinput,keyb_fifo_get,1,NULL,NULL);
    if(mp_conf.slave_mode)
	mp_input_add_cmd_fd(priv->libinput,0,1,NULL,NULL);
    else if(!mp_conf.use_stdin)
	mp_input_add_key_fd(priv->libinput,0,1,NULL,NULL);
    priv->inited_flags|=INITED_INPUT;
}

static void mpxp_init_osd(void) {
// check font
#ifdef USE_OSD
    if(mp_conf.font_name){
	vo_data->font=read_font_desc(mp_conf.font_name,mp_conf.font_factor,mp_conf.verbose>1);
	if(!vo_data->font) MSG_ERR(MSGTR_CantLoadFont,mp_conf.font_name);
    } else {
	// try default:
	vo_data->font=read_font_desc(get_path("font/font.desc"),mp_conf.font_factor,mp_conf.verbose>1);
	if(!vo_data->font)
	    vo_data->font=read_font_desc(DATADIR"/font/font.desc",mp_conf.font_factor,mp_conf.verbose>1);
    }
#endif
    /* Configure menu here */
    {
	char *menu_cfg;
	menu_cfg = get_path("menu.conf");
	if(menu_init(NULL, menu_cfg))
	    MSG_INFO("Menu initialized: %s\n", menu_cfg);
	else {
	    menu_cfg="/etc/menu.conf";
	    if(menu_init(NULL, menu_cfg))
		MSG_INFO("Menu initialized: %s\n", menu_cfg);
	    else
		MSG_WARN("Menu init failed\n");
	}
    }
    MP_UNIT("init_osd");
    vo_init_osd();
}

static char * mpxp_init_output_subsystems(void) {
    priv_t*priv=mp_data->priv;
    char* rs=NULL;
    unsigned i;
    // check video_out driver name:
    if (mp_conf.video_driver)
	if ((i=strcspn(mp_conf.video_driver, ":")) > 0) {
	    size_t i2 = strlen(mp_conf.video_driver);
	    if (mp_conf.video_driver[i] == ':') {
		vo_conf.subdevice = mp_malloc(i2-i);
		if (vo_conf.subdevice != NULL)
		    strncpy(vo_conf.subdevice, (char *)(mp_conf.video_driver+i+1), i2-i);
		mp_conf.video_driver[i] = '\0';
	    }
	}
    MP_UNIT("vo_register");
    priv->vo_inited = (vo_register(vo_data,mp_conf.video_driver)!=NULL)?1:0;

    if(!priv->vo_inited){
	MSG_FATAL(MSGTR_InvalidVOdriver,mp_conf.video_driver?mp_conf.video_driver:"?");
	exit_player(MSGTR_Exit_error);
    }
    MP_UNIT("vo_init");
    if(vo_init(vo_data,vo_conf.subdevice)!=MPXP_Ok) {
	MSG_FATAL("Error opening/initializing the selected video_out (-vo) device!\n");
	exit_player(MSGTR_Exit_error);
    }

// check audio_out driver name:
    MP_UNIT("ao_init");
    if (mp_conf.audio_driver)
	if ((i=strcspn(mp_conf.audio_driver, ":")) > 0)
	{
	    size_t i2 = strlen(mp_conf.audio_driver);

	    if (mp_conf.audio_driver[i] == ':')
	    {
		rs = mp_malloc(i2-i);
		if (rs != NULL)  strncpy(rs, (char *)(mp_conf.audio_driver+i+1), i2-i);
		mp_conf.audio_driver[i] = '\0';
	    }
	}
    priv->ao_inited=(ao_register(mp_conf.audio_driver)!=NULL)?1:0;
    if (!priv->ao_inited){
	MSG_FATAL(MSGTR_InvalidAOdriver,mp_conf.audio_driver);
	exit_player(MSGTR_Exit_error);
    }
    return rs;
}

static int mpxp_init_vobsub(const char *filename) {
    priv_t*priv=mp_data->priv;
    int forced_subs_only=0;
    MP_UNIT("vobsub");
    if (mp_conf.vobsub_name){
      vo_data->vobsub=vobsub_open(mp_conf.vobsub_name,mp_conf.spudec_ifo,1,&vo_data->spudec);
      if(vo_data->vobsub==NULL)
        MSG_ERR(MSGTR_CantLoadSub,mp_conf.vobsub_name);
      else {
	priv->inited_flags|=INITED_VOBSUB;
	vobsub_set_from_lang(vo_data->vobsub, mp_conf.dvdsub_lang);
	// check if vobsub requested only to display forced subtitles
	forced_subs_only=vobsub_get_forced_subs_flag(vo_data->vobsub);
      }
    }else if(mp_conf.sub_auto && filename && (strlen(filename)>=5)){
      /* try to autodetect vobsub from movie filename ::atmos */
      char *buf = mp_mallocz((strlen(filename)-3) * sizeof(char));
      strncpy(buf, filename, strlen(filename)-4);
      vo_data->vobsub=vobsub_open(buf,mp_conf.spudec_ifo,0,&vo_data->spudec);
      mp_free(buf);
    }
    if(vo_data->vobsub)
    {
      mp_conf.sub_auto=0; // don't do autosub for textsubs if vobsub found
      priv->inited_flags|=INITED_VOBSUB;
    }
    return forced_subs_only;
}

static int mpxp_handle_playlist(const char *filename) {
    priv_t*priv=mp_data->priv;
    stream_t* stream=priv->demuxer->stream;
    int eof=0;
    play_tree_t* entry;
    // Handle playlist
    MP_UNIT("handle_playlist");
    MSG_V("Parsing playlist %s...\n",filename);
    entry = parse_playtree(priv->libinput,stream);
    if(!entry) {
      entry = playtree_iter->tree;
      if(play_tree_iter_step(playtree_iter,1,0) != PLAY_TREE_ITER_ENTRY) {
	eof = PT_NEXT_ENTRY;
	return eof;
      }
      if(playtree_iter->tree == entry ) { // Loop with a single file
	if(play_tree_iter_up_step(playtree_iter,1,0) != PLAY_TREE_ITER_ENTRY) {
	  eof = PT_NEXT_ENTRY;
	  return eof;
	}
      }
      play_tree_remove(entry,1,1);
      eof = PT_NEXT_SRC;
      return eof;
    }
    play_tree_insert_entry(playtree_iter->tree,entry);
    entry = playtree_iter->tree;
    if(play_tree_iter_step(playtree_iter,1,0) != PLAY_TREE_ITER_ENTRY) {
      eof = PT_NEXT_ENTRY;
      return eof;
    }
    play_tree_remove(entry,1,1);
    eof = PT_NEXT_SRC;
    return eof;
}

static void mpxp_init_dvd_nls(void) {
/* Add NLS support here */
    priv_t*priv=mp_data->priv;
    stream_t* stream=priv->demuxer->stream;
    char *lang;
    if(!mp_conf.audio_lang) mp_conf.audio_lang=nls_get_screen_cp();
    MP_UNIT("dvd lang->id");
    if(mp_conf.audio_lang) {
	lang=mp_malloc(max(strlen(mp_conf.audio_lang)+1,4));
	strcpy(lang,mp_conf.audio_lang);
	if(mp_conf.audio_id==-1 && stream->driver->control(stream,SCTRL_LNG_GET_AID,lang) == SCTRL_OK) {
	    mp_conf.audio_id=*(int *)lang;
	}
	mp_free(lang);
    }
    if(mp_conf.dvdsub_lang) {
	lang=mp_malloc(max(strlen(mp_conf.dvdsub_lang)+1,4));
	strcpy(lang,mp_conf.dvdsub_lang);
	if(mp_conf.dvdsub_id==-1 && stream->driver->control(stream,SCTRL_LNG_GET_SID,lang) == SCTRL_OK) {
	    mp_conf.dvdsub_id=*(int *)lang;
	}
	mp_free(lang);
    }
}

static void mpxp_print_stream_formats(void) {
    priv_t*priv=mp_data->priv;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    int fmt;
    char *c;
    MSG_INFO("[Stream]:");
    if(sh_video) {
	MSG_INFO("Video=");
	if(sh_video->bih)fmt=sh_video->bih->biCompression;
	else		 fmt=sh_video->fourcc;
	c=(char *)&fmt;
	if(isprint(c[0]) && isprint(c[1]) && isprint(c[2]) && isprint(c[3]))
	    MSG_INFO("%.4s",c);
	else
	    MSG_INFO("%08X",fmt);
    }
    if(sh_audio) {
	MSG_INFO(" Audio=");
	fmt=sh_audio->wtag;
	c=(char *)&fmt;
	if(isprint(c[0]) && isprint(c[1]) && isprint(c[2]) && isprint(c[3]))
	    MSG_INFO("%.4s",c);
	else
	    MSG_INFO("%08X",fmt);
    }
    MSG_INFO("\n");
}

static void mpxp_read_video_properties(void) {
    priv_t*priv=mp_data->priv;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    demux_stream_t *d_video=priv->demuxer->video;
    MP_UNIT("video_read_properties");
    if(!video_read_properties(sh_video)) {
	MSG_ERR("Video: can't read properties\n");
	sh_video=d_video->sh=NULL;
    } else {
	MSG_V("[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.2f  ftime:=%6.4f\n",
	    priv->demuxer->file_format,sh_video->fourcc, sh_video->src_w,sh_video->src_h,
	    sh_video->fps,1/sh_video->fps
	    );
    /* need to set fps here for output encoders to pick it up in their init */
	if(mp_conf.force_fps){
	    sh_video->fps=mp_conf.force_fps;
	}

	if(!sh_video->fps && !mp_conf.force_fps){
	    MSG_ERR(MSGTR_FPSnotspecified);
	    sh_video=d_video->sh=NULL;
	}
    }
}

static void mpxp_read_subtitles(const char *filename,int forced_subs_only,int stream_dump_type) {
    priv_t*priv=mp_data->priv;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    stream_t* stream=priv->demuxer->stream;
    if (mp_conf.spudec_ifo) {
	unsigned int palette[16], width, height;
	MP_UNIT("spudec_init_vobsub");
	if (vobsub_parse_ifo(NULL,mp_conf.spudec_ifo, palette, &width, &height, 1, -1, NULL) >= 0)
	    vo_data->spudec=spudec_new_scaled(palette, sh_video->src_w, sh_video->src_h);
    }

    if (vo_data->spudec==NULL) {
	unsigned *pal;
	MP_UNIT("spudec_init");
	if(stream->driver->control(stream,SCTRL_VID_GET_PALETTE,&pal)==SCTRL_OK)
	    vo_data->spudec=spudec_new_scaled(pal,sh_video->src_w, sh_video->src_h);
    }

    if (vo_data->spudec==NULL) {
	MP_UNIT("spudec_init_normal");
	vo_data->spudec=spudec_new_scaled(NULL, sh_video->src_w, sh_video->src_h);
	spudec_set_font_factor(vo_data->spudec,mp_conf.font_factor);
    }

    if (vo_data->spudec!=NULL) {
	priv->inited_flags|=INITED_SPUDEC;
	// Apply current settings for forced subs
	spudec_set_forced_subs_only(vo_data->spudec,forced_subs_only);
    }

#ifdef USE_SUB
// after reading video params we should load subtitles because
// we know fps so now we can adjust subtitles time to ~6 seconds AST
// check .sub
    MP_UNIT("read_subtitles_file");
    if(mp_conf.sub_name){
	mp_data->subtitles=sub_read_file(mp_conf.sub_name, sh_video->fps);
	if(!mp_data->subtitles) MSG_ERR(MSGTR_CantLoadSub,mp_conf.sub_name);
    } else if(mp_conf.sub_auto) { // auto load sub file ...
	mp_data->subtitles=sub_read_file( filename ? sub_filename( get_path("sub/"), filename )
				      : "default.sub", sh_video->fps );
    }
    if(mp_data->subtitles) {
	priv->inited_flags|=INITED_SUBTITLE;
	if(stream_dump_type>1) list_sub_file(mp_data->subtitles);
    }
#endif
}

static void mpxp_find_acodec(void) {
    int found=0;
    priv_t*priv=mp_data->priv;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    demux_stream_t *d_audio=priv->demuxer->audio;
// Go through the codec.conf and find the best codec...
    sh_audio->codec=NULL;
    if(mp_conf.audio_family) MSG_INFO(MSGTR_TryForceAudioFmt,mp_conf.audio_family);
    while(1) {
	sh_audio->codec=find_codec(sh_audio->wtag,NULL,sh_audio->codec,1);
	if(!sh_audio->codec) {
	    if(mp_conf.audio_family) {
		sh_audio->codec=NULL; /* re-search */
		MSG_ERR(MSGTR_CantFindAfmtFallback);
		mp_conf.audio_family=NULL;
		continue;
	    }
	    break;
	}
	if(mp_conf.audio_codec && strcmp(sh_audio->codec->codec_name,mp_conf.audio_codec)) continue;
	else if(mp_conf.audio_family && strcmp(sh_audio->codec->driver_name,mp_conf.audio_family)) continue;
	if(mpca_find_driver(sh_audio->codec->driver_name)) {
	    MSG_V("%s audio codec: [%s] drv:%s (%s)\n",mp_conf.audio_codec?"Forcing":"Detected",sh_audio->codec->codec_name,sh_audio->codec->driver_name,sh_audio->codec->s_info);
	    found=1;
	    break;
	}
    }
    if(!found) {
	sh_audio->codec=find_ffmpeg_audio(sh_audio);
	if(sh_audio->codec) found=1;
    }
    if(!found) {
	const char *fmt;
	MSG_ERR(MSGTR_CantFindAudioCodec);
	fmt = (const char *)&sh_audio->wtag;
	if(isprint(fmt[0]) && isprint(fmt[1]) && isprint(fmt[2]) && isprint(fmt[3]))
	    MSG_ERR(" '%c%c%c%c'!\n",fmt[0],fmt[1],fmt[2],fmt[3]);
	else
	    MSG_ERR(" 0x%08X!\n",sh_audio->wtag);
	MSG_HINT( MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
	sh_audio=d_audio->sh=NULL;
    }
}

static int mpxp_find_vcodec(void) {
    priv_t*priv=mp_data->priv;
    demux_stream_t *d_video=priv->demuxer->video;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    int rc=0;
    MP_UNIT("init_video_codec");
    /* Go through the codec.conf and find the best codec...*/
    sh_video->inited=0;
    vo_data->flags=0;
    if(vo_conf.fullscreen)	vo_FS_SET(vo_data);
    if(vo_conf.softzoom)	vo_ZOOM_SET(vo_data);
    if(vo_conf.flip>0)		vo_FLIP_SET(vo_data);
    if(vo_conf.vidmode)		vo_VM_SET(vo_data);
    codecs_reset_selection(0);
    if(mp_conf.video_codec) {
    /* forced codec by name: */
	MSG_INFO("Forced video codec: %s\n",mp_conf.video_codec);
	mpcv_init(sh_video,mp_conf.video_codec,NULL,-1,priv->libinput);
    } else {
	int status;
    /* try in stability order: UNTESTED, WORKING, BUGGY, BROKEN */
	if(mp_conf.video_family) MSG_INFO(MSGTR_TryForceVideoFmt,mp_conf.video_family);
	for(status=CODECS_STATUS__MAX;status>=CODECS_STATUS__MIN;--status){
	    if(mp_conf.video_family) /* try first the preferred codec family:*/
		if(mpcv_init(sh_video,NULL,mp_conf.video_family,status,priv->libinput)==MPXP_Ok) break;
	    if(mpcv_init(sh_video,NULL,NULL,status,priv->libinput)==MPXP_Ok) break;
	}
    }
    /* Use ffmpeg decoders as last hope */
    if(!sh_video->inited) mpcv_ffmpeg_init(sh_video,priv->libinput);

    if(!sh_video->inited) {
	const char *fmt;
	MSG_ERR(MSGTR_CantFindVideoCodec);
	fmt = (const char *)&sh_video->fourcc;
	if(isprint(fmt[0]) && isprint(fmt[1]) && isprint(fmt[2]) && isprint(fmt[3]))
	    MSG_ERR(" '%c%c%c%c'!\n",fmt[0],fmt[1],fmt[2],fmt[3]);
	else
	    MSG_ERR(" 0x%08X!\n",sh_video->fourcc);
	MSG_HINT( MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
	sh_video = d_video->sh = NULL;
	rc=-1;
    } else  priv->inited_flags|=INITED_VCODEC;

    if(sh_video)
    MSG_V("%s video codec: [%s] vfm:%s (%s)\n",
	mp_conf.video_codec?"Forcing":"Detected",sh_video->codec->codec_name,sh_video->codec->driver_name,sh_video->codec->s_info);
    return rc;
}

static int mpxp_configure_audio(void) {
    priv_t*priv=mp_data->priv;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    demux_stream_t *d_audio=priv->demuxer->audio;
    int rc=0;
    const ao_info_t *info=ao_get_info();
    MP_UNIT("setup_audio");
    MSG_V("AO: [%s] %iHz %s %s\n",
	info->short_name,
	mp_conf.force_srate?mp_conf.force_srate:sh_audio->rate,
	sh_audio->nch>7?"surround71":
	sh_audio->nch>6?"surround61":
	sh_audio->nch>5?"surround51":
	sh_audio->nch>4?"surround41":
	sh_audio->nch>3?"surround40":
	sh_audio->nch>2?"stereo2.1":
	sh_audio->nch>1?"Stereo":"Mono",
	ao_format_name(sh_audio->afmt)
    );
    MSG_V("AO: Description: %s\nAO: Author: %s\n",
	info->name, info->author);
    if(strlen(info->comment) > 0)
	MSG_V("AO: Comment: %s\n", info->comment);

    MP_UNIT("af_preinit");
    ao_data->samplerate=mp_conf.force_srate?mp_conf.force_srate:sh_audio->rate;
    ao_data->channels=mp_conf.ao_channels?mp_conf.ao_channels:sh_audio->nch;
    ao_data->format=sh_audio->afmt;
#if 1
    if(mpca_preinit_filters(sh_audio,
	    // input:
	    (int)(sh_audio->rate),
	    sh_audio->nch, sh_audio->afmt,
	    // output:
	    &ao_data->samplerate, &ao_data->channels, &ao_data->format)!=MPXP_Ok){
	    MSG_ERR("Audio filter chain preinit failed\n");
    } else {
	MSG_V("AF_pre: %dHz %dch (%s) afmt=%08X sh_audio_min=%i\n",
		ao_data->samplerate, ao_data->channels,
		ao_format_name(ao_data->format),ao_data->format
		,sh_audio->audio_out_minsize);
    }
#endif
    if(MPXP_Ok!=ao_configure(ao_data,mp_conf.force_srate?mp_conf.force_srate:ao_data->samplerate,
		    ao_data->channels,ao_data->format)) {
	MSG_ERR("Can't configure audio device\n");
	sh_audio=d_audio->sh=NULL;
	if(sh_video == NULL) rc=-1;
    } else {
	priv->inited_flags|=INITED_AO;
	MP_UNIT("af_init");
	if(mpca_init_filters(sh_audio,
	    sh_audio->rate,
	    sh_audio->nch, sh_audio->afmt,
	    ao_data->samplerate, ao_data->channels, ao_data->format,
	    ao_data->outburst*4, ao_data->buffersize)!=MPXP_Ok) {
		MSG_ERR("No matching audio filter found!\n");
	    }
    }
    return rc;
}

static void mpxp_run_ahead_engine(void) {
    priv_t*priv=mp_data->priv;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    MP_UNIT("init_xp");
    if(sh_video && xp_core->num_v_buffs < 3) {/* we need at least 3 buffers to suppress screen judering */
	MSG_FATAL("Not enough buffers for DECODING AHEAD!\nNeed %u buffers but exist only %u\n",3,xp_core->num_v_buffs);
	exit_player("Try other '-vo' driver.\n");
    }
    if(xmp_init_engine(sh_video,sh_audio)!=0)
	exit_player("Can't initialize decoding ahead!\n");
    if(xmp_run_decoders()!=0)
	exit_player("Can't run decoding ahead!\n");
    if(sh_video)	MSG_OK("Using DECODING AHEAD mplayer's core with %u video buffers\n",xp_core->num_v_buffs);
    else 		MSG_OK("Using DECODING AHEAD mplayer's core with %u audio buffers\n",xp_core->num_a_buffs);
/* reset counters */
    if(sh_video) xp_core->video->num_dropped_frames=0;
    priv->inited_flags|=INITED_XMP;
}

static void mpxp_print_audio_status(void) {
    priv_t*priv=mp_data->priv;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    /* PAINT audio OSD */
    unsigned ipts,rpts;
    unsigned char h,m,s,rh,rm,rs;
    static char ph=0,pm=0,ps=0;
    ipts=(unsigned)(sh_audio->timer-ao_get_delay(ao_data));
    rpts=priv->demuxer->movi_length-ipts;
    h = ipts/3600;
    m = (ipts/60)%60;
    s = ipts%60;
    if(priv->demuxer->movi_length!=UINT_MAX) {
	rh = rpts/3600;
	rm = (rpts/60)%60;
	rs = rpts%60;
    } else rh=rm=rs=0;
    if(h != ph || m != pm || s != ps) {
	MSG_STATUS(">%02u:%02u:%02u (%02u:%02u:%02u)\r",h,m,s,rh,rm,rs);
	ph = h;
	pm = m;
	ps = s;
    }
}

#ifdef USE_OSD
static int mpxp_paint_osd(int* osd_visible,int* in_pause) {
    priv_t*priv=mp_data->priv;
    stream_t* stream=priv->demuxer->stream;
    sh_audio_t* sh_audio=priv->demuxer->audio->sh;
    sh_video_t* sh_video=priv->demuxer->video->sh;
    int rc=0;
    if(*osd_visible) {
	if (!--(*osd_visible)) {
	    vo_data->osd_progbar_type=-1; // disable
	    vo_osd_changed(OSDTYPE_PROGBAR);
	    if (!((priv->osd_function == OSD_PAUSE)||(priv->osd_function==OSD_DVDMENU)))
		priv->osd_function = OSD_PLAY;
	}
    }
    if(priv->osd_function==OSD_DVDMENU) {
	rect_highlight_t hl;
	if(stream->driver->control(stream,SCTRL_VID_GET_HILIGHT,&hl)==SCTRL_OK) {
	    osd_set_nav_box (hl.sx, hl.sy, hl.ex, hl.ey);
	    MSG_V("Set nav box: %i %i %i %i\n",hl.sx, hl.sy, hl.ex, hl.ey);
	    vo_osd_changed (OSDTYPE_DVDNAV);
	}
    }
    if(priv->osd_function==OSD_PAUSE||priv->osd_function==OSD_DVDMENU) {
	mp_cmd_t* cmd;
	if (priv->vo_inited && sh_video) {
	    if(mp_conf.osd_level>1 && !*in_pause) {
		*in_pause = 1;
		return -1;
	    }
	    vo_pause(vo_data);
	}
	if(mp_conf.verbose) {
	    MSG_STATUS("\n------ PAUSED -------\r");
	    fflush(stdout);
	}

	if (priv->ao_inited && sh_audio) {
	    if(xmp_test_model(XMP_Run_AudioPlayer)) {
		xp_core->in_pause=1;
		while( !dec_ahead_can_aseek ) usleep(0);
	    }
	    ao_pause(ao_data);	// pause audio, keep data if possible
	}

	while( (cmd = mp_input_get_cmd(priv->libinput,20,1,1)) == NULL) {
	    if(sh_video && priv->vo_inited) vo_check_events(vo_data);
	    usleep(20000);
	}

	if (cmd && cmd->id == MP_CMD_PAUSE) {
	    cmd = mp_input_get_cmd(priv->libinput,0,1,0);
	    mp_cmd_free(cmd);
	}

	if(priv->osd_function==OSD_PAUSE) priv->osd_function=OSD_PLAY;
	if (priv->ao_inited && sh_audio) {
	    ao_resume(ao_data);	// resume audio
	    if(xmp_test_model(XMP_Run_AudioPlayer)) {
		xp_core->in_pause=0;
		__MP_SYNCHRONIZE(audio_play_mutex,pthread_cond_signal(&audio_play_cond));
	    }
	}
	if (priv->vo_inited && sh_video)
	    vo_resume(vo_data);	// resume video
	*in_pause=0;
	(void)GetRelativeTime();	// keep TF around FT in next cycle
    }
    return rc;
}
#endif

typedef struct input_state_s {
    int		need_repaint;
    int		after_dvdmenu;
    int		next_file;
}input_state_t;

static int mpxp_handle_input(seek_args_t* seek,osd_args_t* osd,input_state_t* state) {
    priv_t*priv=mp_data->priv;
    stream_t* stream=priv->demuxer->stream;
    sh_video_t* sh_video=priv->demuxer->video->sh;
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
  while( (cmd = mp_input_get_cmd(priv->libinput,0,0,0)) != NULL) {
    switch(cmd->id) {
    case MP_CMD_SEEK : {
      int v,i_abs;
      v = cmd->args[0].v.i;
      i_abs = (cmd->nargs > 1) ? cmd->args[1].v.i : 0;
      if(i_abs) {
	seek->flags = DEMUX_SEEK_SET|DEMUX_SEEK_PERCENTS;
	if(sh_video) priv->osd_function= (v > dae_played_frame(xp_core->video).v_pts) ? OSD_FFW : OSD_REW;
	seek->secs = v/100.;
      }
      else {
	seek->flags = DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS;
	if(sh_video) priv->osd_function= (v > 0) ? OSD_FFW : OSD_REW;
	seek->secs+= v;
      }
    } break;
    case MP_CMD_SPEED_INCR :
    case MP_CMD_SPEED_MULT :
    case MP_CMD_SPEED_SET :
	MSG_WARN("Speed adjusting is not implemented yet!\n");
	break;
    case MP_CMD_SWITCH_AUDIO :
	MSG_INFO("ID_AUDIO_TRACK=%i\n",demuxer_switch_audio_r(priv->demuxer, priv->demuxer->audio->id+1));
	break;
    case MP_CMD_SWITCH_VIDEO :
	MSG_INFO("ID_VIDEO_TRACK=%i\n",demuxer_switch_video_r(priv->demuxer, priv->demuxer->video->id+1));
	break;
    case MP_CMD_SWITCH_SUB :
	MSG_INFO("ID_SUB_TRACK=%i\n",demuxer_switch_subtitle_r(priv->demuxer, priv->demuxer->sub->id+1));
	break;
    case MP_CMD_FRAME_STEP :
    case MP_CMD_PAUSE : {
      priv->osd_function=OSD_PAUSE;
    } break;
    case MP_CMD_SOFT_QUIT : {
      soft_exit_player();
      break;
    }
    case MP_CMD_QUIT : {
      exit_player(MSGTR_Exit_quit);
    }
    case MP_CMD_PLAY_TREE_STEP : {
      int n = cmd->args[0].v.i > 0 ? 1 : -1;
      play_tree_iter_t* it = play_tree_iter_new_copy(playtree_iter);

      if(play_tree_iter_step(it,n,0) == PLAY_TREE_ITER_ENTRY)
	eof = (n > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
      play_tree_iter_free(it);
    } break;
    case MP_CMD_PLAY_TREE_UP_STEP : {
      int n = cmd->args[0].v.i > 0 ? 1 : -1;
      play_tree_iter_t* it = play_tree_iter_new_copy(playtree_iter);
      if(play_tree_iter_up_step(it,n,0) == PLAY_TREE_ITER_ENTRY)
	eof = (n > 0) ? PT_UP_NEXT : PT_UP_PREV;
      play_tree_iter_free(it);
    } break;
    case MP_CMD_PLAY_ALT_SRC_STEP : {
      if(playtree_iter->num_files > 1) {
	int v = cmd->args[0].v.i;
	if(v > 0 && playtree_iter->file < playtree_iter->num_files)
	  eof = PT_NEXT_SRC;
	else if(v < 0 && playtree_iter->file > 1)
	  eof = PT_PREV_SRC;
      }
    } break;
    case MP_CMD_OSD :
      if(sh_video) {
	int v = cmd->args[0].v.i;
	if(v < 0)
	  mp_conf.osd_level=(mp_conf.osd_level+1)%4;
	else
	  mp_conf.osd_level= v > 3 ? 3 : v;
      } break;
    case MP_CMD_MUTE:
      mixer_mute(ao_data);
      break;
    case MP_CMD_VOLUME :  {
      int v = cmd->args[0].v.i;
      if(v > 0)
	mixer_incvolume(ao_data);
      else
	mixer_decvolume(ao_data);
#ifdef USE_OSD
      if(mp_conf.osd_level){
	osd->visible=sh_video->fps; // 1 sec
	vo_data->osd_progbar_type=OSD_VOLUME;
	vo_data->osd_progbar_value=(mixer_getbothvolume(ao_data)*256.0)/100.0;
	vo_osd_changed(OSDTYPE_PROGBAR);
      }
#endif
    } break;
    case MP_CMD_CONTRAST :  {
      int v = cmd->args[0].v.i, i_abs = cmd->args[1].v.i;
      if(i_abs) v_cont=v;
      else      v_cont+=v;
      if(v_cont > 100) v_cont=100;
      if(v_cont < -100) v_cont = -100;
      if(mpcv_set_colors(sh_video,VO_EC_CONTRAST,v_cont)==MPXP_Ok){
#ifdef USE_OSD
	if(mp_conf.osd_level){
	  osd->visible=sh_video->fps; // 1 sec
	  vo_data->osd_progbar_type=OSD_CONTRAST;
	  vo_data->osd_progbar_value=((v_cont)<<8)/100;
	  vo_data->osd_progbar_value = ((v_cont+100)<<8)/200;
	  vo_osd_changed(OSDTYPE_PROGBAR);
	}
#endif
      }
    } break;
    case MP_CMD_BRIGHTNESS :  {
      int v = cmd->args[0].v.i, i_abs = cmd->args[1].v.i;
      if(i_abs)v_bright=v;
      else     v_bright+=v;
      if(v_bright > 100) v_bright = 100;
      if(v_bright < -100) v_bright = -100;
      if(mpcv_set_colors(sh_video,VO_EC_BRIGHTNESS,v_bright)==MPXP_Ok){
#ifdef USE_OSD
	if(mp_conf.osd_level){
	  osd->visible=sh_video->fps; // 1 sec
	  vo_data->osd_progbar_type=OSD_BRIGHTNESS;
	  vo_data->osd_progbar_value=((v_bright)<<8)/100;
	  vo_data->osd_progbar_value = ((v_bright+100)<<8)/200;
	  vo_osd_changed(OSDTYPE_PROGBAR);
	}
#endif
      }
    } break;
    case MP_CMD_HUE :  {
      int v = cmd->args[0].v.i, i_abs = cmd->args[1].v.i;
      if(i_abs) v_hue=v;
      else      v_hue+=v;
      if(v_hue > 100) v_hue = 100;
      if(v_hue < -100) v_hue = -100;
      if(mpcv_set_colors(sh_video,VO_EC_HUE,v_hue)==MPXP_Ok){
#ifdef USE_OSD
	if(mp_conf.osd_level){
	  osd->visible=sh_video->fps; // 1 sec
	  vo_data->osd_progbar_type=OSD_HUE;
	  vo_data->osd_progbar_value=((v_hue)<<8)/100;
	  vo_data->osd_progbar_value = ((v_hue+100)<<8)/200;
	  vo_osd_changed(OSDTYPE_PROGBAR);
	}
#endif
      }
    } break;
    case MP_CMD_SATURATION :  {
      int v = cmd->args[0].v.i, i_abs = cmd->args[1].v.i;
      if(i_abs) v_saturation=v;
      else      v_saturation+=v;
      if(v_saturation > 100) v_saturation = 100;
      if(v_saturation < -100) v_saturation = -100;
      if(mpcv_set_colors(sh_video,VO_EC_SATURATION,v_saturation)==MPXP_Ok){
#ifdef USE_OSD
	if(mp_conf.osd_level){
	  osd->visible=sh_video->fps; // 1 sec
	  vo_data->osd_progbar_type=OSD_SATURATION;
	  vo_data->osd_progbar_value=((v_saturation)<<8)/100;
	  vo_data->osd_progbar_value = ((v_saturation+100)<<8)/200;
	  vo_osd_changed(OSDTYPE_PROGBAR);
	}
#endif
      }
    } break;
    case MP_CMD_FRAMEDROPPING :  {
      int v = cmd->args[0].v.i;
      if(v < 0)
	mp_conf.frame_dropping = (mp_conf.frame_dropping+1)%3;
      else
	mp_conf.frame_dropping = v > 2 ? 2 : v;
      priv->osd_show_framedrop = osd->info_factor;
    } break;
    case MP_CMD_TV_STEP_CHANNEL:
	if(cmd->args[0].v.i > 0) cmd->id=MP_CMD_TV_STEP_CHANNEL_UP;
	else	  cmd->id=MP_CMD_TV_STEP_CHANNEL_DOWN;
    case MP_CMD_TV_STEP_NORM:
    case MP_CMD_TV_STEP_CHANNEL_LIST:
	stream->driver->control(stream,SCRTL_MPXP_CMD,(any_t*)cmd->id);
	break;
    case MP_CMD_DVDNAV:
      if(stream->driver->control(stream,SCRTL_MPXP_CMD,(any_t*)cmd->args[0].v.i)==SCTRL_OK)
      {
	if(cmd->args[0].v.i!=MP_CMD_DVDNAV_SELECT)
	{
//		seek->flags = DEMUX_SEEK_SET|DEMUX_SEEK_PERCENTS;
//		seek->secs = 0.;
		stream->type|=STREAMTYPE_MENU;
		state->need_repaint=1;
	}
	priv->osd_function=OSD_DVDMENU;
	if(cmd->args[0].v.i==MP_CMD_DVDNAV_SELECT)
	{
		priv->osd_function=NULL;
		state->need_repaint=1;
		state->after_dvdmenu=1;
		state->next_file=1;
		return eof;
//		goto goto_next_file; /* menu may have different size against of movie */
	}
      }
      break;
    case MP_CMD_VO_FULLSCREEN:
	vo_fullscreen(vo_data);
	break;
    case MP_CMD_VO_SCREENSHOT:
	vo_screenshot(vo_data,dae_curr_vplayed(xp_core));
	break;
    case MP_CMD_SUB_POS:
    {
        int v;
	v = cmd->args[0].v.i;

	sub_data.pos+=v;
	if(sub_data.pos >100) sub_data.pos=100;
	if(sub_data.pos <0) sub_data.pos=0;
	vo_osd_changed(OSDTYPE_SUBTITLE);
    }	break;
    default : {
      MSG_ERR("Received unknow cmd %s\n",cmd->name);
    }
    }
    mp_cmd_free(cmd);
  }
  return eof;
}
/******************************************\
* MAIN MPLAYERXP FUNCTION !!!              *
\******************************************/
int main(int argc,char* argv[], char *envp[]){
    mpxp_init_antiviral_protection(1);
    int i;
    stream_t* stream=NULL;
    int stream_dump_type=0;
    input_state_t input_state = { 0, 0, 0 };
    char *ao_subdevice;
    char* filename=NULL; //"MI2-Trailer.avi";
    int file_format=DEMUXER_TYPE_UNKNOWN;

// movie info:
    int eof=0;
    osd_args_t osd = { 100, 9 };
    int forced_subs_only=0;
    seek_args_t seek_args = { 0, DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS };

    mp_conf.malloc_debug=0;
    for(i=0;i<argc;i++) if(strcmp(argv[i],"-core.malloc-debug")==0) { mp_conf.malloc_debug=1; break; }
    mp_init_malloc(argv[0],1000,10,mp_conf.malloc_debug?MPA_FLG_BACKTRACE:MPA_FLG_RANDOMIZER);
//    mp_init_malloc(argv[0],1000,10,MPA_FLG_BOUNDS_CHECK);
//    mp_init_malloc(argv[0],1000,10,MPA_FLG_BEFORE_CHECK);

    mpxp_init_structs();
    priv_t*priv=mp_data->priv;
    vo_data=vo_preinit_structs();
    init_signal_handling();

    xmp_init();
    xmp_register_main(exit_sighandler);

    mpxp_init_keyboard_fifo();

    mp_msg_init(MSGL_STATUS);
    MSG_INFO("%s",banner_text);
  /* Test for cpu capabilities (and corresponding OS support) for optimizing */

    priv->playtree = play_tree_new();

    mp_data->mconfig = m_config_new(priv->playtree,priv->libinput);
    m_config_register_options(mp_data->mconfig,mplayer_opts);
    // TODO : add something to let modules register their options
    mp_register_options(mp_data->mconfig);
    parse_cfgfiles(mp_data->mconfig);

    if(m_config_parse_command_line(mp_data->mconfig, argc, argv, envp)!=MPXP_Ok)
	exit_player("Error parse command line"); // error parsing cmdline

    if(!mp_conf.xp) {
	MSG_ERR("Error: detected option: -core.xp=0\n"
		"Note!  Single-thread mode is not longer supported by MPlayerXP\n");
	exit_player(MSGTR_Exit_quit);
    }
    if(mp_conf.test_av) {
	int verb=1;
	if(mpxp_test_antiviral_protection(&verb)==MPXP_Virus)
	    exit_player("Bad test of antiviral protection");
    }

    xp_num_cpu=get_number_cpu();
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
    get_mmx_optimizations();
#endif
    if(!sws_init()) {
	MSG_ERR("MPlayerXP requires working copy of libswscaler\n");
	exit_player(MSGTR_Exit_quit);
    }
    if(mp_conf.shuffle_playback) priv->playtree->flags|=PLAY_TREE_RND;
    else			 priv->playtree->flags&=~PLAY_TREE_RND;

    priv->playtree = play_tree_cleanup(priv->playtree);
    if(priv->playtree) {
      playtree_iter = play_tree_iter_new(priv->playtree,mp_data->mconfig);
      if(playtree_iter) {
	if(play_tree_iter_step(playtree_iter,0,0) != PLAY_TREE_ITER_ENTRY) {
	  play_tree_iter_free(playtree_iter);
	  playtree_iter = NULL;
	}
	filename = play_tree_iter_get_file(playtree_iter,1);
      }
    }

    xp_core->num_a_buffs = vo_conf.xp_buffs;

    init_player();

    if(!filename){
	show_help();
	exit_player(MSGTR_Exit_quit);
    }

    // Many users forget to include command line in bugreports...
    if(mp_conf.verbose){
	MSG_INFO("CommandLine:");
	for(i=1;i<argc;i++) MSG_INFO(" '%s'",argv[i]);
	MSG_INFO("\n");
    }

    mp_msg_init(mp_conf.verbose+MSGL_STATUS);

//------ load global data first ------
    mpxp_init_osd();
// ========== Init keyboard FIFO (connection to libvo) ============

    MP_UNIT(NULL);


// ******************* Now, let's see the per-file stuff ********************
play_next_file:

    // We must enable getch2 here to be able to interrupt network connection
    // or cache filling
    if(!mp_conf.use_stdin && !mp_conf.slave_mode){
	getch2_enable();  // prepare stdin for hotkeys...
	priv->inited_flags|=INITED_GETCH2;
    }

    ao_subdevice=mpxp_init_output_subsystems();
    if(filename) MSG_OK(MSGTR_Playing, filename);

    forced_subs_only=mpxp_init_vobsub(filename);

    MP_UNIT("mplayer");
    if(!input_state.after_dvdmenu && priv->demuxer) {
	free_stream(priv->demuxer->stream);
	priv->demuxer->stream=NULL;
	priv->inited_flags&=~INITED_STREAM;
	free_demuxer(priv->demuxer);
	priv->demuxer=NULL;
	priv->inited_flags&=~INITED_DEMUXER;
    }
    if(priv->demuxer) {
	priv->demuxer->audio=NULL;
	priv->demuxer->video=NULL;
	priv->demuxer->sub=NULL;
	priv->demuxer->audio->sh=NULL;
	priv->demuxer->video->sh=NULL;
    }
//============ Open & Sync STREAM --- fork cache2 ====================
    stream_dump_type=0;
    if(mp_conf.stream_dump)
	if((stream_dump_type=dump_parse(mp_conf.stream_dump))==0) {
	    MSG_ERR("Wrong dump parameters! Unable to continue\n");
	    exit_player(MSGTR_Exit_error);
	}

    if(stream_dump_type) mp_conf.s_cache_size=0;
    MP_UNIT("open_stream");
    if(!input_state.after_dvdmenu) stream=open_stream(priv->libinput,filename,&file_format,stream_dump_type>1?dump_stream_event_handler:mpxp_stream_event_handler);
    if(!stream) { // error...
	eof = libmpdemux_was_interrupted(PT_NEXT_ENTRY);
	goto goto_next_file;
    }
    priv->inited_flags|=INITED_STREAM;

    if(stream->type & STREAMTYPE_TEXT) {
	eof=mpxp_handle_playlist(filename);
	goto goto_next_file;
    }

    MP_UNIT(NULL);

    // CACHE2: initial prefill: 20%  later: 5%  (should be set by -cacheopts)
    if(mp_conf.s_cache_size && !stream_dump_type){
	MP_UNIT("enable_cache");
	if(!stream_enable_cache(stream,priv->libinput,mp_conf.s_cache_size*1024,mp_conf.s_cache_size*1024/5,mp_conf.s_cache_size*1024/20))
	    if((eof = libmpdemux_was_interrupted(PT_NEXT_ENTRY))) goto goto_next_file;
    }

    // DUMP STREAMS:
    if(stream_dump_type==1) dump_stream(stream);

//============ Open priv->demuxerS --- DETECT file type =======================
    if(mp_conf.playbackspeed_factor!=1.0) mp_conf.has_audio=0;
    xp_core->initial_apts=HUGE;
    if(!mp_conf.has_audio) mp_conf.audio_id=-2;  // do NOT read audio packets...
    if(!mp_conf.has_video) mp_conf.video_id=-2;  // do NOT read video packets...
    if(!mp_conf.has_dvdsub) mp_conf.dvdsub_id=-2;// do NOT read subtitle packets...

    MP_UNIT("demux_open");

    if(!input_state.after_dvdmenu) priv->demuxer=demux_open(stream,file_format,mp_conf.audio_id,mp_conf.video_id,mp_conf.dvdsub_id);
    if(!priv->demuxer) goto goto_next_file; // exit_player(MSGTR_Exit_error); // ERROR
    priv->inited_flags|=INITED_DEMUXER;
    input_state.after_dvdmenu=0;

    demux_stream_t *d_video;
    demux_stream_t *d_audio;
    demux_stream_t *d_dvdsub;
    d_audio=priv->demuxer->audio;
    d_video=priv->demuxer->video;
    d_dvdsub=priv->demuxer->sub;

/* Add NLS support here */
    mpxp_init_dvd_nls();

    if(mp_conf.seek_to_byte) stream_skip(stream,mp_conf.seek_to_byte);

    sh_audio_t* sh_audio;
    sh_video_t* sh_video;

    sh_audio=priv->demuxer->audio->sh;
    sh_video=priv->demuxer->video->sh;

    mpxp_print_stream_formats();

    if(sh_video) mpxp_read_video_properties();

    fflush(stdout);

    if(!sh_video && !sh_audio) {
	MSG_FATAL("No stream found\n");
	goto goto_next_file; // exit_player(MSGTR_Exit_error);
    }

//================== Read SUBTITLES (DVD & TEXT) ==========================
    if(sh_video) mpxp_read_subtitles(filename,forced_subs_only,stream_dump_type);

//================== Init AUDIO (codec) ==========================
    MP_UNIT("init_audio_codec");

    if(sh_audio) mpxp_find_acodec();

    if(stream_dump_type>1) {
	dump_mux_init(priv->demuxer,priv->libinput);
	goto dump_file;
    }

    if(!(ao_data=ao_init(0,ao_subdevice))) {
	MSG_ERR(MSGTR_CannotInitAO);
	sh_audio=d_audio->sh=NULL;
    }
    if(ao_subdevice) mp_free(ao_subdevice);

    if(sh_audio){
	MSG_V("Initializing audio codec...\n");
	if(mpca_init(sh_audio)!=MPXP_Ok){
	    MSG_ERR(MSGTR_CouldntInitAudioCodec);
	    sh_audio=d_audio->sh=NULL;
	} else {
	    MSG_V("AUDIO: srate=%d  chans=%d  bps=%d  sfmt=0x%X  ratio: %d->%d\n"
	    ,sh_audio->rate,sh_audio->nch,afmt2bps(sh_audio->afmt)
	    ,sh_audio->afmt,sh_audio->i_bps,sh_audio->af_bps);
	}
    }

    if(sh_audio)   priv->inited_flags|=INITED_ACODEC;

    if(stream_dump_type>1) {
	dump_file:
	dump_mux(priv->demuxer,mp_conf.av_sync_pts,mp_conf.seek_to_sec,mp_conf.play_n_frames);
	goto goto_next_file;
    }
/*================== Init VIDEO (codec & libvo) ==========================*/
    if(!sh_video) goto main;

    MP_UNIT("init_video_filters");
    if(sh_video->vfilter_inited<=0) {
	sh_video->vfilter=vf_init(sh_video,priv->libinput);
	sh_video->vfilter_inited=1;
    }
    if((mpxp_find_vcodec())!=0) {
	if(!sh_audio) goto goto_next_file;
	goto main;
    }

    xp_core->num_v_buffs=vo_get_num_frames(vo_data); /* that really known after init_vcodecs */

    if(mp_conf.autoq>0){
	/* Auto quality option enabled*/
	MPXP_Rc rc;
	unsigned quality;
	rc=mpcv_get_quality_max(sh_video,&quality);
	if(rc==MPXP_Ok) mp_data->output_quality=quality;
	if(mp_conf.autoq>mp_data->output_quality) mp_conf.autoq=mp_data->output_quality;
	else mp_data->output_quality=mp_conf.autoq;
	MSG_V("AutoQ: setting quality to %d\n",mp_data->output_quality);
	mpcv_set_quality(sh_video,mp_data->output_quality);
    }

    vf_showlist(sh_video->vfilter);
// ========== Init display (sh_video->src_w*sh_video->src_h/out_fmt) ============

    priv->inited_flags|=INITED_VO;
    MSG_V("INFO: Video OUT driver init OK!\n");
    MP_UNIT("init_libvo");
    fflush(stdout);

//================== MAIN: ==========================
main:
    if(!sh_video) mp_conf.osd_level = 0;
    else if(sh_video->fps<60) osd.info_factor=sh_video->fps/2; /* 0.5 sec */

//================ SETUP AUDIO ==========================

    if(sh_audio) if((mpxp_configure_audio())!=0) goto goto_next_file;

    MP_UNIT("av_init");

    if(mp_conf.av_force_pts_fix2==1 ||
	(mp_conf.av_force_pts_fix2==-1 && mp_conf.av_sync_pts &&
	(d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_ES ||
	d_video->demuxer->file_format == DEMUXER_TYPE_MPEG4_ES ||
	d_video->demuxer->file_format == DEMUXER_TYPE_H264_ES ||
	d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_PS ||
	d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_TS)))
	    mp_data->use_pts_fix2=1;
    else
	    mp_data->use_pts_fix2=0;

    if(sh_video) sh_video->chapter_change=0;

    if(sh_audio) { // <- ??? always true
	sh_audio->chapter_change=0;
	sh_audio->a_pts=HUGE;
    } else {
	MSG_INFO(MSGTR_NoSound);
	if(mp_conf.verbose) MSG_V("Freeing %d unused audio chunks\n",d_audio->packs);
	ds_free_packs(d_audio); // mp_free buffered chunks
	d_audio->id=-2;         // do not read audio chunks
	if(priv->ao_inited) uninit_player(INITED_AO); // close device
    }

    if(!sh_video){
	MSG_INFO("Video: no video!!!\n");
	if(mp_conf.verbose) MSG_V("Freeing %d unused video chunks\n",d_video->packs);
	ds_free_packs(d_video);
	d_video->id=-2;
	if(priv->vo_inited) uninit_player(INITED_VO);
    }

    if(!sh_audio && !sh_video) exit_player("Nothing to do");

    if(priv->demuxer->file_format!=DEMUXER_TYPE_AVI) pts_from_bps=0; // it must be 0 for mpeg/asf!

    if(mp_conf.force_fps && sh_video) {
	sh_video->fps=mp_conf.force_fps;
	MSG_INFO(MSGTR_FPSforced,sh_video->fps,1.0f/sh_video->fps);
    }

    /* Init timers and benchmarking */
    mp_data->rtc_fd=InitTimer();
    if(!mp_conf.nortc && mp_data->rtc_fd>0) { close(mp_data->rtc_fd); mp_data->rtc_fd=-1; }
    MSG_V("Using %s timing\n",mp_data->rtc_fd>0?"rtc":mp_conf.softsleep?"software":"usleep()");

    mp_data->bench->total_start=GetTimer();
    mp_data->bench->audio=0; mp_data->bench->audio_decode=0; mp_data->bench->video=0;
    mp_data->bench->audio_decode_correction=0;

    if(mp_conf.benchmark) init_benchmark();

    /* display clip info */
    demux_info_print(priv->demuxer,filename);

    mpxp_run_ahead_engine();

    fflush(stdout);
    fflush(stderr);
/*
   let thread will decode ahead!
   We may print something in block window ;)
 */
    mp_data->seek_time = GetTimerMS();

    if(sh_video) dae_wait_decoder_outrun(xp_core->video);

    if(xmp_run_players()!=0) exit_player("Can't run xp players!\n");
    MSG_OK("Using the next %i threads:\n",xp_core->num_threads);
    unsigned idx;
    for(idx=0;idx<xp_core->num_threads;idx++)
	MSG_OK("[%i] %s (id=%u, pth_id=%lu)\n"
	,idx
	,xp_core->mpxp_threads[idx]->name
	,xp_core->mpxp_threads[idx]->pid
	,xp_core->mpxp_threads[idx]->pth_id);

//==================== START PLAYING =======================

    MSG_OK(MSGTR_StartPlaying);fflush(stdout);

    mp_msg_flush();
    while(!eof){
	int in_pause=0;

	eof |= xp_core->audio->eof;
/*========================== UPDATE TIMERS ============================*/
	MP_UNIT("Update timers");
	if(sh_video && input_state.need_repaint) goto repaint;
	if(sh_audio) eof = xp_core->audio->eof;
	if(sh_video) eof|=dae_played_eof(xp_core->video);
	if(!sh_video) {
	    if(mp_conf.benchmark && mp_conf.verbose) show_benchmark_status();
	    else mpxp_print_audio_status();
	}
	usleep(250000);
//read_input:
	if(sh_video) vo_check_events(vo_data);
repaint:
#ifdef USE_OSD
	if((mpxp_paint_osd(&osd.visible,&in_pause))!=0) goto repaint;
#endif

//================= Keyboard events, SEEKing ====================

	memset(&input_state,0,sizeof(input_state_t));
	eof=mpxp_handle_input(&seek_args,&osd,&input_state);
	if(input_state.next_file) goto goto_next_file;

	if (mp_conf.seek_to_sec) {
	    int a,b; float d;

	    if (sscanf(mp_conf.seek_to_sec, "%d:%d:%f", &a,&b,&d)==3)
		seek_args.secs += 3600*a +60*b +d ;
	    else if (sscanf(mp_conf.seek_to_sec, "%d:%f", &a, &d)==2)
		seek_args.secs += 60*a +d;
	    else if (sscanf(mp_conf.seek_to_sec, "%f", &d)==1)
		seek_args.secs += d;
	    mp_conf.seek_to_sec = NULL;
	}
  /* Looping. */
	if(eof && mp_conf.loop_times>=0) {
	    MSG_V("loop_times = %d, eof = %d\n", mp_conf.loop_times,eof);

	    if(mp_conf.loop_times>1) mp_conf.loop_times--; else
	    if(mp_conf.loop_times==1) mp_conf.loop_times=-1;

	    eof=0;
	    xp_core->audio->eof=0;
	    seek_args.flags=DEMUX_SEEK_SET|DEMUX_SEEK_PERCENTS;
	    seek_args.secs=0; // seek to start of movie (0%)
	}

	if(seek_args.secs || (seek_args.flags&DEMUX_SEEK_SET)) {
	    MP_UNIT("seek");

	    xmp_halt_threads(0);

	    if(seek_args.secs && sh_video) {
	    xmp_frame_t shvap = dae_played_frame(xp_core->video);
	    xmp_frame_t shvad = dae_prev_decoded_frame(xp_core->video);
		seek_args.secs -= (xp_core->bad_pts?shvad.v_pts:d_video->pts)-shvap.v_pts;
	    }

	    mpxp_seek(&osd,&seek_args);

	    xp_core->audio->eof=0;
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

    MSG_V("EOF code: %d\n",eof);

goto_next_file:  // don't jump here after ao/vo/getch initialization!

    if(mp_conf.benchmark) show_benchmark();

if(playtree_iter != NULL && !input_state.after_dvdmenu) {
if(eof == PT_NEXT_ENTRY || eof == PT_PREV_ENTRY) {
  eof = eof == PT_NEXT_ENTRY ? 1 : -1;
  if(play_tree_iter_step(playtree_iter,eof,0) == PLAY_TREE_ITER_ENTRY) {
    uninit_player(INITED_ALL-(INITED_LIRC+INITED_INPUT+INITED_VO));
    eof = 1;
  } else {
    play_tree_iter_free(playtree_iter);
    playtree_iter = NULL;
  }
} else if (eof == PT_UP_NEXT || eof == PT_UP_PREV) {
  eof = eof == PT_UP_NEXT ? 1 : -1;
  if(play_tree_iter_up_step(playtree_iter,eof,0) == PLAY_TREE_ITER_ENTRY) {
    uninit_player(INITED_ALL-(INITED_LIRC+INITED_INPUT+INITED_VO));
    eof = 1;
  } else {
    play_tree_iter_free(playtree_iter);
    playtree_iter = NULL;
  }
}else { // NEXT PREV SRC
     uninit_player(INITED_ALL-(INITED_LIRC+INITED_INPUT+INITED_VO+INITED_DEMUXER));
     eof = eof == PT_PREV_SRC ? -1 : 1;
}
}
    uninit_player(INITED_VO);

    if(eof == 0) eof = 1;

if(!input_state.after_dvdmenu)
while(playtree_iter != NULL) {
  filename = play_tree_iter_get_file(playtree_iter,eof);
  if(filename == NULL) {
    if( play_tree_iter_step(playtree_iter,eof,0) != PLAY_TREE_ITER_ENTRY) {
      play_tree_iter_free(playtree_iter);
      playtree_iter = NULL;
    };
  } else
    break;
}

    if( playtree_iter != NULL ){
	int flg;
	flg=INITED_ALL;
	if(input_state.after_dvdmenu) flg &=~(INITED_STREAM|INITED_DEMUXER);
	uninit_player(flg&(~INITED_INPUT)); /* TODO: |(~INITED_AO)|(~INITED_VO) */
	priv->vo_inited=0;
	priv->ao_inited=0;
	eof = 0;
	xp_core->audio->eof=0;
	goto play_next_file;
    }

    if(stream_dump_type>1) dump_mux_close(priv->demuxer);
    exit_player(MSGTR_Exit_eof);

    mpxp_uninit_structs();
    return 1;
}