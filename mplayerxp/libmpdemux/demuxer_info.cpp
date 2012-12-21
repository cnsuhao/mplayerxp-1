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
    "Comments"
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
	mpxp_warn<<"Unknown info type "<<opt<<std::endl;
	return MPXP_False;
    }
    opt--;
    if(id[opt]) {
	mpxp_v<<"Demuxer info '"<<info_names[opt]<<"' already present as '"<<id[opt]<<"'!"<<std::endl;
	delete id[opt];
    }
    id[opt]=nls_recode2screen_cp(sub_data.cp,param,strlen(param));
    return MPXP_Ok;
}

int Demuxer_Info::print(const std::string& filename) const
{
    unsigned i;
    mpxp_hint<<" CLIP INFO ("<<filename<<"):"<<std::endl;
    for(i=0;i<INFOT_MAX;i++)
	if(id[i])
	    mpxp_hint<<"   "<<info_names[i]<<": "<<id[i]<<std::endl;
    return 0;
}

const char* Demuxer_Info::get(unsigned opt) const {
    if(!opt || opt > INFOT_MAX) return NULL;
    return id[opt-1];
}
} // namespaec mpxp;
