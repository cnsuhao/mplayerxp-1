#ifndef __PLAYTREEPARSER_H
#define __PLAYTREEPARSER_H
#include "osdep/mplib.h"

#include "playtree.h"

namespace	usr {
    class PlayTree_Parser : public Opaque {
	public:
	    PlayTree_Parser(Stream* _stream,int _deep);
	    virtual ~PlayTree_Parser();

	    virtual PlayTree*	get_play_tree(libinput_t&libinput);
	private:
	    void		reset();
	    char*		get_line();
	    void		stop_keeping();
	    PlayTree*		parse_asx(libinput_t& libinput);
	    PlayTree*		parse_pls();
	    PlayTree*		parse_textplain();

	    Stream*	stream;
	    char*	buffer;
	    char*	iter;
	    char*	line;
	    int		buffer_size;
	    int		buffer_end;
	    int		deep;
	    int		keep;
    };
}// namespace	usr
#endif
