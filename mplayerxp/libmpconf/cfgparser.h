/*
 * command line and config file parser
 */
#ifndef __CFG_PARSER_H
#define __CFG_PARSER_H 1
#include "xmpcore/xmp_enums.h"
#include <vector>
#include <string>
#include <map>

namespace	usr {
    struct libinput_t;
    struct PlayTree;

    /* config types */
    enum {
	CONF_TYPE_FLAG	=0,
	CONF_TYPE_INT	=1,
	CONF_TYPE_FLOAT	=2,
	CONF_TYPE_STRING=3,
	CONF_TYPE_PRINT	=4,
	CONF_TYPE_INC	=5,
	CONF_TYPE_INCLUDE=6,
	CONF_TYPE_SUBCONFIG=7
    };

    enum {
	ERR_NOT_AN_OPTION	=-1,
	ERR_MISSING_PARAM	=-2,
	ERR_OUT_OF_RANGE	=-3,
	ERR_FUNC_ERR	=-4,
	ERR_NO_SUBCONF	=-5
    };

    /* config flags */
    enum {
	CONF_MIN		=(1<<0),
	CONF_MAX		=(1<<1),
	CONF_RANGE		=(CONF_MIN|CONF_MAX),
	CONF_NOCFG		=(1<<2),
	CONF_NOCMD		=(1<<3),
	CONF_GLOBAL		=(1<<4),
	CONF_NOSAVE		=(1<<5)
    };

    // Plain C-structure for static declarations
    struct mpxp_option_t {
	const char*	name;
	any_t* const	p;
	unsigned int	type;
	unsigned int	flags;
	float		min,max;
	const char*	help;
    };

    typedef void (*cfg_default_func_t)(mpxp_option_t*,const std::string&);

    class M_Config : public Opaque {
	public:
	    enum mode_e {
		CmdLine=0,
		File=1,
	    };
	    enum flags_e {
		Global=0x01,
		Running=0x02
	    };

	    M_Config(PlayTree* pt,libinput_t& _libinput);
	    virtual ~M_Config();

	    virtual MPXP_Rc	parse_command_line(const std::vector<std::string>& argv,const std::map<std::string,std::string>& envm);
	    virtual MPXP_Rc	register_options(const mpxp_option_t *args);
	    virtual MPXP_Rc	parse_config_file(const std::string& conffile);
	    /** Return 1 on sucess 0 on failure
	    **/
	    virtual int		set_option(const std::string& opt,const std::string& param);
	    /** Get the config struct defining an option
	      * @return NULL on error
	    **/
	    virtual const mpxp_option_t* get_option(const std::string& arg) const;
	    /** Get the p field of the struct defining an option
	      * @return NULL on error
	    **/
	    virtual any_t*	get_option_ptr(const std::string& arg) const;
	    /** Return 0 on error 1 on success
	    **/
	    virtual int		switch_flag(const std::string& opt);
	    /** Return 0 on error 1 on success
	    **/
	    virtual int		set_flag(const std::string& opt, int max);
	    /** Return the value of a flag (O or 1) and -1 on error
	    **/
	    virtual int		get_flag(const std::string& opt) const;
	    /** Set the value of an int option
	      * @return	0 on error 1 on success
	    **/
	    virtual int		set_int(const std::string& arg,int val);
	    /** Get the value of an int option
	      * @param err_ret	If it is not NULL it's set to 1 on error
	      * @return		the option value or -1 on error
	    **/
	    virtual int		get_int (const std::string& arg,int& err_ret) const;
	    /** Set the value of a float option
	      * @return	0 on error 1 on success
	    **/
	    virtual int		set_float(const std::string& arg,float val);
	    /** Get the value of a float option
	      * @param err_ret	If it is not NULL it's set to 1 on error
	      * @return		the option value or -1 on error
	    **/
	    virtual float	get_float (const std::string& arg,int& err_ret) const;

	    virtual void	parse_cfgfiles(const std::map<std::string,std::string>& envm);
	    virtual void	show_options() const;
	private:
	    void	set_global();
	    void	unset_global();
	    int		is_global() const;
	    void	set_running();
	    void	unset_running();
	    int		is_running() const;

	    void	__show_options(unsigned ntabs,const std::string& pfx,const mpxp_option_t* opts) const;
	    const mpxp_option_t* find_option(const std::vector<const mpxp_option_t*>& list,const std::string& name) const;
	    MPXP_Rc	init_conf(mode_e mode);
	    int		read_option(const std::vector<const mpxp_option_t*>& conf_list,const std::string& opt,const std::string& param);
	    MPXP_Rc	cfg_include(const std::string& filename);
	    int		cfg_inc_int(int value);
	    int		is_entry_option(const std::string& opt,const std::string& param);

	    std::vector<const mpxp_option_t*>	opt_list;
	    mode_e	parser_mode;
	    flags_e	flags;
	    PlayTree*	pt; // play tree we use for playlist option, etc
	    PlayTree*	last_entry; // last added entry
	    PlayTree*	last_parent; // if last_entry is NULL we must create child of this
	    int		recursion_depth;
	    libinput_t&	libinput;
    };
    inline M_Config::flags_e operator~(M_Config::flags_e a) { return static_cast<M_Config::flags_e>(~static_cast<unsigned>(a)); }
    inline M_Config::flags_e operator|(M_Config::flags_e a, M_Config::flags_e b) { return static_cast<M_Config::flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline M_Config::flags_e operator&(M_Config::flags_e a, M_Config::flags_e b) { return static_cast<M_Config::flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline M_Config::flags_e operator^(M_Config::flags_e a, M_Config::flags_e b) { return static_cast<M_Config::flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline M_Config::flags_e operator|=(M_Config::flags_e& a, M_Config::flags_e b) { return (a=static_cast<M_Config::flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline M_Config::flags_e operator&=(M_Config::flags_e& a, M_Config::flags_e b) { return (a=static_cast<M_Config::flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline M_Config::flags_e operator^=(M_Config::flags_e& a, M_Config::flags_e b) { return (a=static_cast<M_Config::flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }
} // namespace	usr

#endif /* __CONFIG_H */
