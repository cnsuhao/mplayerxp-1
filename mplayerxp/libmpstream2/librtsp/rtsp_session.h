#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * This file was ported to MPlayer from xine CVS rtsp_session.h,v 1.4 2003/01/31 14:06:18
 */

/*
 * Copyright (C) 2000-2002 the xine project
 *
 * This file is part of xine, a mp_free video player.
 *
 * xine is mp_free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * high level interface to rtsp servers.
 *
 *    2006, Benjamin Zores and Vincent Mussard
 *      Support for MPEG-TS streaming through RFC compliant RTSP servers
 */

#ifndef HAVE_RTSP_SESSION_H
#define HAVE_RTSP_SESSION_H
#include "../rtp_cache.h"
struct real_rtsp_session_t;
namespace mpxp {
    class Rtp_Rtsp_Session;
    class Rtsp;
    struct Rtsp_Session : public Opaque {
	public:
	    Rtsp_Session();
	    virtual ~Rtsp_Session();

	    static Rtsp_Session* start(Tcp& tcp, char **mrl, const std::string& path, const std::string& host,
					int port, int *redir, uint32_t bandwidth, const std::string& user, const std::string& pass);
	    virtual int		read(char *data, int len);
	    virtual void	end();
	private:
	    Rtsp*		s;
	    real_rtsp_session_t*real_session;
	    Rtp_Rtsp_Session*	rtp_session;
	    Rtp_Cache*		rtp;
    };
} // namespace mpxp
#endif
