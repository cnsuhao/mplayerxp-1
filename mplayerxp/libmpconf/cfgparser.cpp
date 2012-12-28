#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * command line and config file parser
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 *
 * subconfig support by alex
 */
#include <algorithm>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "mplayerxp.h"
#include "cfgparser.h"
#include "libplaytree/playtree.h"
#include "parser_msg.h"

namespace mpxp {
enum {
    COMMAND_LINE=0,
    CONFIG_FILE=1,

    CONFIG_GLOBAL=(1<<0),
    CONFIG_RUNNING=(1<<1),

    MAX_RECURSION_DEPTH=8
};

inline void SET_GLOBAL(m_config_t& c) { c.flags |= CONFIG_GLOBAL; }
inline void UNSET_GLOBAL(m_config_t& c) { c.flags &= (!CONFIG_GLOBAL); }
inline int  IS_GLOBAL(const m_config_t& c) { return c.flags & CONFIG_GLOBAL; }
inline void SET_RUNNING(m_config_t& c) { c.flags |= CONFIG_RUNNING; }
inline int  IS_RUNNING(const m_config_t& c) { return c.flags & CONFIG_RUNNING; }

typedef int (*cfg_func_arg_param_t)(const mpxp_option_t *,const std::string& ,const std::string& );
typedef int (*cfg_func_param_t)(const mpxp_option_t *,const std::string& );
typedef int (*cfg_func_t)(const mpxp_option_t *);

static void
m_config_save_option(m_config_t& config,const mpxp_option_t* conf,const std::string& opt,const std::string& param) {
  config_save_t* save;
  int sl=0;

  switch(conf->type) {
  case CONF_TYPE_PRINT :
  case CONF_TYPE_SUBCONFIG :
    return;
  default :
    ;
  }

  mpxp_dbg2<<"Saving option "<<opt<<std::endl;

  save = config.config_stack[config.cs_level];

  if(save) {
    std::string lopt=opt;
    std::transform(lopt.begin(),lopt.end(),lopt.begin(), ::tolower);
    for(sl = 0; save[sl].opt != NULL; sl++){
      // Check to not save the same arg two times
      if(save[sl].opt == conf && (save[sl].opt_name == NULL || lopt==save[sl].opt_name))
	break;
    }
    if(save[sl].opt)
      return;
  }

  save = (config_save_t*)mp_realloc(save,(sl+2)*sizeof(config_save_t));
  if(save == NULL) {
    mpxp_err<<"Can't allocate "<<((sl+2)*sizeof(config_save_t))<<" bytes of memory : "<<strerror(errno)<<std::endl;
    return;
  }
  memset(&save[sl],0,2*sizeof(config_save_t));
  save[sl].opt = conf;

  switch(conf->type) {
  case CONF_TYPE_FLAG :
  case CONF_TYPE_INC :
  case CONF_TYPE_INT :
    save[sl].param.as_int = *((int*)conf->p);
    break;
  case CONF_TYPE_FLOAT :
    save[sl].param.as_float = *((float*)conf->p);
    break;
  case CONF_TYPE_STRING :
    save[sl].param.as_pointer = *((char**)conf->p);
    break;
  case CONF_TYPE_INCLUDE :
    if(!param.empty())
      save->param.as_pointer = mp_strdup(param.c_str());
  default :
    mpxp_err<<"Should never append in m_config_save_option : conf->type="<<conf->type<<std::endl;
  }

  config.config_stack[config.cs_level] = save;
}

static int m_config_revert_option(m_config_t& config, config_save_t* save) {
  const char* arg = NULL;
  config_save_t* iter=NULL;
  int i=-1;

  arg = save->opt_name ? save->opt_name : save->opt->name;
  mpxp_dbg2<<"Reverting option: "<<arg<<std::endl;

  switch(save->opt->type) {
  case CONF_TYPE_FLAG:
  case CONF_TYPE_INC :
  case CONF_TYPE_INT :
    *((int*)save->opt->p) = save->param.as_int;
    break;
  case CONF_TYPE_FLOAT :
    *((float*)save->opt->p) = save->param.as_float;
    break;
  case CONF_TYPE_STRING :
    *((char**)save->opt->p) = reinterpret_cast<char*>(save->param.as_pointer);
    break;
  case CONF_TYPE_INCLUDE :
    if(config.cs_level > 0) {
      for(i = config.cs_level - 1 ; i >= 0 ; i--){
	if(config.config_stack[i] == NULL) continue;
	for(iter = config.config_stack[i]; iter != NULL && iter->opt != NULL ; iter++) {
	  if(iter->opt == save->opt &&
	     ((save->param.as_pointer == NULL || iter->param.as_pointer == NULL) || strcasecmp((const char *)save->param.as_pointer,(const char *)iter->param.as_pointer) == 0) &&
	     (save->opt_name == NULL ||
	      (iter->opt_name && strcasecmp(save->opt_name,iter->opt_name)))) break;
	}
      }
    }
    delete save->param.as_pointer;
    if(save->opt_name) delete save->opt_name;
    save->param.as_pointer = NULL;
    save->opt_name = reinterpret_cast<char*>(save->param.as_pointer);
    if(i < 0) break;
    arg = iter->opt_name ? iter->opt_name : iter->opt->name;
    switch(iter->opt->type) {
    case CONF_TYPE_INCLUDE :
      if (iter->param.as_pointer == NULL) {
	mpxp_err<<"We lost param for option "<<iter->opt->name<<"?"<<std::endl;
	return -1;
      }
      if ((((cfg_func_param_t) iter->opt->p)(iter->opt, (char*)iter->param.as_pointer)) < 0)
	return -1;
      break;
    }
    break;
  default :
    mpxp_err<<"Why do we reverse this : name="<<save->opt->name<<" type="<<save->opt->type<<" ?"<<std::endl;
  }

  return 1;
}

void m_config_push(m_config_t& config) {

  config.cs_level++;
  config.config_stack = (config_save_t**)mp_realloc(config.config_stack ,sizeof(config_save_t*)*(config.cs_level+1));
  if(config.config_stack == NULL) {
    mpxp_err<<"Can't allocate "<<(sizeof(config_save_t*)*(config.cs_level+1))<<" bytes of memory : "<<strerror(errno)<<std::endl;
    config.cs_level = -1;
    return;
  }
  config.config_stack[config.cs_level] = NULL;
  mpxp_dbg2<<"Config pushed level="<<config.cs_level<<std::endl;
}

int m_config_pop(m_config_t& config) {
  int i,ret= 1;
  config_save_t* cs;

  if(config.config_stack[config.cs_level] != NULL) {
    cs = config.config_stack[config.cs_level];
    for(i=0; cs[i].opt != NULL ; i++ ) {
      if (m_config_revert_option(config,&cs[i]) < 0)
	ret = -1;
    }
    delete config.config_stack[config.cs_level];
  }
  config.config_stack = (config_save_t**)mp_realloc(config.config_stack ,sizeof(config_save_t*)*config.cs_level);
  config.cs_level--;
  if(config.cs_level > 0 && config.config_stack == NULL) {
    mpxp_err<<"Can't allocate memory"<<std::endl;
    config.cs_level = -1;
    return -1;
  }
  mpxp_dbg2<<"Config poped level="<<config.cs_level<<std::endl;
  return ret;
}

m_config_t& m_config_new(PlayTree* pt,libinput_t&libinput) {
  m_config_t& config = *new(zeromem) m_config_t(libinput);
  config.config_stack = (config_save_t**)mp_calloc(1,sizeof(config_save_t*));
  SET_GLOBAL(config); // We always start with global options
  config.pt = pt;
  return config;
}

void m_config_free(m_config_t* config) {
  delete config->config_stack;
  delete config;
}

static int init_conf(m_config_t& config, int mode)
{
    config.parser_mode = mode;
    return 1;
}

static int config_is_entry_option(m_config_t& config,const std::string& opt,const std::string& param) {
    PlayTree* entry = NULL;

    std::string lopt=opt;
    std::transform(lopt.begin(),lopt.end(),lopt.begin(), ::tolower);
    if(lopt=="playlist") { // We handle playlist here
	if(param.empty()) return ERR_MISSING_PARAM;
	entry = PlayTree::parse_playlist_file(config.libinput,param);
	if(!entry) {
	    mpxp_err<<"Playlist parsing failed: "<<param<<std::endl;
	    return 1;
	}
    }

    if(entry) {
	if(config.last_entry)	config.last_entry->append_entry(entry);
	else			config.pt->set_child(entry);
	config.last_entry = entry;
	if(config.parser_mode == COMMAND_LINE) UNSET_GLOBAL(config);
	return 1;
    }
    return 0;
}

static MPXP_Rc cfg_include(m_config_t& conf,const std::string& filename){
    return m_config_parse_config_file(conf, filename);
}

static int cfg_inc_int(int value){ return ++value; }

static int config_read_option(m_config_t& config,const std::vector<const mpxp_option_t*>& conf_list,const std::string& opt,const std::string& param)
{
	int i=0,nconf = 0;
	long tmp_int;
	double tmp_float;
	int ret = -1;
	char *endptr;
	const mpxp_option_t* conf=NULL;

	mpxp_dbg3<<"read_option: opt='"<<opt<<"' param='"<<param<<"'"<<std::endl;
	std::string lopt=opt;
	std::transform(lopt.begin(),lopt.end(),lopt.begin(), ::tolower);
	for(nconf = 0 ;  conf_list[nconf] != NULL; nconf++) {
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
	mpxp_err<<"invalid option: "<<opt<<std::endl;
	ret = ERR_NOT_AN_OPTION;
	goto out;
	option_found :
	mpxp_dbg3<<"read_option: name='"<<conf[i].name<<"' type="<<conf[i].type<<std::endl;

	if (conf[i].flags & CONF_NOCFG && config.parser_mode == CONFIG_FILE) {
		mpxp_err<<"this option can only be used on command line:"<<opt<<std::endl;
		ret = ERR_NOT_AN_OPTION;
		goto out;
	}
	if (conf[i].flags & CONF_NOCMD && config.parser_mode == COMMAND_LINE) {
		mpxp_err<<"this option can only be used in config file:"<<opt<<std::endl;
		ret = ERR_NOT_AN_OPTION;
		goto out;
	}
	ret = config_is_entry_option(config,opt,param);
	if(ret != 0)
	  return ret;
	else
	  ret = -1;
	if(! IS_RUNNING(config) && ! IS_GLOBAL(config) &&
	   ! (conf[i].flags & CONF_GLOBAL)  && conf[i].type != CONF_TYPE_SUBCONFIG  )
	  m_config_push(config);
	if( !(conf[i].flags & CONF_NOSAVE) && ! (conf[i].flags & CONF_GLOBAL) )
	  m_config_save_option(config,&conf[i],opt,param);
	switch (conf[i].type) {
		case CONF_TYPE_FLAG:
			/* flags need a parameter in config file */
			if (config.parser_mode == CONFIG_FILE) {
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
					mpxp_err<<"invalid parameter for flag: "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}
				ret = 1;
			} else {	/* parser_mode == COMMAND_LINE */
				*((int *) conf[i].p) = conf[i].max;
				mpxp_dbg3<<"assigning "<<conf[i].name<<"="<<conf[i].max<<" as flag value"<<std::endl;
				ret = 0;
			}
			break;
		case CONF_TYPE_INT:
			if (param.empty())
				goto err_missing_param;

			tmp_int = ::strtol(param.c_str(), &endptr, 0);
			if (*endptr) {
				mpxp_err<<"parameter must be an integer: "<<param<<std::endl;
				ret = ERR_OUT_OF_RANGE;
				goto out;
			}

			if (conf[i].flags & CONF_MIN)
				if (tmp_int < conf[i].min) {
					mpxp_err<<"parameter must be >= "<<(int) conf[i].min<<": "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (tmp_int > conf[i].max) {
					mpxp_err<<"parameter must be <= "<<(int) conf[i].max<<": "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((int *) conf[i].p) = tmp_int;
			mpxp_dbg3<<"assigning "<<conf[i].name<<"="<<tmp_int<<" as int value"<<std::endl;
			ret = 1;
			break;
		case CONF_TYPE_FLOAT:
			if (param.empty())
				goto err_missing_param;

			tmp_float = ::strtod(param.c_str(), &endptr);

			if ((*endptr == ':') || (*endptr == '/'))
				tmp_float /= ::strtod(endptr+1, &endptr);

			if (*endptr) {
				mpxp_err<<"parameter must be a floating point number or a ratio (numerator[:/]denominator): "<<param<<std::endl;
				ret = ERR_MISSING_PARAM;
				goto out;
			}

			if (conf[i].flags & CONF_MIN)
				if (tmp_float < conf[i].min) {
					mpxp_err<<"parameter must be >= "<<(float)conf[i].min<<": "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (tmp_float > conf[i].max) {
					mpxp_err<<"parameter must be <= "<<(float)conf[i].max<<": "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((float *) conf[i].p) = tmp_float;
			mpxp_dbg3<<"assigning "<<conf[i].name<<"="<<tmp_float<<" as float value"<<std::endl;
			ret = 1;
			break;
		case CONF_TYPE_STRING:
			if (param.empty())
				goto err_missing_param;

			if (conf[i].flags & CONF_MIN)
				if (param.length() < conf[i].min) {
					mpxp_err<<"parameter must be >= "<<(int) conf[i].min<<" chars: "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (param.length() > conf[i].max) {
					mpxp_err<<"parameter must be <= "<<(int) conf[i].max<<" chars: "<<param<<std::endl;
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}
			*((char **) conf[i].p) = mp_strdup(param.c_str());
			mpxp_dbg3<<"assigning "<<conf[i].name<<"="<<param<<" as string value"<<std::endl;
			ret = 1;
			break;
		case CONF_TYPE_INC:
			*((int *) conf[i].p) = cfg_inc_int(*((int *) conf[i].p));
			ret = 0;
			break;
		case CONF_TYPE_INCLUDE:
			if (param.empty())
				goto err_missing_param;
			if (cfg_include(config, param) < 0) {
				ret = ERR_FUNC_ERR;
				goto out;
			}
			ret = 1;
			break;
		case CONF_TYPE_PRINT:
			mpxp_info<<(char *)conf[i].p;
			exit(1);
		default:
			mpxp_err<<"Unknown config type specified in conf-mplayerxp.h!"<<std::endl;
			break;
	}
out:
	if(ret >= 0 && ! IS_RUNNING(config) && ! IS_GLOBAL(config) && ! (conf[i].flags & CONF_GLOBAL) && conf[i].type != CONF_TYPE_SUBCONFIG ) {
	  PlayTree* dest = config.last_entry ? config.last_entry : config.last_parent;
	  std::string o;
	  if(config.sub_conf)	o=std::string(config.sub_conf)+":"+opt;
	  else			o=opt;

	  if(ret == 0)
	    dest->set_param(o,"");
	  else if(ret > 0)
	    dest->set_param(o,param);
	  m_config_pop(config);
	}
	return ret;
err_missing_param:
	mpxp_err<<"missing parameter for option: "<<opt<<std::endl;
	ret = ERR_MISSING_PARAM;
	goto out;
}

static const mpxp_option_t* m_config_find_option(const std::vector<const mpxp_option_t*>& list,const std::string& name);

int m_config_set_option(m_config_t& config,const std::string& _opt,const std::string& param) {
    size_t e;
    std::vector<const mpxp_option_t*> clist=config.opt_list;
    std::string opt=_opt;
    std::string s;
    mpxp_dbg2<<"Setting option "<<opt<<"="<<param<<std::endl;
    clist = config.opt_list;

    e=opt.find('.');
    if(e!=std::string::npos) {
	int flg,ret;
	const mpxp_option_t *subconf=NULL;
	mpxp_dbg2<<"Parsing "<<opt<<" as subconfig"<<std::endl;
	do {
	    if((e = opt.find('.'))==std::string::npos) break;
	    s=opt.substr(0,e);
	    mpxp_dbg2<<"Treat "<<s<<" as subconfig name"<<std::endl;
	    subconf = m_config_find_option(clist,s);
	    clist.clear();
	    if(!subconf) return ERR_NO_SUBCONF;
	    if(subconf->type!=CONF_TYPE_SUBCONFIG) return ERR_NO_SUBCONF;
	    clist.push_back(reinterpret_cast<const mpxp_option_t*>(subconf->p));
	    opt = opt.substr(e+1);
	    mpxp_dbg2<<"switching next subconf="<<subconf->name<<std::endl;
	}while(1);
	flg=config.flags;
	config.flags|=CONFIG_GLOBAL;
	ret=config_read_option(config,clist,opt,param);
	config.flags=flg;
	return ret;
    }

    e = opt.find(':');
    if(e!=std::string::npos && e<(opt.length()-1)) {
	int ret;
	const mpxp_option_t* m_opt;
	std::vector<const mpxp_option_t*> opt_list;
	s=opt.substr(0,e);
	m_opt=(const mpxp_option_t*)m_config_get_option_ptr(config,s);
	if(!m_opt) {
	    mpxp_err<<"m_config_set_option "<<opt<<"="<<param<<" : no "<<s<<" subconfig"<<std::endl;
	    return ERR_NOT_AN_OPTION;
	}
	opt_list.push_back(m_opt);
	s=opt.substr(e+1);
	ret = config_read_option(config,opt_list,s,param);
	return ret;
    }
    return config_read_option(config,config.opt_list,opt,param);
}

static void PRINT_LINENUM(const std::string& conffile,int line_num) { mpxp_err<<conffile<<"("<<line_num<<")"<<std::endl; }
static const int MAX_LINE_LEN=1000;
static const int MAX_OPT_LEN=100;
static const int MAX_PARAM_LEN=100;
MPXP_Rc m_config_parse_config_file(m_config_t& config,const std::string& conffile)
{
    FILE *fp;
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

    if (++config.recursion_depth > 1) mpxp_info<<"Reading config file: "<<conffile<<std::endl;

    if (config.recursion_depth > MAX_RECURSION_DEPTH) {
	mpxp_fatal<<": too deep 'include'. check your configfiles"<<std::endl;
	ret = MPXP_False;
	goto out;
    }

    if (init_conf(config, CONFIG_FILE) == -1) {
	ret = MPXP_False;
	goto out;
    }

    if ((line = new char [MAX_LINE_LEN + 1]) == NULL) {
	mpxp_fatal<<"can't get memory for 'line': "<<strerror(errno)<<std::endl;
	ret = MPXP_False;
	goto out;
    }

    if ((fp = ::fopen(conffile.c_str(), "r")) == NULL) {
	if (config.recursion_depth > 1) mpxp_err<<": "<<strerror(errno)<<std::endl;
	delete line;
	ret = MPXP_Ok;
	goto out;
    }
    if (config.recursion_depth > 1) mpxp_fatal<<std::endl;

    while (fgets(line, MAX_LINE_LEN, fp)) {
	if (errors >= 16) {
	    mpxp_fatal<<"too many errors"<<std::endl;
	    goto out;
	}

	line_num++;
	line_pos = 0;

	/* skip whitespaces */
	while (isspace(line[line_pos])) ++line_pos;

	/* EOL / comment */
	if (line[line_pos] == '\0' || line[line_pos] == '#') continue;

	/* read option. */
	for (opt_pos = 0; isprint(line[line_pos]) &&
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
	while (isspace(line[line_pos])) ++line_pos;

	/* check '=' */
	if (line[line_pos++] != '=') {
	    PRINT_LINENUM(conffile,line_num);
	    mpxp_err<<"option without parameter"<<std::endl;
	    ret = MPXP_False;
	    errors++;
	    continue;
	}

	/* whitespaces... */
	while (isspace(line[line_pos])) ++line_pos;

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
	    for (param_pos = 0; isprint(line[line_pos]) && !isspace(line[line_pos])
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
	while (isspace(line[line_pos])) ++line_pos;

	/* EOL / comment */
	if (line[line_pos] != '\0' && line[line_pos] != '#') {
	    PRINT_LINENUM(conffile,line_num);
	    mpxp_err<<"extra characters on line: "<<line+line_pos<<std::endl;
	    ret = MPXP_False;
	}

	tmp = m_config_set_option(config, opt, param);
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
    fclose(fp);
out:
    --config.recursion_depth;
    return ret;
}

MPXP_Rc mpxp_parse_command_line(m_config_t& config, const std::vector<std::string>& argv,const std::map<std::string,std::string>& envm)
{
    size_t i,siz=argv.size();
    int tmp;
    std::string opt;
    int no_more_opts = 0;

    if (init_conf(config, COMMAND_LINE) == -1) return MPXP_False;
    if(config.last_parent == NULL) config.last_parent = config.pt;
    /* in order to work recursion detection properly in parse_config_file */
    ++config.recursion_depth;

    for (i = 1; i < siz; i++) {
	 //next:
	opt = argv[i];
	if(opt=="--help") {
	    show_help();
	    exit(0);
	}
	if(opt=="--long-help") {
	    show_long_help(config,envm);
	    exit(0);
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
	    UNSET_GLOBAL(config);
	    if(config.last_entry == NULL) {
		config.last_parent->set_child(entry);
	    } else {
		config.last_entry->append_entry(entry);
		config.last_entry = NULL;
	    }
	    config.last_parent = entry;
	    continue;
	}

	if(opt[0] == '}' && opt[1] == '\0') {
	    if( ! config.last_parent || ! config.last_parent->get_parent()) {
		mpxp_err<<"too much }-"<<std::endl;
		goto err_out;
	    }
	    config.last_entry = config.last_parent;
	    config.last_parent = config.last_entry->get_parent();
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
	    tmp = m_config_set_option(config, item, parm);
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
	    if(argv[i]=="-") m_config_set_option(config,"use-stdin",NULL);
	    /* opt is not an option -> treat it as a filename */
	    UNSET_GLOBAL(config); // We start entry specific options
	    if(config.last_entry == NULL) config.last_parent->set_child(entry);
	    else config.last_entry->append_entry(entry);
	    config.last_entry = entry;
	}
    }

    --config.recursion_depth;
    if(config.last_parent != config.pt) mpxp_err<<"Missing }- ?"<<std::endl;
    UNSET_GLOBAL(config);
    SET_RUNNING(config);
    return MPXP_Ok;
err_out:
    --config.recursion_depth;
    mpxp_err<<"command line: "<<argv[i]<<std::endl;
    return MPXP_False;
}

MPXP_Rc m_config_register_options(m_config_t& config,const mpxp_option_t *args) {
  config.opt_list.push_back(args);
  return MPXP_Ok;
}

static const mpxp_option_t* m_config_find_option(const std::vector<const mpxp_option_t*>& list,const std::string& name) {
    unsigned i,j;
    const mpxp_option_t *conf;
    if(!list.empty()) {
	std::string ln=name;
	std::transform(ln.begin(),ln.end(),ln.begin(), ::tolower);
	for(j = 0; list[j] != NULL ; j++) {
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

const mpxp_option_t* m_config_get_option(const m_config_t& config,const std::string& arg) {
    size_t e;

    e = arg.find(':');
    if(e!=std::string::npos) {
	std::vector<const mpxp_option_t*> cl;
	const mpxp_option_t* opt;
	std::string s;
	s=arg.substr(0,e);
	opt = m_config_get_option(config,s);
	cl.push_back(opt);
	return m_config_find_option(cl,arg);
    }
    return m_config_find_option(config.opt_list,arg);
}

any_t* m_config_get_option_ptr(const m_config_t& config,const std::string& arg) {
  const mpxp_option_t* conf;

  conf = m_config_get_option(config,arg);
  if(!conf) return NULL;
  return conf->p;
}

int m_config_get_int (const m_config_t& config,const std::string& arg,int& err_ret) {
    int *ret;

    ret = (int*)m_config_get_option_ptr(config,arg);
    err_ret = 0;
    if(!ret) {
	err_ret = 1;
	return -1;
    }
    return *ret;
}

float m_config_get_float (const m_config_t& config,const std::string& arg,int& err_ret) {
    float *ret;

    ret = (float*)m_config_get_option_ptr(config,arg);
    err_ret = 0;
    if(!ret) {
	err_ret = 1;
	return -1;
    }
    return *ret;
}

inline int AS_INT(const mpxp_option_t* c) { return *((int*)c->p); }
inline void AS_INT(const mpxp_option_t* c,int val) { *((int*)c->p)=val; }

int m_config_set_int(m_config_t& config,const std::string& arg,int val) {
  const mpxp_option_t* opt;

  opt = m_config_get_option(config,arg);

  if(!opt || opt->type != CONF_TYPE_INT)
    return ERR_NOT_AN_OPTION;

  if(opt->flags & CONF_MIN && val < opt->min)
    return ERR_OUT_OF_RANGE;
  if(opt->flags & CONF_MAX && val > opt->max)
    return ERR_OUT_OF_RANGE;

  m_config_save_option(config,opt,arg,NULL);
  AS_INT(opt,val);

  return 1;
}

int m_config_set_float(m_config_t& config,const std::string& arg,float val) {
  const mpxp_option_t* opt;

  opt = m_config_get_option(config,arg);

  if(!opt || opt->type != CONF_TYPE_FLOAT)
    return ERR_NOT_AN_OPTION;

  if(opt->flags & CONF_MIN && val < opt->min)
    return ERR_OUT_OF_RANGE;
  if(opt->flags & CONF_MAX && val > opt->max)
    return ERR_OUT_OF_RANGE;

  m_config_save_option(config,opt,arg,NULL);
  *((float*)opt->p) = val;

  return 1;
}


int m_config_switch_flag(m_config_t& config,const std::string& opt) {
  const mpxp_option_t *conf;

  conf = m_config_get_option(config,opt);
  if(!conf || conf->type != CONF_TYPE_FLAG) return 0;
  if( AS_INT(conf) == conf->min) AS_INT(conf,conf->max);
  else if(AS_INT(conf) == conf->max) AS_INT(conf,conf->min);
  else return 0;

  return 1;
}

int m_config_set_flag(m_config_t& config,const std::string& opt, int state) {
  const mpxp_option_t *conf;

  conf = m_config_get_option(config,opt);
  if(!conf || conf->type != CONF_TYPE_FLAG) return 0;
  if(state) AS_INT(conf,conf->max);
  else AS_INT(conf,conf->min);
  return 1;
}

int m_config_get_flag(const m_config_t& config,const std::string& opt) {

  const mpxp_option_t* conf = m_config_get_option(config,opt);
  if(!conf || conf->type != CONF_TYPE_FLAG) return -1;
  if(AS_INT(conf) == conf->max) return 1;
  else if(AS_INT(conf) == conf->min) return 0;
  return -1;
}

int m_config_is_option_set(const m_config_t& config,const std::string& arg) {
  const mpxp_option_t* opt;
  config_save_t* save;
  int l,i;

  opt = m_config_get_option(config,arg);

  if(!opt)
    return -1;

  for(l = config.cs_level ; l >= 0 ; l--) {
    save = config.config_stack[l];
    if(!save)
      continue;
    for(i = 0 ; save[i].opt != NULL ; i++) {
      if(save[i].opt == opt)
	return 1;
    }
  }

  return 0;
}

static void __m_config_show_options(unsigned ntabs,const std::string& pfx,const mpxp_option_t* opts) {
    unsigned i,n;
    i=0;
    while(opts[i].name) {
	if(opts[i].type==CONF_TYPE_SUBCONFIG  && opts[i].p) {
	    std::string newpfx;
	    unsigned pfxlen;
	    for(n=0;n<ntabs;n++) mpxp_info<<" ";
	    mpxp_info<<opts[i].help<<":"<<std::endl;
	    pfxlen=strlen(opts[i].name)+1;
	    if(!pfx.empty())	pfxlen+=pfx.length();
	    if(!pfx.empty())	newpfx=pfx;
	    else		newpfx="";
	    newpfx+=opts[i].name;
	    newpfx+=".";
	    __m_config_show_options(ntabs+2,newpfx,(const mpxp_option_t *)opts[i].p);
	}
	else
	if(opts[i].type<=CONF_TYPE_PRINT) {
	    for(n=0;n<ntabs;n++) mpxp_info<<" ";
	    if(!pfx.empty())	mpxp_info<<std::left<<pfx<<std::endl;
	    else		mpxp_info<<"-"<<std::endl;
	    mpxp_info<<std::left<<opts[i].name<<" "
		     <<((opts[i].type==CONF_TYPE_PRINT && strcmp(opts[i].help,"show help")!=0)?opts[i].p:opts[i].help)
		     <<std::endl;
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
		if(defv) mpxp_info<<"\""<<*defv<<"\"";
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

void m_config_show_options(const m_config_t& args) {
    size_t j,sz=args.opt_list.size();
    const mpxp_option_t *opts;
    j=0;
    mpxp_info<<"List of available command-line options:"<<std::endl;
    for(j=0;j<sz;j++) {
	opts=args.opt_list[j];
	__m_config_show_options(2,"",opts);
    };
}
} // namespace mpxp
