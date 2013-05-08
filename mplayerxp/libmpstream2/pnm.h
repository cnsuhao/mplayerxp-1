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
 * $Id: pnm.h,v 1.2 2007/12/12 08:20:03 nickols_k Exp $
 *
 * pnm util functions header by joschka
 */

#ifndef HAVE_PNM_H
#define HAVE_PNM_H

#include <string>
#ifndef __CYGWIN__
#include <inttypes.h>
#endif
/*#include "xine_internal.h" */

namespace	usr {
    class Tcp;

    class Pnm : public Opaque {
	public:
	    Pnm(Tcp& tcp);
	    virtual ~Pnm();

	    enum sizes {
		BUF_SIZE=4096,
		HEADER_SIZE=4096
	    };

	    virtual MPXP_Rc	connect(const std::string& path);
	    virtual int		read (char *data, int len);
	    virtual void	close ();
	    virtual int		peek_header(char *data) const;
	private:
	    unsigned int	get_chunk(unsigned int max, unsigned int *chunk_type,char *data, int *need_response) const;
	    int			write_chunk(uint16_t chunk_id, uint16_t length, const char *chunk, char *data) const;
	    void		send_request(uint32_t bandwidth);
	    void		send_response(const char *response);
	    int			get_headers(int *need_response);
	    int			calc_stream();
	    int			get_stream_chunk();

	    Tcp&	tcp;
	    std::string	path;
	    char	buffer[Pnm::BUF_SIZE]; /* scratch buffer */
	    /* receive buffer */
	    uint8_t	recv[Pnm::BUF_SIZE];
	    int		recv_size;
	    int		recv_read;

	    uint8_t	header[Pnm::HEADER_SIZE];
	    int		header_len;
	    int		header_read;
	    unsigned	seq_num[4];     /* two streams with two indices   */
	    unsigned	seq_current[2]; /* seqs of last stream chunk read */
	    uint32_t	ts_current;     /* timestamp of current chunk     */
	    uint32_t	ts_last[2];     /* timestamps of last chunks      */
	    unsigned	packet;         /* number of last recieved packet */
    };
} // namespace	usr
#endif

