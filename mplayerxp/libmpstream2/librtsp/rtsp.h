/*
 * This file was ported to MPlayer from xine CVS rtsp.h,v 1.2 2002/12/16 21:50:55
 */

/*
 * Copyright (C) 2002 the xine project
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
 * a minimalistic implementation of rtsp protocol,
 * *not* RFC 2326 compilant yet.
 *
 *    2006, Benjamin Zores and Vincent Mussard
 *      fixed a lot of RFC compliance issues.
 */

#ifndef HAVE_RTSP_H
#define HAVE_RTSP_H
#include "libmpstream2/tcp.h"

namespace	usr {
/* some codes returned by rtsp_request_* functions */
    enum {
	RTSP_STATUS_SET_PARAMETER	=10,
	RTSP_STATUS_OK			=200,
	RTSP_MAX_FIELDS			=256
    };
static const char* RTSP_METHOD_OPTIONS="OPTIONS";
static const char* RTSP_METHOD_DESCRIBE="DESCRIBE";
static const char* RTSP_METHOD_SETUP="SETUP";
static const char* RTSP_METHOD_PLAY="PLAY";
static const char* RTSP_METHOD_TEARDOWN="TEARDOWN";
static const char* RTSP_METHOD_SET_PARAMETER="SET_PARAMETER";

    class Rtsp : public Opaque {
	public:
	    Rtsp(Tcp& tcp);
	    virtual ~Rtsp();

	    static Rtsp*	connect (Tcp& tcp, char *mrl, const char *path, const char *host, int port, const char *user_agent);

	    virtual int		request_options(const char *what);
	    virtual int		request_describe(const char *what);
	    virtual int		request_setup(const char *what, char *control);
	    virtual int		request_setparameter(const char *what);
	    virtual int		request_play(const char *what);
	    virtual int		request_teardown(const char *what);

	    virtual int		send_ok() const;
	    virtual int		read_data(char *buffer, unsigned int size) const;

	    virtual char*	search_answers(const char *tag) const;

	    virtual void	free_answers();

	    virtual void	close ();

	    virtual void	set_session(const char *id);
	    virtual const char*	get_session() const;

	    virtual char*	get_mrl() const;
	    virtual char*	get_param(const char *param) const;

	    virtual void	schedule_field(const char *string);
	    virtual void	unschedule_field(const char *string);
	    virtual void	unschedule_all();
	private:
	    char*		get() const;
	    void		put(const char *string) const;
	    int			get_code(const char *string) const;
	    void		send_request(const char *type, const char *what);
	    void		schedule_standard();
	    int			get_answers();
	    int			write_stream(const char *buf, int len) const;
	    ssize_t		read_stream(any_t*buf, size_t count) const;

	    Tcp&	tcp;

	    const char*	host;
	    int		port;
	    const char*	path;
	    const char*	param;
	    char*	mrl;
	    const char*	user_agent;

	    const char*	server;
	    unsigned	server_state;
	    uint32_t	server_caps;

	    unsigned	cseq;
	    const char*	session;

	    char*	answers[RTSP_MAX_FIELDS];   /* data of last message */
	    char*	scheduled[RTSP_MAX_FIELDS]; /* will be sent with next message */
    };
} // namespace	usr
#endif

