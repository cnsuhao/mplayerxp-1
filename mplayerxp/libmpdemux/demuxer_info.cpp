#include "demuxer_info.h"

#include "nls/nls.h"

#include "demux_msg.h"


namespace mpxp {
static const char *info_names[INFOT_MAX] =
{
    "Author",
    "Name",
    "Subject",
    "Copyright",
    "Description",
    "Album",
    "Date",
    "Track",
    "Genre",
    "Encoder",
    "SrcMedia",
    "WWW",
    "Mail",
    "Rating",
    "Comments",
    "Mime"
};

Demuxer_Info::Demuxer_Info() {}
Demuxer_Info::~Demuxer_Info() {
    unsigned i;
    for(i=0;i<INFOT_MAX;i++)
	if(id[i])
	    delete id[i];
}

MPXP_Rc Demuxer_Info::add(unsigned opt, const char *param)
{
    if(!opt || opt > INFOT_MAX) {
	MSG_WARN("Unknown info type %u\n",opt);
	return MPXP_False;
    }
    opt--;
    if(id[opt]) {
	MSG_V( "Demuxer info '%s' already present as '%s'!\n",info_names[opt],id[opt]);
	delete id[opt];
    }
    id[opt]=nls_recode2screen_cp(sub_data.cp,param,strlen(param));
    return MPXP_Ok;
}

int Demuxer_Info::print(const char *filename) const
{
    unsigned i;
    MSG_HINT(" CLIP INFO (%s):\n",filename);
    for(i=0;i<INFOT_MAX;i++)
	if(id[i])
	    MSG_HINT("   %s: %s\n",info_names[i],id[i]);
    return 0;
}

const char* Demuxer_Info::get(unsigned opt) const {
    if(!opt || opt > INFOT_MAX) return NULL;
    return id[opt-1];
}
} // namespaec mpxp;
