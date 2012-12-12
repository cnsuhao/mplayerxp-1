#ifndef ASXPARSER_H
#define ASXPARSER_H 1

#include "osdep/mplib.h"
#include "playtree.h"

using namespace mpxp;

namespace mpxp {
    struct ASX_LineSave_t {
	const char* buffer;
	int line;
    };

    class ASX_Parser : public Opaque {
	public:
	    ASX_Parser();
	    virtual ~ASX_Parser();

	    static play_tree_t*	build_tree(libinput_t* libinput,const char* buffer, int ref);

	    virtual int		parse_attribs(char* buffer,char*** _attribs);
	    /*
	     * Return -1 on error, 0 when nothing is found, 1 on sucess
	     */
	    virtual int		get_element(const char** _buffer,char** _element,char** _body,char*** _attribs);
	    int			get_line() const { return line; }
	private:
	    play_tree_t*	repeat(libinput_t*libinput,const char* buffer,char** _attribs);
	    void		warning_attrib_invalid(char* elem, char* attrib,const char** valid_vals,char* val);
	    void		warning_attrib_required(const char *e, const char *a);
	    void		warning_body_parse_error(const char *e);
	    int			get_yes_no_attrib(char* element, char* attrib,char** attribs,int def);
	    void		param(char** attribs, play_tree_t* pt);
	    void		ref(char** attribs, play_tree_t* pt);
	    play_tree_t*	entryref(libinput_t* libinput,char* buffer,char** _attribs);
	    play_tree_t*	entry(const char* buffer,char** _attribs);

	    int			line; // Curent line
	    ASX_LineSave_t*	ret_stack;
	    int			ret_stack_size;
	    char*		last_body;
	    int			deep;
};

/////// Attribs utils

extern char* __FASTCALL__ asx_get_attrib(const char* attrib,char** attribs);

extern int __FASTCALL__ asx_attrib_to_enum(const char* val,char** valid_vals);

////// List utils

typedef void (* __FASTCALL__ ASX_FreeFunc)(any_t* arg);

extern void __FASTCALL__ asx_list_free(any_t* list_ptr,ASX_FreeFunc free_func);

static inline void asx_free_attribs(any_t*a) { asx_list_free(&a,mp_free); }
} // namespace mpxp
#endif
