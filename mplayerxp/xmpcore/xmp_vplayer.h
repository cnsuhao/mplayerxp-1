#ifndef XMP_VPLAYER_H_INCLUDED
#define XMP_VPLAYER_H_INCLUDED 1

namespace mpxp {
    any_t*	xmp_video_player( any_t* arg );
    void	sig_video_play( void );
} // namespace
extern float max_pts_correction;
#endif
