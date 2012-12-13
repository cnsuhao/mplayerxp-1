#ifndef __MPLAYERXP_MAIN
#define __MPLAYERXP_MAIN 1

#include <string>

#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "mp_config.h"
#include "osdep/mplib.h"
#include "xmpcore/xmp_enums.h"
#include "libmpconf/cfgparser.h"
#include "libmpsub/subreader.h"
#include "libao2/audio_out.h"
#include "libvo2/video_out.h"

struct audio_decoder_t;
struct video_decoder_t;
namespace mpxp {
    /* List of all modules which require protection by pin-code */
    enum {
	Module_Stream=0,
	Module_Demuxer,
	Module_AudioDecoder,
	Module_VideoDecoder,
	Module_AudioFilters,
	Module_VideoFilters,
	Module_AudioOut,
	Module_VideoOut,
	Module_XPCore,
	Module_MPContext
    };

#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
    struct x86_features_t {
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
    };
#endif

    struct MP_Config {
	MP_Config();
	~MP_Config() {}

	int		has_video;
	int		has_audio;
	int		has_dvdsub;
	uint32_t	msg_filter;
	int		test_av;
	int		malloc_debug;
	unsigned	max_trace;
// XP-core
	int		xp;   /* XP-mode */
	int		gomp; /* currently it's experimental feature */
	const char*	stream_dump; // dump name
	int		s_cache_size; /* cache2: was 1024 let it be 0 by default */
	int		autoq; /* quality's options: */
	unsigned	verbose;
	int		benchmark;
	float		playbackspeed_factor;
// sync
	int		frame_dropping; // option  0=no drop  1= drop vo  2= drop decode
	int		av_sync_pts;
	int		av_force_pts_fix;
	int		av_force_pts_fix2;
	int		frame_reorder;
	float		force_fps;
	int		softsleep;
	int		nortc;
// streaming:
	int		audio_id;
	int		video_id;
	int		dvdsub_id;
	int		vobsub_id;
	const char*	audio_lang;
	const char*	dvdsub_lang;
	const char*	spudec_ifo;
	unsigned	force_srate;
// seek:
	const char*	seek_to_sec;
	long long int	seek_to_byte;
	int		loop_times;
	int		shuffle_playback;
	int		play_n_frames;
/* codecs: */
	const char*	audio_codec;  /* override audio codec */
	const char*	video_codec;  /* override video codec */
	const char*	audio_family; /* override audio codec family */
	const char*	video_family; /* override video codec family */
// drivers:
	char*		video_driver; //"mga"; // default
	char*		audio_driver;
// sub:
	int		osd_level;
	const char*	font_name;
	float		font_factor;
	const char*	sub_name;
	float		sub_fps;
	int		sub_auto;
	const char*	vobsub_name;
	int		subcc_enabled;
// others
	const char*	npp_options;
	unsigned	ao_channels;
	int		z_compression;
	float		monitor_pixel_aspect;
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
	x86_features_t	x86;
#endif
    };
    extern MP_Config mp_conf;

    /* Benchmarking */
    typedef struct time_usage_s {
	double video;
	double vout;
	double audio_decode_correction;
	double audio_decode;  /**< time for decoding in thread */
	double audio,max_audio,min_audio,cur_audio;
	double min_audio_decode;
	double max_audio_decode;
	double max_demux;
	double demux;
	double min_demux;
	double max_c2;
	double c2;
	double min_c2;
	double max_video;
	double cur_video;
	double min_video;
	double max_vout;
	double cur_vout;
	double min_vout;
	double total_start;
    }time_usage_t;

    struct MPXPSystem;
    struct xp_core_t;
    struct mpxp_engine_t {
	MPXPSystem*	MPXPSys;
	xp_core_t*	xp_core;
    };

    struct audio_processing_t {
	audio_decoder_t*	decoder;
	ao_data_t*		output;
    };

    struct video_processing_t {
	video_decoder_t*	decoder;
	Video_Output*		output;
    };

    /* non-configurable through command line stuff */
    struct MPXPContext :public Opaque {
	public:
	    MPXPContext();
	    virtual ~MPXPContext();

	    virtual mpxp_engine_t& engine() const { return *_engine; }
	    virtual audio_processing_t& audio() const { return *_audio; }
	    virtual video_processing_t& video() const { return *_video; }

	    int			rtc_fd;
	    int			seek_time;
	    int			output_quality;
	    unsigned		mpxp_after_seek;
	    int			use_pts_fix2;
	    unsigned		mplayer_accel;
	    subtitle*		subtitles;
	    m_config_t*		mconfig;
	    time_usage_t*	bench;
	    any_t*		msg_priv;
	private:
	    LocalPtr<mpxp_engine_t> _engine;
	    LocalPtr<audio_processing_t> _audio;
	    LocalPtr<video_processing_t> _video;
    };

    MPXPContext& mpxp_context();

    unsigned get_number_cpu(void);
    void show_help(void);
    void show_long_help(void);


    void update_osd( float v_pts );

    extern pthread_mutex_t audio_timer_mutex;

    void exit_player(const std::string& why);

    /* 10 ms or 10'000 microsecs is optimal time for thread sleeping */
    inline int yield_timeslice() { return ::usleep(10000); }

    inline void escape_player(const std::string& why,unsigned num_calls) {
	show_backtrace(why,num_calls);
	exit_player(why);
    }

    inline MPXP_Rc check_pin(const std::string& module,unsigned pin1,unsigned pin2) {
	if(pin1!=pin2) {
	    std::string msg;
	    msg=std::string("Found incorrect PIN in module: ")+module;
	    escape_player(msg,mp_conf.max_trace);
	}
	return MPXP_Ok;
    }
    void mpxp_resync_audio_stream(void);
    void mpxp_reset_vcache(void);
    void __exit_sighandler(void);

    void mplayer_put_key(int code);

    void mp_register_options(m_config_t* cfg);

    extern play_tree_iter_t* playtree_iter;
}
#endif
