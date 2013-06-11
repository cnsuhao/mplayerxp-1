#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
  xacodec.c -- XAnim Video Codec DLL support

  (C) 2001 Alex Beregszaszi <alex@naxine.org>
       and Arpad Gereoffy <arpi@thot.banki.hu>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strerror */
#ifdef __FreeBSD__
#include <unistd.h>
#endif
#include <dlfcn.h> /* ld_sym, dlopen, dlclose */
#include <stdarg.h> /* va_alist, va_start, va_end */
#include <errno.h> /* strerror, errno */

#include "osdep/bswap.h"

#include "libmpconf/codec-cfg.h"

#include "libvo2/img_format.h"
#include "osdep/timer.h"
#include "osdep/fastmemcpy.h"
#include "codecs_ld.h"

#include "libvo2/img_format.h"
#include "vd.h"
#include "vd_msg.h"

namespace	usr {
/*************************** START OF XA CODEC BINARY INTERFACE ****************/
    struct XAVID_FUNC_HDR {
	unsigned int	what;
	unsigned int	id;
	int		(*iq_func)();	/* init/query function */
	unsigned int	(*dec_func)();  /* opt decode function */
    };

    enum {
	XAVID_WHAT_NO_MORE=0x0000,
	XAVID_AVI_QUERY	=0x0001,
	XAVID_QT_QUERY	=0x0002,
	XAVID_DEC_FUNC	=0x0100,

	XAVID_API_REV	=0x0003
    };

    struct XAVID_MOD_HDR {
	unsigned int	api_rev;
	char		*desc;
	char		*rev;
	char		*copyright;
	char		*mod_author;
	char		*authors;
	unsigned int	num_funcs;
	XAVID_FUNC_HDR	*funcs;
    };

/* XA CODEC .. */
    struct XA_CODEC_HDR {
	any_t*		anim_hdr;
	unsigned long	compression;
	unsigned long	x, y;
	unsigned long	depth;
	any_t*		extra;
	unsigned long	xapi_rev;
	unsigned long	(*decoder)();
	char*		description;
	unsigned long	avi_ctab_flag;
	unsigned long	(*avi_read_ext)();
    };

    enum {
	CODEC_SUPPORTED=1,
	CODEC_UNKNOWN=0,
	CODEC_UNSUPPORTED=-1
    };

/* fuckin colormap structures for xanim */
    struct ColorReg {
	unsigned short	red;
	unsigned short	green;
	unsigned short	blue;
	unsigned short	gray;
    };

    typedef struct XA_ACTION_STRUCT {
	int				type;
	int				cmap_rev;
	unsigned char*		data;
	struct XA_ACTION_STRUCT*	next;
	struct XA_CHDR_STRUCT*	chdr;
	ColorReg*			h_cmap;
	unsigned int*		map;
	struct XA_ACTION_STRUCT*	next_same_chdr;
    }XA_ACTION;

    typedef struct XA_CHDR_STRUCT {
	unsigned int		rev;
	ColorReg*		cmap;
	unsigned int		csize, coff;
	unsigned int*		map;
	unsigned int		msize, moff;
	struct XA_CHDR_STRUCT*	next;
	XA_ACTION*		acts;
	struct XA_CHDR_STRUCT*	new_chdr;
    }XA_CHDR;

    struct XA_DEC_INFO {
	unsigned int	cmd;
	unsigned int	skip_flag;
	unsigned int	imagex, imagey;	/* image buffer size */
	unsigned int	imaged;		/* image depth */
	XA_CHDR*	chdr;		/* color map header */
	unsigned int	map_flag;
	unsigned int*	map;
	unsigned int	xs, ys;
	unsigned int	xe, ye;
	unsigned int	special;
	any_t*		extra;
    };

    struct XA_ANIM_HDR {
	unsigned int	file_num;
	unsigned int	anim_type;
	unsigned int	imagex;
	unsigned int	imagey;
	unsigned int	imagec;
	unsigned int	imaged;
    };

    // Added by A'rpi
    struct xacodec_image_t {
	unsigned int	out_fmt;
	unsigned	bpp;
	unsigned	width,height;
	unsigned char*	planes[3];
	unsigned	stride[3];
	unsigned char*	mem;
    };

    enum {
	xaFALSE=0,
	xaTRUE
    };

    class xanim_decoder : public Video_Decoder {
	public:
	    xanim_decoder(VD_Interface&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~xanim_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    int				xacodec_init(const std::string& filename);
	    int				xacodec_exit();
	    void			TheEnd1(const std::string& err_mess);
	    void			XA_Add_Func_To_Free_Chain(XA_ANIM_HDR* anim_hdr, void (*function)());
	    int				xacodec_init_video(int out_format);
	    xacodec_image_t*		xacodec_decode_frame(uint8_t* frame, int frame_size, int skip_flag);
	    int				xacodec_query(XA_CODEC_HDR *codec_hdr);

	    VD_Interface&		parent;
	    sh_video_t&			sh;
	    const video_probe_t*	probe;

	    static const int		XA_CLOSE_FUNCS=5;

	    XA_DEC_INFO*		decinfo;
	    any_t*			file_handler;
	    long			(*iq_func)(XA_CODEC_HDR *codec_hdr);
	    unsigned int		(*dec_func)(unsigned char *image, unsigned char *delta,
						    unsigned int dsize, XA_DEC_INFO *dec_info);
	    any_t*			close_func[XA_CLOSE_FUNCS];
	    xacodec_image_t		image;
	    int				xa_close_func;
	    /* 0 is no debug (needed by 3ivX) */
	    long			xa_debug;
    };


void xanim_decoder::TheEnd1(const std::string& err_mess)
{
    mpxp_info<<"error: "<<err_mess<<" - exiting"<<std::endl;
    xacodec_exit();

    return;
}

void xanim_decoder::XA_Add_Func_To_Free_Chain(XA_ANIM_HDR* anim_hdr, void (*function)())
{
    close_func[xa_close_func] = (any_t*)function;
    if (xa_close_func+1 < XA_CLOSE_FUNCS)
	xa_close_func++;

    return;
}
/* end of crap */

/* load, init and query */
int xanim_decoder::xacodec_init(const std::string& filename)
{
    any_t*(*what_the)();
    const char *error;
    XAVID_MOD_HDR *mod_hdr;
    XAVID_FUNC_HDR *func;
    unsigned int i;

    file_handler = ::dlopen(filename.c_str(), RTLD_NOW|RTLD_GLOBAL);
    if (!file_handler) {
	error = ::dlerror();
	if (error)
	    mpxp_v<<"xacodec: failed to dlopen "<<filename<<" while "<<error<<std::endl;
	else
	    mpxp_v<<"xacodec: failed to dlopen "<<filename<<std::endl;
	return 0;
    }

    what_the = (any_t* (*)())ld_sym(file_handler, "What_The");
    if ((error = ::dlerror()) != NULL) {
	mpxp_v<<"xacodec: failed to init "<<filename<<" while "<<error<<std::endl;
	::dlclose(file_handler);
	return 0;
    }

    mod_hdr = reinterpret_cast<XAVID_MOD_HDR *>(what_the());
    if (!mod_hdr) {
	mpxp_v<<"xacodec: initializer function failed in "<<filename<<std::endl;
	::dlclose(file_handler);
	return 0;
    }

    mpxp_info<<"=== XAnim Codec ==="<<std::endl;
    mpxp_info<<" Filename: "<<filename<<" (API revision: "<<std::hex<<mod_hdr->api_rev<<")"<<std::endl;
    mpxp_info<<" Codec: "<<mod_hdr->desc<<". Rev: "<<mod_hdr->rev<<std::endl;
    if (mod_hdr->copyright)
	mpxp_info<<" "<<mod_hdr->copyright<<std::endl;
    if (mod_hdr->mod_author)
	mpxp_info<<" Module Author(s): "<<mod_hdr->mod_author<<std::endl;
    if (mod_hdr->authors)
	mpxp_info<<" Codec Author(s): "<<mod_hdr->authors<<std::endl;

    if (mod_hdr->api_rev > XAVID_API_REV) {
	mpxp_v<<"xacodec: not supported api revision ("<<mod_hdr->api_rev<<") in %"<<filename<<std::endl;
	::dlclose(file_handler);
	return 0;
    }

    func = mod_hdr->funcs;
    if (!func) {
	mpxp_v<<"xacodec: function table error in "<<filename<<std::endl;
	::dlclose(file_handler);
	return 0;
    }

    mpxp_dbg2<<"Exported functions by codec: [functable: 0x"<<std::hex<<mod_hdr->funcs<<" entries: "<<mod_hdr->num_funcs<<"]"<<std::endl;
    for (i = 0; i < mod_hdr->num_funcs; i++) {
	mpxp_dbg2<<" "<<i<<": "<<func[i].what<<" "<<func[i].id<<" [iq:0x"<<std::hex<<func[i].iq_func<<" d:0x"<<std::hex<<func[i].dec_func<<"]"<<std::endl;
	if (func[i].what & XAVID_AVI_QUERY) {
	    mpxp_dbg2<<" 0x"<<std::hex<<func[i].iq_func<<": avi init/query func (id: "<<func[i].id<<")"<<std::endl;
	    iq_func = (long (*)(XA_CODEC_HDR*))func[i].iq_func;
	}
	if (func[i].what & XAVID_QT_QUERY) {
	    mpxp_dbg2<<" 0x"<<std::hex<<func[i].iq_func<<": qt init/query func (id: "<<func[i].id<<")"<<std::endl;
	    iq_func = (long (*)(XA_CODEC_HDR*))func[i].iq_func;
	}
	if (func[i].what & XAVID_DEC_FUNC) {
	    mpxp_dbg2<<" 0x"<<std::hex<<func[i].dec_func<<": decoder func (init/query: 0x"<<std::hex<<func[i].iq_func<<") (id: "<<func[i].id<<")"<<std::endl;
	    dec_func = (unsigned (*)(unsigned char*,unsigned char *,unsigned int, XA_DEC_INFO *))func[i].dec_func;
	}
    }
    return 1;
}

int xanim_decoder::xacodec_query(XA_CODEC_HDR *codec_hdr)
{
    long codec_ret;

    codec_ret = iq_func(codec_hdr);
    switch(codec_ret) {
	case CODEC_SUPPORTED:
	    dec_func = (unsigned (*)(unsigned char*,unsigned char *,unsigned int, XA_DEC_INFO *))codec_hdr->decoder;
	    mpxp_dbg2<<"Codec is supported: found decoder for "<<codec_hdr->description<<" at 0x"<<(any_t*)codec_hdr->decoder<<std::endl;
	    return 1;
	case CODEC_UNSUPPORTED:
	    mpxp_fatal<<"Codec ("<<codec_hdr->description<<") is unsupported by driver"<<std::endl;
	    return 0;
	case CODEC_UNKNOWN:
	default:
	    mpxp_fatal<<"Codec ("<<codec_hdr->description<<") is unknown by driver"<<std::endl;
	    return 0;
    }
}

static std::string xacodec_def_path = "/usr/lib/xanim/mods";

int xanim_decoder::xacodec_init_video(int out_format)
{
    char dll[1024];
    XA_CODEC_HDR codec_hdr;
    int i;

    iq_func = NULL;
    dec_func = NULL;

    for (i=0; i < XA_CLOSE_FUNCS; i++)
	close_func[i] = NULL;

    const std::map<std::string,std::string>& envm=mpxp_get_environment();
    std::map<std::string,std::string>::const_iterator it;
    it = envm.find("XANIM_MOD_DIR");
    if(it!=envm.end()) xacodec_def_path = (*it).second;

    snprintf(dll, 1024, "%s/%s", xacodec_def_path.c_str(), sh.codec->dll_name);
    if (xacodec_init(dll) == 0)
	return 0;

    codec_hdr.xapi_rev = XAVID_API_REV;
    codec_hdr.anim_hdr = mp_malloc(4096);
    codec_hdr.description = sh.codec->s_info;
    codec_hdr.compression = bswap_32(sh.bih->biCompression);
    codec_hdr.decoder = NULL;
    codec_hdr.x = sh.bih->biWidth; /* ->src_w */
    codec_hdr.y = sh.bih->biHeight; /* ->src_h */
    /* extra fields to store palette */
    codec_hdr.avi_ctab_flag = 0;
    codec_hdr.avi_read_ext = NULL;
    codec_hdr.extra = NULL;

    switch(out_format) {
/*	case IMGFMT_RGB8:
	    codec_hdr.depth = 8;
	    break;
	case IMGFMT_RGB15:
	    codec_hdr.depth = 15;
	    break;
	case IMGFMT_RGB16:
	    codec_hdr.depth = 16;
	    break;
	case IMGFMT_RGB24:
	    codec_hdr.depth = 24;
	    break;
	case IMGFMT_RGB32:
	    codec_hdr.depth = 32;
	    break;
	case IMGFMT_BGR8:
	    codec_hdr.depth = 8;
	    break;
	case IMGFMT_BGR15:
	    codec_hdr.depth = 15;
	    break;
	case IMGFMT_BGR16:
	    codec_hdr.depth = 16;
	    break;
	case IMGFMT_BGR24:
	    codec_hdr.depth = 24;
	    break;
	case IMGFMT_BGR32:
	    codec_hdr.depth = 32;
	    break;*/
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	    codec_hdr.depth = 12;
	    break;
	default:
	    mpxp_v<<"xacodec: not supported image out format ("<<vo_format_name(out_format)<<")"<<std::endl;
	    return 0;
    }
    mpxp_v<<"xacodec: querying for input "<<codec_hdr.x<<"x"<<codec_hdr.y<<" "<<codec_hdr.depth<<"bit [fourcc: "<<std::hex<<std::setw(4)<<codec_hdr.compression
	<<"] ("<<codec_hdr.description<<")..."<<std::endl;

    if (xacodec_query(&codec_hdr) == 0) return 0;

//    delete codec_hdr.anim_hdr;

    decinfo = new(zeromem) XA_DEC_INFO;

    decinfo->cmd = 0;
    decinfo->skip_flag = 0;
    decinfo->imagex = decinfo->xe = codec_hdr.x;
    decinfo->imagey = decinfo->ye = codec_hdr.y;
    decinfo->imaged = codec_hdr.depth;
    decinfo->chdr = NULL;
    decinfo->map_flag = 0; /* xaFALSE */
    decinfo->map = NULL;
    decinfo->xs = decinfo->ys = 0;
    decinfo->special = 0;
    decinfo->extra = codec_hdr.extra;
    mpxp_dbg2<<"decinfo->extra, filled by codec: "<<(any_t*)&decinfo->extra<<" ["<<decinfo->extra<<"]"<<std::endl;

    image.out_fmt = out_format;
    image.bpp = codec_hdr.depth;
    image.width = codec_hdr.x;
    image.height = codec_hdr.y;
    image.mem = new unsigned char [codec_hdr.y * codec_hdr.x * ((codec_hdr.depth+7)/8)];

    return 1;
}

enum {
    ACT_DLTA_NORM	=0x00000000,
    ACT_DLTA_BODY	=0x00000001,
    ACT_DLTA_XOR	=0x00000002,
    ACT_DLTA_NOP	=0x00000004,
    ACT_DLTA_MAPD	=0x00000008,
    ACT_DLTA_DROP	=0x00000010,
    ACT_DLTA_BAD	=0x80000000
};

//    unsigned int (*dec_func)(unsigned char *image, unsigned char *delta,
//	unsigned int dsize, XA_DEC_INFO *dec_info);

xacodec_image_t* xanim_decoder::xacodec_decode_frame(uint8_t* frame, int frame_size, int skip_flag)
{
    unsigned int ret;

    decinfo->skip_flag = skip_flag;

    image.planes[0]=image.mem;
    image.stride[0]=image.width;
    image.stride[1]=image.stride[2]=image.width/2;
    switch(image.out_fmt){
    case IMGFMT_YV12:
	image.planes[2]=image.planes[0]+image.width*image.height;
	image.planes[1]=image.planes[2]+image.width*image.height/4;
	break;
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	image.planes[1]=image.planes[0]+image.width*image.height;
	image.planes[2]=image.planes[1]+image.width*image.height/4;
	break;
    }

    ret = dec_func((uint8_t*)&image, frame, frame_size, decinfo);


    if (ret == ACT_DLTA_NORM) {
	return &image;
    }

    if (ret & ACT_DLTA_MAPD)
	mpxp_dbg2<<"mapd"<<std::endl;

    if (ret & ACT_DLTA_XOR) {
	mpxp_dbg2<<"xor"<<std::endl;
	return &image;
    }

    /* nothing changed */
    if (ret & ACT_DLTA_NOP) {
	mpxp_dbg2<<"nop"<<std::endl;
	return NULL;
    }

    /* frame dropped (also display latest frame) */
    if (ret & ACT_DLTA_DROP) {
	mpxp_dbg2<<"drop"<<std::endl;
	return NULL;
    }

    if (ret & ACT_DLTA_BAD) {
	mpxp_dbg2<<"bad"<<std::endl;
	return NULL;
    }

    /* used for double buffer */
    if (ret & ACT_DLTA_BODY) {
	mpxp_dbg2<<"body"<<std::endl;
	return NULL;
    }

    return NULL;
}

int xanim_decoder::xacodec_exit()
{
    int i;
    void (*_close_func)();
    for (i=0; i < XA_CLOSE_FUNCS; i++)
	if (close_func[i]) {
	    _close_func = reinterpret_cast<void(*)()>(close_func[i]);
	    _close_func();
	}
    ::dlclose(file_handler);
    if (decinfo != NULL) delete decinfo;
    return TRUE;
}

/* *** EOF XANIM *** */

/*************************** END OF XA CODEC BINARY INTERFACE ******************/

video_probe_t xanim_decoder::get_probe_information() const { return *probe; }

static const video_probe_t probes[] = {
    { "xanim", "vid_3ivX.xa", FOURCC_TAG('3','I','V','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "xanim", "vid_cvid.xa", FOURCC_TAG('C','V','I','D'), VCodecStatus_Problems,{IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "xanim", "vid_h261.xa", FOURCC_TAG('H','2','6','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "xanim", "vid_h263.xa", FOURCC_TAG('H','2','6','3'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "xanim", "vid_h263.xa", FOURCC_TAG('V','I','V','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "xanim", "vid_h263.xa", FOURCC_TAG('V','I','V','O'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "xanim", "vid_iv32.xa", FOURCC_TAG('I','V','3','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "xanim", "vid_iv32.xa", FOURCC_TAG('I','V','3','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "xanim", "vid_iv41.xa", FOURCC_TAG('I','V','4','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "xanim", "vid_iv50.xa", FOURCC_TAG('I','V','5','0'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None } }
};

xanim_decoder::xanim_decoder(VD_Interface& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t fourcc)
	    :Video_Decoder(p,_sh,psi,fourcc)
	    ,parent(p)
	    ,sh(_sh)
{
    unsigned i;
    MPXP_Rc rc=MPXP_False;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    if(xacodec_init_video(sh.codec->outfmt[sh.outfmtidx])) {
	rc=parent.config_vf(sh.src_w,sh.src_h);
    }
    if(rc!=MPXP_Ok) throw bad_format_exception();
}

// uninit driver
xanim_decoder::~xanim_decoder() {
    xacodec_exit();
}

// to set/get/query special features/parameters
MPXP_Rc xanim_decoder::ctrl(int cmd,any_t* arg,long arg2){
    UNUSED(arg2);
    switch(cmd) {
      case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12 ||
		*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV)
			return MPXP_True;
	    else 	return MPXP_False;
      default: break;
    }
    return MPXP_Unknown;
}

// decode a frame
mp_image_t* xanim_decoder::run(const enc_frame_t& frame){
    mp_image_t* mpi;
    xacodec_image_t* _image;

    if(frame.len<=0) return NULL; // skipped frame

    _image=xacodec_decode_frame(reinterpret_cast<uint8_t*>(frame.data),frame.len,(frame.flags&3)?1:0);
    if(!_image) return NULL;

    mpi=parent.get_image(MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE,sh.src_w, sh.src_h);
    if(!mpi) return NULL;

    mpi->planes[0]=_image->planes[0];
    mpi->planes[1]=_image->planes[1];
    mpi->planes[2]=_image->planes[2];
    mpi->stride[0]=_image->stride[0];
    mpi->stride[1]=_image->stride[1];
    mpi->stride[2]=_image->stride[2];

    return mpi;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(VD_Interface& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) xanim_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_xanim_info = {
    "XAnim codecs",
    "xanim",
    "A'rpi & Alex <Xanim (http://xanim.va.pubnix.com/)>",
    "build-in",
    query_interface,
    options
};
} // namespace	usr