#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "osdep_msg.h"
namespace mpxp {
std::string get_path(const std::string& filename){
    char *homedir;
    std::string rs;
    std::string config_dir = std::string("/.")+PROGNAME;

    if ((homedir = ::getenv("HOME")) == NULL) return "";
    rs=std::string(homedir)+config_dir;
    if (!filename.empty()) rs+="/"+filename;
    mpxp_v<<"get_path('"<<filename<<"') -> "<<rs<<std::endl;
    return rs;
}
}// namespace mpxp
