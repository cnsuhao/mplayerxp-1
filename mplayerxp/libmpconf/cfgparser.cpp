#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * command line and config file parser
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 *
 * subconfig support by alex
 */
#include <algorithm>
#include <limits>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mplayerxp.h"
#include "mpxp_help.h"
#include "cfgparser.h"
#include "libplaytree2/playtree.h"
#include "parser_msg.h"
#include "osdep/get_path.h"

namespace	usr {
static const int MAX_RECURSION_DEPTH=8;

typedef int (*cfg_func_arg_param_t)(const mpxp_option_t *,const std::string& ,const std::string& );
typedef int (*cfg_func_param_t)(const mpxp_option_t *,const std::string& );
typedef int (*cfg_func_t)(const mpxp_option_t *);


M_Config::M_Config(PlayTree* _pt,libinput_t& _libinput)
	:pt(_pt),libinput(_libinput)
{
    set_global(); // We always start with global options
}
M_Config::~M_Config() {}

void	M_Config::set_global() { flags |= M_Config::Global; }
void	M_Config::unset_global() { flags &= ~M_Config::Global; }
int	M_Config::is_global() const { return flags & M_Config::Global; }
void	M_Config::set_running() { flags |= M_Config::Running; }
void	M_Config::unset_running() { flags &= ~M_Config::Running; }
int	M_Config::is_running() const { return flags & M_Config::Running; }

MPXP_Rc M_Config::init_conf(mode_e mode)
{
    parser_mode = mode;
    return MPXP_Ok;
}

int M_Config::is_entry_option(const std::string& opt,const std::string& param) {
    PlayTree* entry = NULL;

    std::string lopt=opt;
    std::transform(lopt.begin(),lopt.end(),lopt.begin(), ::tolower);
    if(lopt=="playlist") { // We handle playlist here
	if(param.empty()) return ERR_MISSING_PARAM;
	entry = PlayTree::parse_playlist_file(libinput,param);
	if(!entry) {
	    mpxp_err<<"Playlist parsing failed: "<<param<<std::endl;
	    return 1;
	}
    }

    if(entry) {
	if(last_entry)	last_entry->append_entry(entry);
	else		pt->set_child(entry);
	last_entry = entry;
	if(parser_mode == M_Config::CmdLine) unset_global();
	return 1;
    }
    return 0;
}

MPXP_Rc M_Config::cfg_include(const std::string& filename){
    return parse_config_file(filename);
}

int M_Config::cfg_inc_int(int value){ return ++value; }

int M_Config::read_option(const std::vector<const mpxp_option_t*>& conf_list,const std::string& opt,const std::string& param) {
	int i=0;
	long tmp_int;
	double tmp_float;
	int ret = -1;
	char *endptr;
	const mpxp_option_t* conf=NULL;

	mpxp_dbg3<<"read_option: opt='"<<opt<<"' param='"<<param<<"'"<<std::endl;
	std::string lopt=opt;
	std::transform(lopt.begin(),lopt.end(),lopt.begin(), ::tolower);
	size_t nconf,sz = conf_list.size();
	for(nconf = 0 ; nconf<sz ; nconf++) {
	  conf = conf_list[nconf];
		for (i = 0; conf[i].name != NULL; i++) {
		    std::string lname=conf[i].name;
		    std::transform(lname.begin(),lname.end(),lname.begin(), ::tolower);
			int namelength;
			/* allow 'aa*' in config.name */
			namelength=strlen(conf[i].name);
			if ( (conf[i].name[namelength-1]=='*') &&
				!memcmp(opt.c_str(), conf[i].name, namelength-1))
			  goto option_found;
			if (lopt==lname) goto option_found;
		}
	}
	mpxp_err<<"read_option: invalid option: "<<opt<<std::endl;
	ret = ERR_NOT_AN_OPTION;
	goto out;
	option_found :
	mpxp_dbg3<<"read_option: name='"<<conf[i].name<<"' type="<<conf[i].type<<std::endl;

	if (conf[i].flags & CONF_NOCFG && parser_mode == M_Config::File) {
		mpxp_err<<"this option can only be used on command line:"<<opt<<std::endl;
		ret = ERR_NOT_AN_OPTION;
		goto out;
	}
	if (conf[i].flags & CONF_NOCMD && parser_mode == M_Config::CmdLine) {
		mpxp_err<<"read_option: this option can only be used in config file:"<<opt<<std::endl;
		ret = ERR_NOT_AN_OPTION;
		goto out;
	}
	ret = is_entry_option(opt,param);
	if(ret != 0)
	  return ret;
	else
	  ret = -1;
	switch (conf[i].type) {
		case CONF_TYPE_FLAG:
			/* flags need a parameter in config file */
			if (parser_mode == M_Config::File) {
			    std::string lparm=param;
			    std::transform(lparm.begin(),lparm.end(),lparm.begin(), ::tolower);
				if (lparm=="yes" ||	/* any other language? */
				    lparm=="ja" ||
				    lparm=="si" ||
				    lparm=="igen" ||
				    lparm=="y" ||
				    lparm=="j" ||
				    lparm=="i" ||
				    lparm=="1")
					*((int *) conf[i].p) = conf[i].max;
				else if (lparm=="no" ||
				    lparm=="nein" ||
				    lparm=="nicht" ||
				    lparm=="nem" ||
				    lparm=="n" ||
				    lparm=="0")
					*((int *) conf[i].p) = conf[i].min;
				else {
					mpxp_err<<"read_option: invalid parameter for flag: "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}
				ret = 1;
			} else {	/* parser_mode == COMMAND_LINE */
				*((int *) conf[i].p) = conf[i].max;
				mpxp_dbg3<<"read_option: assigning "<<conf[i].name<<"="<<conf[i].max<<" as flag value"<<std::endl;
				ret = 0;
			}
			break;
		case CONF_TYPE_INT:
			if (param.empty())
				goto err_missing_param;

			tmp_int = ::strtol(param.c_str(), &endptr, 0);
			if (*endptr) {
				mpxp_err<<"read_option: parameter must be an integer: "<<param<<std::endl;
				ret = ERR_OUT_OF_RANGE;
				goto out;
			}

			if (conf[i].flags & CONF_MIN)
				if (tmp_int < conf[i].min) {
					mpxp_err<<"read_option: parameter must be >= "<<(int) conf[i].min<<": "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (tmp_int > conf[i].max) {
					mpxp_err<<"read_option: parameter must be <= "<<(int) conf[i].max<<": "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((int *) conf[i].p) = tmp_int;
			mpxp_dbg3<<"read_option: assigning "<<conf[i].name<<"="<<tmp_int<<" as int value"<<std::endl;
			ret = 1;
			break;
		case CONF_TYPE_FLOAT:
			if (param.empty())
				goto err_missing_param;

			tmp_float = ::strtod(param.c_str(), &endptr);

			if ((*endptr == ':') || (*endptr == '/'))
				tmp_float /= ::strtod(endptr+1, &endptr);

			if (*endptr) {
				mpxp_err<<"read_option: parameter must be a floating point number or a ratio (numerator[:/]denominator): "<<param<<std::endl;
				ret = ERR_MISSING_PARAM;
				goto out;
			}

			if (conf[i].flags & CONF_MIN)
				if (tmp_float < conf[i].min) {
					mpxp_err<<"read_option: parameter must be >= "<<(float)conf[i].min<<": "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (tmp_float > conf[i].max) {
					mpxp_err<<"read_option: parameter must be <= "<<(float)conf[i].max<<": "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((float *) conf[i].p) = tmp_float;
			mpxp_dbg3<<"read_option: assigning "<<conf[i].name<<"="<<tmp_float<<" as float value"<<std::endl;
			ret = 1;
			break;
		case CONF_TYPE_STRING:
			if (param.empty())
				goto err_missing_param;

			if (conf[i].flags & CONF_MIN)
				if (param.length() < conf[i].min) {
					mpxp_err<<"read_option: parameter must be >= "<<(int) conf[i].min<<" chars: "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (param.length() > conf[i].max) {
					mpxp_err<<"read_option: parameter must be <= "<<(int) conf[i].max<<" chars: "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}
			*((char **) conf[i].p) = mp_strdup(param.c_str());
			mpxp_dbg3<<"read_option: assigning "<<conf[i].name<<"="<<param<<" as string value"<<std::endl;
			ret = 1;
			break;
		case CONF_TYPE_INC:
			*((int *) conf[i].p) = cfg_inc_int(*((int *) conf[i].p));
			ret = 0;
			break;
		case CONF_TYPE_INCLUDE:
			if (param.empty())
				goto err_missing_param;
			if (cfg_include(param) < 0) {
				ret = ERR_FUNC_ERR;
				goto out;
			}
			ret = 1;
			break;
		case CONF_TYPE_PRINT:
			mpxp_info<<(char *)conf[i].p;
			throw soft_exit_exception(MSGTR_Exit_quit);
		default:
			mpxp_err<<"read_option: Unknown config type specified in conf-mplayerxp.h!"<<std::endl;
			break;
	}
out:
	if(ret >= 0 && ! is_running() && !is_global() && ! (conf[i].flags & CONF_GLOBAL) && conf[i].type != CONF_TYPE_SUBCONFIG ) {
	  PlayTree* dest = last_entry ? last_entry : last_parent;

	  if(ret == 0)		dest->set_param(opt,"");
	  else if(ret > 0)	dest->set_param(opt,param);
	}
	return ret;
err_missing_param:
	mpxp_err<<"read_option: missing parameter for option: "<<opt<<std::endl;
	ret = ERR_MISSING_PARAM;
	goto out;
}

int M_Config::set_option(const std::string& _opt,const std::string& param) {
    size_t e;
    std::vector<const mpxp_option_t*> clist=opt_list;
    std::string opt=_opt;
    std::string s;
    mpxp_dbg2<<"Setting option "<<opt<<"="<<param<<std::endl;
    clist = opt_list;

    e=opt.find('.');
    if(e!=std::string::npos) {
	int ret;
	flags_e flg;
	const mpxp_option_t *subconf=NULL;
	mpxp_dbg2<<"Parsing "<<opt<<" as subconfig"<<std::endl;
	do {
	    if((e = opt.find('.'))==std::string::npos) break;
	    s=opt.substr(0,e);
	    mpxp_dbg2<<"Treat "<<s<<" as subconfig name"<<std::endl;
	    subconf = find_option(clist,s);
	    clist.clear();
	    if(!subconf) return ERR_NO_SUBCONF;
	    if(subconf->type!=CONF_TYPE_SUBCONFIG) return ERR_NO_SUBCONF;
	    clist.push_back(reinterpret_cast<const mpxp_option_t*>(subconf->p));
	    opt = opt.substr(e+1);
	    mpxp_dbg2<<"switching next subconf="<<subconf->name<<std::endl;
	}while(1);
	flg=flags;
	set_global();
	ret=read_option(clist,opt,param);
	flags=flg;
	return ret;
    }

    e = opt.find(':');
    if(e!=std::string::npos && e<(opt.length()-1)) {
	int ret;
	const mpxp_option_t* m_opt;
	std::vector<const mpxp_option_t*> _opt_list;
	s=opt.substr(0,e);
	m_opt=(const mpxp_option_t*)get_option_ptr(s);
	if(!m_opt) {
	    mpxp_err<<"m_config_set_option "<<opt<<"="<<param<<" : no "<<s<<" subconfig"<<std::endl;
	    return ERR_NOT_AN_OPTION;
	}
	_opt_list.push_back(m_opt);
	s=opt.substr(e+1);
	ret = read_option(_opt_list,s,param);
	return ret;
    }
    return read_option(opt_list,opt,param);
}

static void PRINT_LINENUM(const std::string& conffile,int line_num) { mpxp_err<<conffile<<"("<<line_num<<")"<<std::endl; }
static const int MAX_LINE_LEN=1000;
static const int MAX_OPT_LEN=100;
static const int MAX_PARAM_LEN=100;
MPXP_Rc M_Config::parse_config_file(const std::string& conffile)
{
    std::ifstream fs;
    char *line;
    char opt[MAX_OPT_LEN + 1];
    char param[MAX_PARAM_LEN + 1];
    char c;		/* for the "" and '' check */
    int tmp;
    int line_num = 0;
    int line_pos;	/* line pos */
    int opt_pos;	/* opt pos */
    int param_pos;	/* param pos */
    MPXP_Rc ret = MPXP_Ok;
    int errors = 0;

    if (++recursion_depth > 1) mpxp_info<<"Reading config file: "<<conffile<<std::endl;

    if (recursion_depth > MAX_RECURSION_DEPTH) {
	mpxp_fatal<<": too deep 'include'. check your configfiles"<<std::endl;
	ret = MPXP_False;
	goto out;
    }

    if (init_conf(M_Config::File) == -1) {
	ret = MPXP_False;
	goto out;
    }

    if ((line = new char [MAX_LINE_LEN + 1]) == NULL) {
	mpxp_fatal<<"can't get memory for 'line': "<<strerror(errno)<<std::endl;
	ret = MPXP_False;
	goto out;
    }

    fs.open(conffile.c_str(),std::ios_base::in);
    if (!fs.is_open()) {
	if (recursion_depth > 1) mpxp_err<<": "<<::strerror(errno)<<std::endl;
	delete line;
	ret = MPXP_Ok;
	goto out;
    }
    if (recursion_depth > 1) mpxp_fatal<<std::endl;

    while (!fs.eof()) {
	fs.getline(line, MAX_LINE_LEN);
	if (errors >= 16) {
	    mpxp_fatal<<"too many errors"<<std::endl;
	    goto out;
	}

	line_num++;
	line_pos = 0;

	/* skip whitespaces */
	while (::isspace(line[line_pos])) ++line_pos;

	/* EOL / comment */
	if (line[line_pos] == '\0' || line[line_pos] == '#') continue;

	/* read option. */
	for (opt_pos = 0; ::isprint(line[line_pos]) &&
		line[line_pos] != ' ' &&
		line[line_pos] != '#' &&
		line[line_pos] != '='; /* NOTHING */) {
	    opt[opt_pos++] = line[line_pos++];
	    if (opt_pos >= MAX_OPT_LEN) {
		PRINT_LINENUM(conffile,line_num);
		mpxp_err<<"too long option"<<std::endl;
		errors++;
		ret = MPXP_False;
		goto nextline;
	    }
	}
	if (opt_pos == 0) {
	    PRINT_LINENUM(conffile,line_num);
	    mpxp_err<<"parse error"<<std::endl;
	    ret = MPXP_False;
	    errors++;
	    continue;
	}
	opt[opt_pos] = '\0';

	/* skip whitespaces */
	while (::isspace(line[line_pos])) ++line_pos;

	/* check '=' */
	if (line[line_pos++] != '=') {
	    PRINT_LINENUM(conffile,line_num);
	    mpxp_err<<"option without parameter"<<std::endl;
	    ret = MPXP_False;
	    errors++;
	    continue;
	}

	/* whitespaces... */
	while (::isspace(line[line_pos])) ++line_pos;

	/* read the parameter */
	if (line[line_pos] == '"' || line[line_pos] == '\'') {
	    c = line[line_pos];
	    ++line_pos;
	    for (param_pos = 0; line[line_pos] != c; /* NOTHING */) {
		param[param_pos++] = line[line_pos++];
		if (param_pos >= MAX_PARAM_LEN) {
		    PRINT_LINENUM(conffile,line_num);
		    mpxp_err<<"too long parameter"<<std::endl;
		    ret = MPXP_False;
		    errors++;
		    goto nextline;
		}
	    }
	    line_pos++;	/* skip the closing " or ' */
	} else {
	    for (param_pos = 0; ::isprint(line[line_pos]) && !::isspace(line[line_pos])
			&& line[line_pos] != '#'; /* NOTHING */) {
		param[param_pos++] = line[line_pos++];
		if (param_pos >= MAX_PARAM_LEN) {
		    PRINT_LINENUM(conffile,line_num);
		    mpxp_err<<"too long parameter"<<std::endl;
		    ret = MPXP_False;
		    errors++;
		    goto nextline;
		}
	    }
	}
	param[param_pos] = '\0';

	/* did we read a parameter? */
	if (param_pos == 0) {
	    PRINT_LINENUM(conffile,line_num);
	    mpxp_err<<"option without parameter"<<std::endl;
	    ret = MPXP_False;
	    errors++;
	    continue;
	}

	/* now, check if we have some more chars on the line */
	/* whitespace... */
	while (::isspace(line[line_pos])) ++line_pos;

	/* EOL / comment */
	if (line[line_pos] != '\0' && line[line_pos] != '#') {
	    PRINT_LINENUM(conffile,line_num);
	    mpxp_err<<"extra characters on line: "<<line+line_pos<<std::endl;
	    ret = MPXP_False;
	}

	tmp = set_option(opt, param);
	switch (tmp) {
	    case ERR_NOT_AN_OPTION:
	    case ERR_MISSING_PARAM:
	    case ERR_OUT_OF_RANGE:
	    case ERR_NO_SUBCONF:
	    case ERR_FUNC_ERR:
		PRINT_LINENUM(conffile,line_num);
		mpxp_err<<opt<<std::endl;
		ret = MPXP_False;
		errors++;
		continue;
		/* break */
	}
nextline:
	;
    }

    delete line;
    fs.close();
out:
    --recursion_depth;
    return ret;
}

MPXP_Rc M_Config::parse_command_line(const std::vector<std::string>& argv,const std::map<std::string,std::string>& envm) {
    size_t i,siz=argv.size();
    int tmp;
    std::string opt;
    int no_more_opts = 0;

    if (init_conf(M_Config::CmdLine) == -1) return MPXP_False;
    if(last_parent == NULL) last_parent = pt;
    /* in order to work recursion detection properly in parse_config_file */
    ++recursion_depth;

    for (i = 1; i < siz; i++) {
	 //next:
	opt = argv[i];
	if(opt=="--help") {
	    show_help();
	    throw soft_exit_exception(MSGTR_Exit_quit);
	}
	if(opt=="--long-help") {
	    show_long_help(*this,envm);
	    throw soft_exit_exception(MSGTR_Exit_quit);
	}
	/* check for -- (no more options id.) except --help! */
	if (opt[0] == '-' && opt[1] == '-') {
	    no_more_opts = 1;
	    if (i+1 >= siz) {
		mpxp_err<<"You added '--' but no filenames presented!"<<std::endl;
		goto err_out;
	    }
	    continue;
	}
	if(opt[0] == '{' && opt[1] == '\0') {
	    PlayTree* entry = new(zeromem) PlayTree;
	    unset_global();
	    if(last_entry == NULL) {
		last_parent->set_child(entry);
	    } else {
		last_entry->append_entry(entry);
		last_entry = NULL;
	    }
	    last_parent = entry;
	    continue;
	}

	if(opt[0] == '}' && opt[1] == '\0') {
	    if( ! last_parent || ! last_parent->get_parent()) {
		mpxp_err<<"too much }-"<<std::endl;
		goto err_out;
	    }
	    last_entry = last_parent;
	    last_parent = last_entry->get_parent();
	    continue;
	}

	if (no_more_opts == 0 && opt[0] == '-' && opt.length()>1) /* option */ {
	    /* remove leading '-' */
	    size_t pos;
	    std::string item,parm;
	    pos=1;

	    mpxp_dbg2<<"this_option: "<<opt<<std::endl;
	    parm = ((i+1)<siz)?argv[i+1]:"";
	    item=opt.substr(pos);
	    pos = item.find('=');
	    if(pos!=std::string::npos) {
		parm=item.substr(pos+1);
		item=item.substr(0,pos);
	    }
	    tmp = set_option(item, parm);
	    if(!tmp && pos!=std::string::npos) {
		mpxp_err<<"Option '"<<item<<"' doesn't require arguments"<<std::endl;
		goto err_out;
	    }

	    switch (tmp) {
		case ERR_NOT_AN_OPTION:
		case ERR_MISSING_PARAM:
		case ERR_OUT_OF_RANGE:
		case ERR_NO_SUBCONF:
		case ERR_FUNC_ERR:
		    mpxp_err<<"Error '"<<
				(tmp==ERR_NOT_AN_OPTION?"no-option":
				 tmp==ERR_MISSING_PARAM?"missing-param":
				 tmp==ERR_OUT_OF_RANGE?"out-of-range":
				 tmp==ERR_NO_SUBCONF?"no-subconfig":"func-error")
			    <<"' while parsing option: '"<<opt<<"'"<<std::endl;
		    goto err_out;
		default:
		    if(tmp) i++;
		    break;
	    }
	} else /* filename */ {
	    PlayTree* entry = new(zeromem) PlayTree;
	    mpxp_dbg2<<"Adding file "<<argv[i]<<std::endl;
	    entry->add_file(argv[i]);
	    if(argv[i]=="-") set_option("use-stdin","");
	    /* opt is not an option -> treat it as a filename */
	    unset_global(); // We start entry specific options
	    if(last_entry == NULL) last_parent->set_child(entry);
	    else last_entry->append_entry(entry);
	    last_entry = entry;
	}
    }

    --recursion_depth;
    if(last_parent != pt) mpxp_err<<"Missing }- ?"<<std::endl;
    unset_global();
    set_running();
    return MPXP_Ok;
err_out:
    --recursion_depth;
    mpxp_err<<"command line: "<<argv[i]<<std::endl;
    return MPXP_False;
}

MPXP_Rc M_Config::register_options(const mpxp_option_t *args) {
    opt_list.push_back(args);
    return MPXP_Ok;
}

const mpxp_option_t* M_Config::find_option(const std::vector<const mpxp_option_t*>& list,const std::string& name) const {
    unsigned i;
    const mpxp_option_t *conf;
    if(!list.empty()) {
	std::string ln=name;
	std::transform(ln.begin(),ln.end(),ln.begin(), ::tolower);
	size_t j,sz=list.size();
	for(j = 0; j<sz ; j++) {
	    conf = list[j];
	    for(i=0; conf[i].name != NULL; i++) {
		std::string lcn=conf[i].name;
		std::transform(lcn.begin(),lcn.end(),lcn.begin(), ::tolower);
		if(lcn==ln) return &conf[i];
	    }
	}
    }
    return NULL;
}

const mpxp_option_t* M_Config::get_option(const std::string& arg) const {
    size_t e;

    e = arg.find(':');
    if(e!=std::string::npos) {
	std::vector<const mpxp_option_t*> cl;
	const mpxp_option_t* opt;
	std::string s;
	s=arg.substr(0,e);
	opt = get_option(s);
	cl.push_back(opt);
	return find_option(cl,arg);
    }
    return find_option(opt_list,arg);
}

any_t* M_Config::get_option_ptr(const std::string& arg) const {
    const mpxp_option_t* conf;

    conf = get_option(arg);
    if(!conf) return NULL;
    return conf->p;
}

int M_Config::get_int (const std::string& arg,int& err_ret) const {
    int *ret;

    ret = (int*)get_option_ptr(arg);
    err_ret = 0;
    if(!ret) {
	err_ret = 1;
	return -1;
    }
    return *ret;
}

float M_Config::get_float (const std::string& arg,int& err_ret) const {
    float *ret;

    ret = (float*)get_option_ptr(arg);
    err_ret = 0;
    if(!ret) {
	err_ret = 1;
	return -1;
    }
    return *ret;
}

inline int AS_INT(const mpxp_option_t* c) { return *((int*)c->p); }
inline void AS_INT(const mpxp_option_t* c,int val) { *((int*)c->p)=val; }

int M_Config::set_int(const std::string& arg,int val) {
    const mpxp_option_t* opt;

    opt = get_option(arg);

    if(!opt || opt->type != CONF_TYPE_INT) return ERR_NOT_AN_OPTION;
    if(opt->flags & CONF_MIN && val < opt->min) return ERR_OUT_OF_RANGE;
    if(opt->flags & CONF_MAX && val > opt->max) return ERR_OUT_OF_RANGE;

    AS_INT(opt,val);

    return 1;
}

int M_Config::set_float(const std::string& arg,float val) {
    const mpxp_option_t* opt;

    opt = get_option(arg);

    if(!opt || opt->type != CONF_TYPE_FLOAT) return ERR_NOT_AN_OPTION;
    if(opt->flags & CONF_MIN && val < opt->min) return ERR_OUT_OF_RANGE;
    if(opt->flags & CONF_MAX && val > opt->max) return ERR_OUT_OF_RANGE;

    *((float*)opt->p) = val;
    return 1;
}


int M_Config::switch_flag(const std::string& opt) {
    const mpxp_option_t *conf;

    conf = get_option(opt);
    if(!conf || conf->type != CONF_TYPE_FLAG) return 0;
    if( AS_INT(conf) == conf->min) AS_INT(conf,conf->max);
    else if(AS_INT(conf) == conf->max) AS_INT(conf,conf->min);
    else return 0;

    return 1;
}

int M_Config::set_flag(const std::string& opt, int state) {
    const mpxp_option_t *conf;

    conf = get_option(opt);
    if(!conf || conf->type != CONF_TYPE_FLAG) return 0;
    if(state) AS_INT(conf,conf->max);
    else AS_INT(conf,conf->min);
    return 1;
}

int M_Config::get_flag(const std::string& opt) const {
    const mpxp_option_t* conf = get_option(opt);

    if(!conf || conf->type != CONF_TYPE_FLAG) return -1;
    if(AS_INT(conf) == conf->max) return 1;
    else if(AS_INT(conf) == conf->min) return 0;
    return -1;
}

void M_Config::__show_options(unsigned ntabs,const std::string& pfx,const mpxp_option_t* opts) const {
    unsigned i,n;
    i=0;
    while(opts[i].name) {
	if(opts[i].type==CONF_TYPE_SUBCONFIG  && opts[i].p) {
	    std::string newpfx;
	    unsigned pfxlen;
	    for(n=0;n<ntabs;n++) mpxp_info<<" ";
	    mpxp_info<<opts[i].help<<":"<<std::endl;
	    pfxlen=strlen(opts[i].name)+1;
	    if(!pfx.empty())	{ pfxlen+=pfx.length(); newpfx=pfx; }
	    else		newpfx="";
	    newpfx+=opts[i].name;
	    newpfx+=".";
	    __show_options(ntabs+2,newpfx,(const mpxp_option_t *)opts[i].p);
	} else if(opts[i].type<=CONF_TYPE_PRINT) {
	    std::ostringstream os;
	    for(n=0;n<ntabs;n++) mpxp_info<<" ";
	    if(!pfx.empty())	os<<std::left<<pfx;
	    else		os<<" ";
	    os<<opts[i].name;
	    mpxp_info<<std::left<<std::setw(25)<<os.str()<<" "<<opts[i].help;
	    if((opts[i].flags&CONF_NOCFG)==0) {
	    mpxp_info<<" {"<<
		    (opts[i].type==CONF_TYPE_FLAG?"flg":
		    opts[i].type==CONF_TYPE_INT?"int":
		    opts[i].type==CONF_TYPE_FLOAT?"flt":
		    opts[i].type==CONF_TYPE_STRING?"str":"")<<"=";
	    switch(opts[i].type) {
	    case CONF_TYPE_FLAG: {
		int defv = (*((int*)(opts[i].p)))?1:0;
		int max  = opts[i].max ? 1:0;
		int res = !(defv^max);
		mpxp_info<<(res?"ON":"OFF");
	    }
	    break;
	    case CONF_TYPE_STRING: {
		const char **defv = (const char**)(opts[i].p);
		if(defv && *defv) mpxp_info<<"\""<<*defv<<"\"";
	    }
	    break;
	    case CONF_TYPE_INT: {
		int defv = *((int*)(opts[i].p));
		mpxp_info<<defv;
		if((opts[i].flags&CONF_RANGE)==CONF_RANGE) {
		    mpxp_info<<" ["<<(int)opts[i].min<<"..."<<(int)opts[i].max<<"]";
		}
		else
		if((opts[i].flags&CONF_MIN)==CONF_MIN) {
		    mpxp_info<<" <min="<<(int)opts[i].min<<">";
		}
		else
		if((opts[i].flags&CONF_MAX)==CONF_MAX) {
		    mpxp_info<<" <max="<<(int)opts[i].max<<">";
		}
	    }
	    break;
	    case CONF_TYPE_FLOAT: {
		float defv = *((float*)(opts[i].p));
		mpxp_info<<defv;
		if((opts[i].flags&CONF_RANGE)==CONF_RANGE) {
		    mpxp_info<<" ["<<(float)opts[i].min<<"..."<<(float)opts[i].max<<"]";
		}
		else
		if((opts[i].flags&CONF_MIN)==CONF_MIN) {
		    mpxp_info<<" <min="<<(float)opts[i].min<<">";
		}
		else
		if((opts[i].flags&CONF_MAX)==CONF_MAX) {
		    mpxp_info<<" <max="<<(float)opts[i].max<<">";
		}
	    }
	    break;
	    default:
	    break;
	    }
	    mpxp_info<<"}";
	    }
	    mpxp_info<<std::endl;
	}
	i++;
    };
}

void M_Config::show_options() const {
    size_t j,sz=opt_list.size();
    const mpxp_option_t *opts;
    j=0;
    mpxp_info<<"List of available command-line options:"<<std::endl;
    for(j=0;j<sz;j++) {
	opts=opt_list[j];
	__show_options(2,"",opts);
    };
}

static const char* default_config=
"# Write your default config options here!\n"
"\n"
//"nosound=nein"
"\n";

void M_Config::parse_cfgfiles(const std::map<std::string,std::string>& envm)
{
    std::string conffile;
    int conffile_fd;
    conffile = get_path(envm);
    if (conffile.empty()) mpxp_warn<<MSGTR_NoHomeDir<<std::endl;
    else {
	::mkdir(conffile.c_str(), 0777);
	conffile = get_path(envm,"config");
	if (conffile.empty()) {
	    mpxp_err<<MSGTR_GetpathProblem<<std::endl;
	    conffile="config";
	}
	if ((conffile_fd = ::open(conffile.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0666)) != -1) {
	    mpxp_info<<MSGTR_CreatingCfgFile<<": "<<conffile<<std::endl;
	    ::write(conffile_fd, default_config, strlen(default_config));
	    ::close(conffile_fd);
	}
	if (parse_config_file(conffile) != MPXP_Ok) throw std::runtime_error("Error in config file");
    }
}

} // namespace	usr
