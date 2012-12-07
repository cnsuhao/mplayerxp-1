/*
 *  Copyright (C) 2006 Benjamin Zores
 *   Network helpers for UDP connections (originally borrowed from rtp.c).
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

#ifndef UDP_H_INCLUDED
#define UDP_H_INCLUDED 1

namespace mpxp {
    typedef int net_fd_t;
    class Udp {
	public:
	    Udp(net_fd_t fd);
	    Udp(const URL_t* url,int reuse_socket=0);
	    virtual ~Udp();
	
	    void	open(const URL_t* url,int reuse_socket=0);
	    net_fd_t	socket() const { return _fd; }
	    int		established() const;
	    int		error() const { return _error; }
	private:
	    net_fd_t	_fd;
	    int		_error;
    };
} // namespace mpxp
#endif /* UDP_H */
