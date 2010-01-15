
#include "mp_config.h"
#include <stdlib.h>
#include <stdio.h>
#include "libmpdemux/stream.h"
#include "cfgparser.h"

extern void mp_input_register_options(m_config_t* cfg);
extern void libmpdemux_register_options(m_config_t* cfg);
extern void demuxer_register_options(m_config_t* cfg);
#ifdef HAVE_LIBCDIO
extern void cdda_register_options(m_config_t* cfg);
#endif
extern void libmpcodecs_ad_register_options(m_config_t* cfg);
extern void libmpcodecs_vd_register_options(m_config_t* cfg);

void mp_register_options(m_config_t* cfg)
{
  mp_input_register_options(cfg);
  libmpdemux_register_options(cfg);
  demuxer_register_options(cfg);
#ifdef HAVE_LIBCDIO
  cdda_register_options(cfg);
#endif
  libmpcodecs_ad_register_options(cfg);
  libmpcodecs_vd_register_options(cfg);
}
