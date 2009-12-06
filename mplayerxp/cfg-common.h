#ifdef MAIN_CONF /* this will be included in conf[] */
// ------------------------- stream options --------------------
#ifdef HAVE_STREAMING
	{"ipv4", &network_prefer_ipv4, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#ifdef HAVE_AF_INET6
	{"ipv6", &network_prefer_ipv4, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#else
	{"ipv6", "MPlayerXP was compiled without IPv6 support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif /* HAVE_AF_INET6 */
	{"ipv4-only-proxy", &network_ipv4_only_proxy, CONF_TYPE_FLAG, 0, 0, 1, NULL},	
	{"netuser", &network_username, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"netpasswd", &network_password, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"netbandwidth", &network_bandwidth, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"net-user-agent", &network_useragent, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"netcookies", &network_cookies_enabled, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"cookies-file", &cookies_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif
	{"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 4, 65536, NULL},
	{"nocache", &stream_cache_size, CONF_TYPE_FLAG, 0, 1, 0, NULL},

#ifdef HAVE_LIBCSS
	{"dvdauth", &dvd_auth_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"dvdkey", &dvdimportkey, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"csslib", &css_so, CONF_TYPE_STRING, 0, 0, 0, NULL},
#else
        {"dvdauth", "MPlayerXP was compiled WITHOUT libcss support!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
        {"dvdkey", "MPlayerXP was compiled WITHOUT libcss support!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"csslib", "MPlayerXP was compiled WITHOUT libcss support!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif

// ------------------------- demuxer options --------------------

	{"ni", &force_ni, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noni", &force_ni, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	{"noidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 0, NULL},
	{"idx", &index_mode, CONF_TYPE_FLAG, 0, -1, 1, NULL},
	{"forceidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 2, NULL},

	{"aid", &audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"vid", &video_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"sid", &dvdsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
	{"ifo", &spudec_ifo, CONF_TYPE_STRING, 0, 0, 0, NULL},

// ------------------------- a-v sync options --------------------

	{"frames", &play_n_frames, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

	{"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 10, NULL},
	{"fps", &force_fps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, NULL},
	{"srate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, NULL},

// ------------------------- codec/pp options --------------------

#ifdef USE_FAKE_MONO
	{"stereo", &fakemono, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
#endif
	{"sound", &has_audio, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nosound", &has_audio, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	{"af-adv", audio_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
	{"af", &af_cfg.list, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vf", &vf_cfg.list, CONF_TYPE_STRING, 0, 0, 0, NULL},

	{"afm", &audio_family, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vfm", &video_family, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ac", &audio_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vc", &video_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},

	{"divxq", "Option -divxq has been renamed to -npp (postprocessing), use -npp !\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"pp", "Option -pp is obsolete! Use -npp instead\n" ,
	    CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"npp", &npp_options, CONF_TYPE_STRING, 0, 0, 0, NULL},

	{"sws", &sws_flags, CONF_TYPE_INT, 0, 0, 2, NULL},
/*UD*/	{"ssf", scaler_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

#else

#include "mp_config.h"

extern int sws_chr_vshift;
extern int sws_chr_hshift;
extern float sws_chr_gblur;
extern float sws_lum_gblur;
extern float sws_chr_sharpen;
extern float sws_lum_sharpen;

const struct config scaler_filter_conf[]={
	{"lgb", &sws_lum_gblur, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
	{"cgb", &sws_chr_gblur, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
	{"cvs", &sws_chr_vshift, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"chs", &sws_chr_hshift, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"ls", &sws_lum_sharpen, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
	{"cs", &sws_chr_sharpen, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

#include "postproc/af.h"
extern af_cfg_t af_cfg; // Audio filter configuration, defined in libmpcodecs/dec_audio.c
const struct config audio_filter_conf[]={       
	{"list", &af_cfg.list, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {"force", &af_cfg.force, CONF_TYPE_INT, CONF_RANGE, 0, 7, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

#include "postproc/vf.h"
extern vf_cfg_t vf_cfg; // Video filter configuration, defined in libmpcodecs/dec_video.c
const struct config video_filter_conf[]={       
	{"list", &vf_cfg.list, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {"force", &vf_cfg.force, CONF_TYPE_INT, CONF_RANGE, 0, 7, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

#endif
