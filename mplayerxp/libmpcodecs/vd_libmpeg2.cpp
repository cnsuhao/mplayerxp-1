#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 we still need that:
 benchmarks:
    ffmpeg12-0.4.9.pre.2 = 47.07%
    libmpeg2-0.2.0       = 42.46%
    libmpeg2-0.4.0       = 37.12%
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "mplayerxp.h"
#include "vd_internal.h"
#include "osdep/cpudetect.h"
#include "osdep/mm_accel.h"
#include "postproc/postprocess.h"
#include "codecs_ld.h"
#include "osdep/bswap.h"

#include "libmpdemux/parse_es.h"
#include "libvo2/video_out.h"
#ifdef ATTRIBUTE_ALIGNED_MAX
#define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
#else
#define ATTR_ALIGN(align)
#endif

namespace	usr {
static const video_probe_t probes[] = {
    { "mpeg2", "libmpeg2"SLIBSUFFIX, 0x10000001,                  VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, 0x10000002,                  VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('A','V','M','P'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('D','V','R',' '), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('L','M','P','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','1','V',' '), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','1','V','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','2','V','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','7','0','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','M','E','S'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','P','2','V'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','P','G','V'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','P','E','G'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','P','G','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','P','G','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','X','5','P'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('P','I','M','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('P','I','M','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','3'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','5'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','6'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','7'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','8'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','9'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('H','D','V','A'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','X','3','N'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','X','3','P'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','X','4','N'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','X','4','P'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('M','X','5','N'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','3'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','5'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','6'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','7'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','8'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','9'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','A'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','B'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','C'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','D'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','E'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','5','F'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','3'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','5'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','6'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','7'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','8'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','9'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','A'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','B'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','C'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','D'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','E'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "mpeg2", "libmpeg2"SLIBSUFFIX, FOURCC_TAG('X','D','V','F'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

struct mpeg2dec_t;

struct mpeg2_sequence_t {
    unsigned int width, height;
    unsigned int chroma_width, chroma_height;
    unsigned int byte_rate;
    unsigned int vbv_buffer_size;
    uint32_t flags;

    unsigned int picture_width, picture_height;
    unsigned int display_width, display_height;
    unsigned int pixel_width, pixel_height;
    unsigned int frame_period;

    uint8_t profile_level_id;
    uint8_t colour_primaries;
    uint8_t transfer_characteristics;
    uint8_t matrix_coefficients;
};

struct mpeg2_gop_t {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t pictures;
    uint32_t flags;
};

enum {
    PIC_MASK_CODING_TYPE=7,
    PIC_FLAG_CODING_TYPE_I=1,
    PIC_FLAG_CODING_TYPE_P=2,
    PIC_FLAG_CODING_TYPE_B=3,
    PIC_FLAG_CODING_TYPE_D=4,
    PIC_FLAG_TOP_FIELD_FIRST=8,
    PIC_FLAG_PROGRESSIVE_FRAME=16,
    PIC_FLAG_COMPOSITE_DISPLAY=32,
    PIC_FLAG_SKIP=64,
    PIC_FLAG_TAGS=128,
    PIC_FLAG_REPEAT_FIRST_FIELD=256,
    PIC_MASK_COMPOSITE_DISPLAY=0xfffff000
};
struct mpeg2_picture_t {
    unsigned int temporal_reference;
    unsigned int nb_fields;
    uint32_t tag, tag2;
    uint32_t flags;
    struct {
	int x, y;
    } display_offset[3];
};

struct mpeg2_fbuf_t {
    uint8_t * buf[3];
    any_t* id;
};

struct mpeg2_info_t {
    const mpeg2_sequence_t * sequence;
    const mpeg2_gop_t * gop;
    const mpeg2_picture_t * current_picture;
    const mpeg2_picture_t * current_picture_2nd;
    const mpeg2_fbuf_t * current_fbuf;
    const mpeg2_picture_t * display_picture;
    const mpeg2_picture_t * display_picture_2nd;
    const mpeg2_fbuf_t * display_fbuf;
    const mpeg2_fbuf_t * discard_fbuf;
    const uint8_t * user_data;
    unsigned int user_data_len;
};

typedef enum {
    STATE_BUFFER = 0,
    STATE_SEQUENCE = 1,
    STATE_SEQUENCE_REPEATED = 2,
    STATE_GOP = 3,
    STATE_PICTURE = 4,
    STATE_SLICE_1ST = 5,
    STATE_PICTURE_2ND = 6,
    STATE_SLICE = 7,
    STATE_END = 8,
    STATE_INVALID = 9,
    STATE_INVALID_END = 10,
    STATE_SEQUENCE_MODIFIED = 11
} mpeg2_state_t;


    class libmpeg2_decoder : public Video_Decoder {
	public:
	    libmpeg2_decoder(video_decoder_t&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~libmpeg2_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    MPXP_Rc		load_lib(const std::string& libname);
	    void		draw_frame(mp_image_t *mpi,unsigned w,const mpeg2_fbuf_t *src) const;

	    video_decoder_t&	parent;
	    sh_video_t&		sh;
	    mpeg2dec_t*		mpeg2dec;
	    any_t*		dll_handle;
	    mpeg2dec_t*		(*mpeg2_init_ptr) (unsigned);
	    void		(*mpeg2_close_ptr) (mpeg2dec_t * mpeg2dec);
	    const mpeg2_info_t*	(*mpeg2_info_ptr) (mpeg2dec_t * mpeg2dec);
	    int			(*mpeg2_parse_ptr) (mpeg2dec_t * mpeg2dec);
	    void		(*mpeg2_buffer_ptr) (mpeg2dec_t * mpeg2dec, uint8_t * start, uint8_t * end);
	    void		(*mpeg2_set_buf_ptr) (mpeg2dec_t * mpeg2dec, uint8_t * buf[3], any_t* id);
	    int			(*mpeg2_stride_ptr) (mpeg2dec_t * mpeg2dec, int stride);
	    void		(*mpeg2_reset_ptr) (mpeg2dec_t * mpeg2dec, int full_reset);
	    const video_probe_t*	probe;
    };

video_probe_t libmpeg2_decoder::get_probe_information() const { return *probe; }

extern const vd_info_t vd_libmpeg2_info;
MPXP_Rc libmpeg2_decoder::load_lib(const std::string& libname)
{
    if(!(dll_handle=ld_codec(libname,vd_libmpeg2_info.url))) return MPXP_False;
    mpeg2_init_ptr = (mpeg2dec_t* (*)(unsigned int))ld_sym(dll_handle,"mpeg2_init");
    mpeg2_close_ptr = (void (*)(mpeg2dec_t*))ld_sym(dll_handle,"mpeg2_close");
    mpeg2_info_ptr = (const mpeg2_info_t* (*)(mpeg2dec_t*))ld_sym(dll_handle,"mpeg2_info");
    mpeg2_parse_ptr = (int (*)(mpeg2dec_t*))ld_sym(dll_handle,"mpeg2_parse");
    mpeg2_buffer_ptr = (void (*)(mpeg2dec_t*,uint8_t*,uint8_t*))ld_sym(dll_handle,"mpeg2_buffer");
    mpeg2_set_buf_ptr = (void (*)(mpeg2dec_t*,uint8_t**,any_t*))ld_sym(dll_handle,"mpeg2_set_buf");
    mpeg2_stride_ptr = (int (*)(mpeg2dec_t*,int))ld_sym(dll_handle,"mpeg2_stride");
    mpeg2_reset_ptr = (void (*)(mpeg2dec_t*,int))ld_sym(dll_handle,"mpeg2_reset");
    return (mpeg2_init_ptr && mpeg2_close_ptr && mpeg2_info_ptr &&
	mpeg2_parse_ptr && mpeg2_buffer_ptr && mpeg2_set_buf_ptr &&
	mpeg2_stride_ptr && mpeg2_reset_ptr)?MPXP_Ok:MPXP_False;
}

// to set/get/query special features/parameters
MPXP_Rc libmpeg2_decoder::ctrl(int cmd,any_t* arg,long arg2){
    UNUSED(arg2);
    switch(cmd) {
	case VDCTRL_RESYNC_STREAM:
	    /*lib starts looking for the next sequence header.*/
	    (*mpeg2_reset_ptr)(mpeg2dec,1);
	    return MPXP_True;
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12)
			return MPXP_True;
	    else 	return MPXP_False;
	default: break;
    }
    return MPXP_Unknown;
}

libmpeg2_decoder::libmpeg2_decoder(video_decoder_t& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t fourcc)
	    :Video_Decoder(p,_sh,psi,fourcc)
	    ,parent(p)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    if(!load_lib(probe->codec_dll)) throw bad_format_exception();

    if(!(mpeg2dec=(*mpeg2_init_ptr)(mpxp_context().mplayer_accel))) throw bad_format_exception();

    if(mpcodecs_config_vf(parent,sh.src_w,sh.src_h)!=MPXP_Ok) throw bad_format_exception();
}

// uninit driver
libmpeg2_decoder::~libmpeg2_decoder() {
    if(mpeg2dec) (*mpeg2_close_ptr)(mpeg2dec);
    if(dll_handle) ::dlclose(dll_handle);
}

void libmpeg2_decoder::draw_frame(mp_image_t* mpi,unsigned w,const mpeg2_fbuf_t* src) const
{
    mpi->planes[0]=src->buf[0];
    mpi->planes[1]=src->buf[1];
    mpi->planes[2]=src->buf[2];
    mpi->stride[0]=w;
    mpi->stride[1]=
    mpi->stride[2]=w>>1;
    mpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
    mpcodecs_draw_image(parent,mpi);
}

// decode a frame
mp_image_t* libmpeg2_decoder::run(const enc_frame_t& frame) {
    mp_image_t *mpi;
    const mpeg2_info_t* _info;
    int state,buf;
    if(frame.len<=0) return NULL; // skipped null frame

    _info=(*mpeg2_info_ptr)(mpeg2dec);
    mpi=NULL;
    buf=0;
    mpxp_dbg2<<"len="<<frame.len<<" ***mpeg2_info***"<<std::endl;
    while(1)
    {
	state=(*mpeg2_parse_ptr)(mpeg2dec);
	mpxp_dbg2<<state<<"=mpeg2_parse"<<std::endl;
	switch(state)
	{
	    case STATE_BUFFER:
		(*mpeg2_buffer_ptr)(mpeg2dec,reinterpret_cast<uint8_t*>(frame.data),reinterpret_cast<uint8_t*>(frame.data)+frame.len);
		buf++;
		if(buf>2) return NULL; /* parsing of the passed buffer finished, return. */
		break;
	    case STATE_PICTURE:
#if 0
		    if(!mpeg2dec->decoder.mpq_store)
		    {
			mpeg2dec->decoder.mpq_stride=(_info->sequence->picture_width+15)>>4;
			mpeg2dec->decoder.mpq_store=mp_malloc(mpeg2dec->decoder.mpq_stride*((_info->sequence->picture_height+15)>>4));
		    }
#endif
		    mpi=mpcodecs_get_image(parent,MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_DRAW_CALLBACK
					,_info->sequence->width,_info->sequence->height);
		    (*mpeg2_stride_ptr)(mpeg2dec,mpi->stride[0]);
		    break;
	    case STATE_SLICE:
	    case STATE_END:
	    case STATE_INVALID:
		/* we must call draw_frame() only after STATE_BUFFER and STATE_PICTURE events */
		mpxp_dbg2<<"display="<<std::hex<<_info->display_fbuf<<" discard="<<std::hex<<_info->discard_fbuf<<" current="<<std::hex<<_info->current_fbuf<<" mpi="<<std::endl;
		/* Workaround for broken (badly demuxed) streams.
		Reset libmpeg2 to start decoding at the next picture. */
		if(state==STATE_END) (*mpeg2_reset_ptr)(mpeg2dec,0);
		if (_info->display_fbuf && mpi)
		{
		    mpi->pict_type=_info->current_picture->flags&PIC_MASK_CODING_TYPE;
#if 0
		    mpi->qscale_type= 1;
		    mpi->qscale=mpeg2dec->decoder.mpq_store;
		    mpi->qstride=mpeg2dec->decoder.mpq_stride;
#endif
		    draw_frame (mpi,_info->sequence->width,_info->display_fbuf);
		    return mpi;
		}
		break;
	    default: break;
	}
    }
    return NULL; /* segfault */
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(video_decoder_t& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) libmpeg2_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_libmpeg2_info = {
    "libmpeg2 MPEG 1/2 Video decoder",
    "mpeg2",
    "A'rpi",
    "http://libmpeg2.sourceforge.net",
    query_interface,
    options
};
} // namespace	usr
