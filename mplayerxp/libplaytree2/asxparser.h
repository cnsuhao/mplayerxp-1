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

    class ASX_Element {
	public:
	    ASX_Element() {}
	    ~ASX_Element() {}

	    const std::string&	name(const std::string& s) { return _name=s; }
	    const std::string&	body(const std::string& s) { return _body=s; }
	    ASX_Attrib&		attribs(ASX_Attrib& a) { return _attribs=a; }
	    const std::string&	name() const { return _name; }
	    const std::string&	body() const { return _body; }
	    ASX_Attrib&		attribs() { return _attribs; }
	    void		clear() { name(""); body(""); attribs().clear(); }
	private:
	    std::string		_name;
	    std::string		_body;
	    ASX_Attrib		_attribs;
    };

    class ASX_Parser : public Opaque {
	public:
	    ASX_Parser();
	    virtual ~ASX_Parser();

	    static PlayTree*	build_tree(libinput_t& libinput,const char* buffer, int ref);

	    virtual int		parse_attribs(const char* buffer,ASX_Attrib& _attribs) const;
	    /*
	     * Return -1 on error, 0 when nothing is found, 1 on sucess
	     */
	    virtual int		get_element(const char** _buffer,ASX_Element& _attribs);
	    int			get_line() const { return line; }
	private:
	    PlayTree*		repeat(libinput_t&libinput,const char* buffer,ASX_Attrib& _attribs);
	    void		warning_attrib_required(const char *elem, const char *attr) const;
	    void		warning_body_parse_error(const char *elem) const;
	    void		param(ASX_Attrib& attribs, PlayTree* pt) const;
	    void		ref(ASX_Attrib& attribs, PlayTree* pt) const;
	    PlayTree*		entryref(libinput_t& libinput,const char* buffer,ASX_Attrib& _attribs) const;
	    PlayTree*		entry(const char* buffer,ASX_Attrib& _attribs);

	    int			line; // Curent line
	    ASX_LineSave_t*	ret_stack;
	    int			ret_stack_size;
	    char*		last_body;
	    int			deep;
    };
} // namespace mpxp
#endif
