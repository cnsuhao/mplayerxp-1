#ifndef __MPLAYER_OSD_RENDER_H_INCLUDED
#define __MPLAYER_OSD_RENDER_H_INCLUDED 1

/* Generic alpha renderers for all YUV modes and RGB depths. */
/* These are "reference implementations", should be optimized later (MMX, etc) */
namespace	usr {
    class OSD_Render : public Opaque {
	public:
	    OSD_Render(unsigned fourcc);
	    virtual ~OSD_Render();

	    void render(int w,int h, const unsigned char* src, const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride,int finalize) const;
	private:
	    void	get_draw_alpha(unsigned fmt);
	    typedef void (* __FASTCALL__ draw_alpha_f)(int w,int h, const unsigned char* src, const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride,int finalize);
	    draw_alpha_f	draw_alpha_ptr;
	    draw_alpha_f	draw_alpha_yv12_ptr;
	    draw_alpha_f	draw_alpha_yuy2_ptr;
	    draw_alpha_f	draw_alpha_uyvy_ptr;
	    draw_alpha_f	draw_alpha_rgb24_ptr;
	    draw_alpha_f	draw_alpha_rgb32_ptr;
	    draw_alpha_f	draw_alpha_rgb15_ptr;
	    draw_alpha_f	draw_alpha_rgb16_ptr;
    };
} // namespace	usr

#endif
