#ifndef __MPXP_GET_PATH
#define __MPXP_GET_PATH 1
#include <string>
#include <map>
namespace	usr {
    std::string get_path(const std::map<std::string,std::string>& envm,const std::string& filename="");
}
#endif

