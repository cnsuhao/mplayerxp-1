/* Imported from the dvbstream project
 *
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id: rtp.h,v 1.2 2007/11/17 12:43:37 nickols_k Exp $
 */

#ifndef RTP_H
#define RTP_H
#include "network.h"

int read_rtp_from_server(Tcp& fd, char *buffer, int length);

#endif
