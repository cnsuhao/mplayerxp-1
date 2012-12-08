/*
 *  Copyright (C) 2001 Bertrand BAUDET, 2006 Benjamin Zores
 *   Network helpers for TCP connections
 *   (originally borrowed from network.c,
 *      by Bertrand BAUDET <bertrand_baudet@yahoo.com>).
 *
 *   This program is mp_free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef TCP_H_INCLUDED
#define TCP_H_INCLUDED 1
#include <stdint.h>
#include "input2/input.h"

namespace mpxp {

    typedef int net_fd_t;
    class Tcp {
	public:
	    enum tcp_af_e {
		IP4=0,
		IP6
	    };
	    enum tcp_error_e {
		Err_Timeout	=-3, /* connection timeout */
		Err_Fatal	=-2, /* unable to resolve name */
		Err_Port	=-1, /* unable to connect to a particular port */
		Err_None	=0   /* no error */
	    };
	    Tcp(libinput_t* libinput,net_fd_t fd);
	    Tcp(libinput_t* libinput,const char *host,int port,tcp_af_e af=Tcp::IP4);
	    virtual ~Tcp();

	    Tcp&	operator=(Tcp& other);
	    Tcp&	operator=(net_fd_t fd);

	    virtual int		established() const;
	    virtual int		has_data(int timeout) const;
	    virtual tcp_error_e	error() const;

	    virtual void	open(const char *host,int port,tcp_af_e af=Tcp::IP4);
	    virtual int		read(uint8_t* buf,unsigned len,int flags=0);
	    virtual int		write(const uint8_t* buf,unsigned len,int flags=0) const;
	    virtual void	close();
	private:
	    net_fd_t	_fd;
	    tcp_error_e	_error;
	    libinput_t*	libinput;
    };
} // namespace mpxp
#endif /* TCP_H */
