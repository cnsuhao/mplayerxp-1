#ifndef ASXPARSER_H
#define ASXPARSER_H 1
#include "osdep/mplib.h"
#include "playtree.h"
using namespace mpxp;

#include <string>
#include <map>

namespace mpxp {
    struct ASX_LineSave_t {
	const char* buffer;
	int line;
    };

    class ASX_Attrib {
	public:
	    ASX_Attrib() {}
	    ~ASX_Attrib() {}

	    struct stricomp { int operator() (const std::string& lhs, const std::string& rhs) const { return strcasecmp(lhs.c_str(),rhs.c_str()); }};

	    std::string	get(const std::string& key) { return _attrib[key]; }
	    void	set(const std::string& key,const std::string& value) { _attrib[key]=value; }
	    void	clear() { _attrib.clear(); }
	    std::map<std::string,std::string,ASX_Attrib::stricomp>& map() { return _attrib; }
	private:
	    std::map<std::string,std::string,ASX_Attrib::stricomp> _attrib;
    };

    class ASX_Parser : public Opaque {
	public:
	    ASX_Parser();
	    virtual ~ASX_Parser();

	    static play_tree_t*	build_tree(libinput_t* libinput,const char* buffer, int ref);

	    virtual int		parse_attribs(char* buffer,ASX_Attrib& _attribs) const;
	    /*
	     * Return -1 on error, 0 when nothing is found, 1 on sucess
	     */
	    virtual int		get_element(const char** _buffer,char** _element,char** _body,ASX_Attrib& _attribs);
	    int			get_line() const { return line; }
	private:
	    play_tree_t*	repeat(libinput_t*libinput,const char* buffer,ASX_Attrib& _attribs);
	    void		warning_attrib_required(const char *e, const char *a) const;
	    void		warning_body_parse_error(const char *e) const;
	    void		param(ASX_Attrib& attribs, play_tree_t* pt) const;
	    void		ref(ASX_Attrib& attribs, play_tree_t* pt) const;
	    play_tree_t*	entryref(libinput_t* libinput,char* buffer,ASX_Attrib& _attribs) const;
	    play_tree_t*	entry(const char* buffer,ASX_Attrib& _attribs);

	    int			line; // Curent line
	    ASX_LineSave_t*	ret_stack;
	    int			ret_stack_size;
	    char*		last_body;
	    int			deep;
    };
} // namespace mpxp
#endif
