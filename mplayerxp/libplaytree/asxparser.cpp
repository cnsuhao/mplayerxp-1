#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "libmpstream2/stream.h"
#include "playtreeparser.h"
#include "asxparser.h"
#include "libmpconf/cfgparser.h"
#include "mplayerxp.h"
#define MSGT_CLASS MSGT_PLAYTREE
#include "mp_msg.h"

////// List utils
namespace mpxp {
static void __FASTCALL__ asx_list_add(any_t* list_ptr,any_t* entry){
  any_t** list = *(any_t***)list_ptr;
  int c = 0;

  if(list != NULL)
    for( ; list[c] != NULL; c++) ;

  list = (any_t**)mp_realloc(list,sizeof(any_t*)*(c+2));

  list[c] = entry;
  list[c+1] = NULL;

  *(any_t***)list_ptr = list;
}


static void __FASTCALL__ asx_list_remove(any_t* list_ptr,any_t* entry,ASX_FreeFunc free_func) {
  any_t** list = *(any_t***)list_ptr;
  int c,e = -1;

  if(list == NULL) return;

  for(c = 0 ; list[c] != NULL; c++){
    if(list[c] == entry) e = c;
  }

  if(e == -1) return; // Not found

  if(free_func != NULL) free_func(list[e]);

  if(c == 1) { // Only one entry, we drop all
    delete list;
    *(any_t**)list_ptr = NULL;
    return;
  }

  if(c > e) // If c==e the memmove is not needed
    memmove(list+e,list+e+1,(c-e)*sizeof(any_t*));

  list = (any_t**)mp_realloc(list,(c-1)*sizeof(any_t*));
  list[c-1] = NULL;

  *(any_t***)list_ptr = list;
}

void __FASTCALL__ asx_list_free(any_t* list_ptr,ASX_FreeFunc free_func) {
  any_t** ptr = *(any_t***)list_ptr;
  if(ptr == NULL) return;
  if(free_func != NULL) {
    for( ; *ptr != NULL ; ptr++)
      free_func(*ptr);
  }
  delete *(any_t**)list_ptr;
  *(any_t**)list_ptr = NULL;
}

/////// Attribs utils

char* __FASTCALL__ asx_get_attrib(const char* attrib,char** attribs) {
    char** ptr;

    if(attrib == NULL || attribs == NULL) return NULL;
    for(ptr = attribs; ptr[0] != NULL; ptr += 2){
	if(strcasecmp(ptr[0],attrib) == 0) return mp_strdup(ptr[1]);
    }
    return NULL;
}

int __FASTCALL__ asx_attrib_to_enum(const char* val,const char** valid_vals) {
    const char** ptr;
    int r = 0;

    if(valid_vals == NULL || val == NULL) return -2;
    for(ptr = valid_vals ; ptr[0] != NULL ; ptr++) {
	if(strcasecmp(val,ptr[0]) == 0) return r;
	r++;
    }
    return -1;
}

void ASX_Parser::warning_attrib_invalid(char* elem, char* attrib,
					const char** valid_vals,char* val) const
{
    char *str,*vals;
    const char **ptr;
    int len;

    if(valid_vals == NULL || valid_vals[0] == NULL) return;

    len = strlen(valid_vals[0]) + 1;
    for(ptr = valid_vals+1 ; ptr[0] != NULL; ptr++) {
	len += strlen(ptr[0]);
	len += ((ptr[1] == NULL) ? 4 : 2);
    }
    str = vals = new char[len];
    vals += sprintf(vals,"%s",valid_vals[0]);
    for(ptr = valid_vals + 1 ; ptr[0] != NULL ; ptr++) {
	if(ptr[1] == NULL)	vals += sprintf(vals," or %s",ptr[0]);
	else			vals += sprintf(vals,", %s",ptr[0]);
    }
    MSG_ERR("at line %d : attribute %s of element %s is invalid (%s). Valid values are %s",
	    line,attrib,elem,val,str);
    delete str;
}

int ASX_Parser::get_yes_no_attrib(char* element, char* attrib,char** cattribs,int def) const {
    char* val = asx_get_attrib(attrib,cattribs);
    const char* valids[] = { "NO", "YES", NULL };
    int r;

    if(val == NULL) return def;
    r = asx_attrib_to_enum(val,valids);

    if(r < 0) {
	warning_attrib_invalid(element,attrib,valids,val);
	r = def;
    }
    delete val;
    return r;
}

void ASX_Parser::warning_attrib_required(const char *e, const char *a) const { MSG_WARN("At line %d : element %s don't have the required attribute %s",line,e,a); }
void ASX_Parser::warning_body_parse_error(const char *e) const { MSG_WARN("At line %d : error while parsing %s body",line,e); }

ASX_Parser::ASX_Parser() {}
ASX_Parser::~ASX_Parser() { if(ret_stack) delete ret_stack; }

int ASX_Parser::parse_attribs(char* buffer,char*** _attribs) const {
    char *ptr1, *ptr2, *ptr3;
    int n_attrib = 0;
    char **cattribs = NULL;
    char *attrib, *val;

    ptr1 = buffer;
    while(1) {
	for( ; isspace(*ptr1); ptr1++) { // Skip space
	    if(*ptr1 == '\0') goto pa_end;
	}
	ptr3 = strchr(ptr1,'=');
	if(ptr3 == NULL) break;
	for(ptr2 = ptr3-1; isspace(*ptr2); ptr2--) {
	    if (ptr2 <= ptr1) {
		MSG_ERR("At line %d : this should never append, back to attribute begin while skipping end space",line);
		goto pa_end;
	    }
	}
	attrib = new char[ptr2-ptr1+2];
	strncpy(attrib,ptr1,ptr2-ptr1+1);
	attrib[ptr2-ptr1+1] = '\0';

	ptr1 = strchr(ptr3,'"');
	if(ptr1 == NULL || ptr1[1] == '\0') {
	    MSG_WARN("At line %d : can't find attribute %s value",line,attrib);
	    delete attrib;
	    break;
	}
	ptr1++;
	ptr2 = strchr(ptr1,'"');
	if (ptr2 == NULL) {
	    MSG_WARN("At line %d : value of attribute %s isn't finished",line,attrib);
	    delete attrib;
	    break;
	}
	val = new char[ptr2-ptr1+1];
	strncpy(val,ptr1,ptr2-ptr1);
	val[ptr2-ptr1] = '\0';
	n_attrib++;

	cattribs = (char**)mp_realloc(cattribs,(2*n_attrib+1)*sizeof(char*));
	cattribs[n_attrib*2-2] = attrib;
	cattribs[n_attrib*2-1] = val;

	ptr1 = ptr2+1;
    }
pa_end:
    if(n_attrib > 0)
	cattribs[n_attrib*2-0] = NULL;
    *_attribs = cattribs;
    return n_attrib;
}

/*
 * Return -1 on error, 0 when nothing is found, 1 on sucess
 */
int ASX_Parser::get_element(const char** _buffer, char** _element,
			    char** _body,char*** _attribs) {
    const char *ptr1,*ptr2, *ptr3, *ptr4;
    char *cattribs = NULL;
    char *element = NULL, *body = NULL;
    const char *ret = NULL;
    const char *buffer;
    int n_attrib = 0;
    int body_line = 0,attrib_line,ret_line,in = 0;

    if(_buffer == NULL || _element == NULL || _body == NULL || _attribs == NULL) {
	MSG_ERR("At line %d : asx_get_element called with invalid value",line);
	return -1;
    }

    *_body = *_element = NULL;
    *_attribs =  NULL;
    buffer = *_buffer;

    if(buffer == NULL) return 0;

    if(ret_stack && /*last_body && */buffer != last_body) {
	ASX_LineSave_t* ls = ret_stack;
	int i;
	for(i = 0 ; i < ret_stack_size ; i++) {
	    if(buffer == ls[i].buffer) {
		line = ls[i].line;
		break;
	    }
	}
	if( i < ret_stack_size) {
	    i++;
	    if( i < ret_stack_size) memmove(ret_stack,ret_stack+i, (ret_stack_size - i)*sizeof(ASX_LineSave_t));
	    ret_stack_size -= i;
	    if(ret_stack_size > 0) ret_stack = (ASX_LineSave_t*)mp_realloc(ret_stack,ret_stack_size*sizeof(ASX_LineSave_t));
	    else {
		delete ret_stack;
		ret_stack = NULL;
	    }
	}
    }
    ptr1 = buffer;
    while(1) {
	for( ; ptr1[0] != '<' ; ptr1++) {
	    if(ptr1[0] == '\0') {
		ptr1 = NULL;
		break;
	    }
	    if(ptr1[0] == '\n') line++;
	}
	//ptr1 = strchr(ptr1,'<');
	if(!ptr1 || ptr1[1] == '\0') return 0; // Nothing found

	if(strncmp(ptr1,"<!--",4) == 0) { // Comments
	    for( ; strncmp(ptr1,"-->",3) != 0 ; ptr1++) {
		if(ptr1[0] == '\0') {
		    ptr1 = NULL;
		    break;
		}
		if(ptr1[0] == '\n') line++;
	    }
	    //ptr1 = strstr(ptr1,"-->");
	    if(!ptr1) {
		MSG_ERR("At line %d : unfinished comment",line);
		return -1;
	    }
	} else break;
    }

    // Is this space skip very useful ??
    for(ptr1++; isspace(ptr1[0]); ptr1++) { // Skip space
	if(ptr1[0] == '\0') {
	    MSG_ERR("At line %d : EOB reached while parsing element start",line);
	    return -1;
	}
	if(ptr1[0] == '\n') line++;
    }

    for(ptr2 = ptr1; isalpha(*ptr2); ptr2++) { // Go to end of name
	if(*ptr2 == '\0'){
	    MSG_ERR("At line %d : EOB reached while parsing element start",line);
	    return -1;
	}
	if(ptr2[0] == '\n') line++;
    }

    element = new char[ptr2-ptr1+1];
    strncpy(element,ptr1,ptr2-ptr1);
    element[ptr2-ptr1] = '\0';

    for( ; isspace(*ptr2); ptr2++) { // Skip space
	if(ptr2[0] == '\0') {
	    MSG_ERR("At line %d : EOB reached while parsing element start",line);
	    delete element;
	    return -1;
	}
	if(ptr2[0] == '\n') line++;
    }
    attrib_line = line;

    for(ptr3 = ptr2; ptr3[0] != '\0'; ptr3++) { // Go to element end
	if(ptr3[0] == '>' || strncmp(ptr3,"/>",2) == 0) break;
	if(ptr3[0] == '\n') line++;
    }
    if(ptr3[0] == '\0' || ptr3[1] == '\0') { // End of file
	MSG_ERR("At line %d : EOB reached while parsing element start",line);
	delete element;
	return -1;
    }

    // Save attribs string
    if(ptr3-ptr2 > 0) {
	cattribs = new char[ptr3-ptr2+1];
	strncpy(cattribs,ptr2,ptr3-ptr2);
	cattribs[ptr3-ptr2] = '\0';
    }
    //bs_line = line;
    if(ptr3[0] != '/') { // Not Self closed element
	ptr3++;
	for( ; isspace(*ptr3); ptr3++) { // Skip space on body begin
	    if(*ptr3 == '\0') {
		MSG_ERR("At line %d : EOB reached while parsing %s element body",line,element);
		delete element;
		if(cattribs) delete cattribs;
		return -1;
	    }
	    if(ptr3[0] == '\n') line++;
	}
	ptr4 = ptr3;
	body_line = line;
	while(1) { // Find closing element
	    for( ; ptr4[0] != '<' ; ptr4++) {
		if(ptr4[0] == '\0') {
		    ptr4 = NULL;
		    break;
		}
		if(ptr4[0] == '\n') line++;
	    }
	    if(strncmp(ptr4,"<!--",4) == 0) { // Comments
		for( ; strncmp(ptr4,"-->",3) != 0 ; ptr4++) {
		    if(ptr4[0] == '\0') {
			ptr4 = NULL;
			break;
		    }
		    if(ptr1[0] == '\n') line++;
		}
		continue;
	    }
	    if(ptr4 == NULL || ptr4[1] == '\0') {
		MSG_ERR("At line %d : EOB reached while parsing %s element body",line,element);
		delete element;
		if(cattribs) delete cattribs;
		return -1;
	    }
	    if(ptr4[1] != '/' && strncasecmp(element,ptr4+1,strlen(element)) == 0) {
		in++;
		ptr4+=2;
		continue;
	    } else if(strncasecmp(element,ptr4+2,strlen(element)) == 0) { // Extract body
		if(in > 0) {
		    in--;
		    ptr4 += 2+strlen(element);
		    continue;
		}
		ret = ptr4+strlen(element)+3;
		if(ptr4 != ptr3) {
		    ptr4--;
		    for( ; ptr4 != ptr3 && isspace(*ptr4); ptr4--) ;// Skip space on body end
		    //	    if(ptr4[0] == '\0') line--;
		    //}
		    ptr4++;
		    body = new char[ptr4-ptr3+1];
		    strncpy(body,ptr3,ptr4-ptr3);
		    body[ptr4-ptr3] = '\0';
	    }
	    break;
	    } else {
		ptr4 += 2;
	    }
	}
    } else {
	ret = ptr3 + 2; // 2 is for />
    }

    for( ; ret[0] != '\0' && isspace(ret[0]); ret++) { // Skip space
	if(ret[0] == '\n') line++;
    }

    ret_line = line;

    if(cattribs) {
	line = attrib_line;
	n_attrib = parse_attribs(cattribs,_attribs);
	delete cattribs;
	if(n_attrib < 0) {
	    MSG_WARN("At line %d : error while parsing element %s attributes",line,element);
	    delete element;
	    delete body;
	    return -1;
	}
    } else
    *_attribs = NULL;

    *_element = element;
    *_body = body;

    last_body = body;
    ret_stack_size++;
    ret_stack = (ASX_LineSave_t*)mp_realloc(ret_stack,ret_stack_size*sizeof(ASX_LineSave_t));
    if(ret_stack_size > 1) memmove(ret_stack+1,ret_stack,(ret_stack_size-1)*sizeof(ASX_LineSave_t));
    ret_stack[0].buffer = const_cast<char*>(ret);
    ret_stack[0].line = ret_line;
    line = body ? body_line : ret_line;

    *_buffer = ret;
    return 1;
}

void ASX_Parser::param(char** cattribs, play_tree_t* pt) const {
    const char *name,*val;

    name = asx_get_attrib("NAME",cattribs);
    if(!name) {
	warning_attrib_required("PARAM" ,"NAME" );
	return;
    }
    val = asx_get_attrib("VALUE",cattribs);
    if(m_config_get_option(mpxp_context().mconfig,name) == NULL) {
	MSG_WARN("Found unknow param in asx: %s",name);
	if(val)	MSG_WARN("=%s\n",val);
	else	MSG_WARN("\n");
	return;
    }
    play_tree_set_param(pt,name,val);
}

void ASX_Parser::ref(char** cattribs, play_tree_t* pt) const {
    char *href;

    href = asx_get_attrib("HREF",cattribs);
    if(href == NULL) {
	warning_attrib_required("REF" ,"HREF" );
	return;
    }
    // replace http my mmshttp to avoid infinite loops
    if (strncmp(href, "http://", 7) == 0) {
	char *newref = new char [3 + strlen(href) + 1];
	strcpy(newref, "mms");
	strcpy(newref + 3, href);
	delete href;
	href = newref;
    }
    play_tree_add_file(pt,href);
    MSG_V("Adding file %s to element entry\n",href);
    delete href;
}

play_tree_t* ASX_Parser::entryref(libinput_t* libinput,char* buffer,char** _attribs) const {
    play_tree_t* pt;
    char *href;
    Stream* stream;
    play_tree_parser_t* ptp;
    int f;
    UNUSED(buffer);

    if(deep > 0) return NULL;

    href = asx_get_attrib("HREF",_attribs);
    if(href == NULL) {
	warning_attrib_required("ENTRYREF" ,"HREF" );
	return NULL;
    }
    stream=new(zeromem) Stream;
    if(stream->open(libinput,href,&f)!=MPXP_Ok) {
	MSG_WARN("Can't open playlist %s\n",href);
	delete stream;
	return NULL;
    }
    if(!(stream->type() & Stream::Type_Text)) {
	MSG_WARN("URL %s dont point to a playlist\n",href);
	delete stream;
	return NULL;
    }
    MSG_V("Adding playlist %s to element entryref\n",href);
    ptp = play_tree_parser_new(stream,deep+1);
    pt = play_tree_parser_get_play_tree(libinput,ptp);
    play_tree_parser_free(ptp);
    delete stream;
//MSG_INFO("Need to implement entryref\n");
    return pt;
}

play_tree_t* ASX_Parser::entry(const char* buffer,char** _attribs) {
    char *celement,*body,**cattribs;
    int r,nref=0;
    play_tree_t *pt_ref;
    UNUSED(_attribs);

    pt_ref = play_tree_new();

    while(buffer && buffer[0] != '\0') {
	r = get_element(&buffer,&celement,&body,&cattribs);
	if(r < 0) {
	    warning_body_parse_error("ENTRY");
	    return NULL;
	} else if (r == 0) break; // No more element
	if(strcasecmp(celement,"REF") == 0) {
	    ref(cattribs,pt_ref);
	    MSG_DBG2("Adding element %s to entry\n",celement);
	    nref++;
	} else MSG_DBG2("Ignoring element %s\n",celement);
	if(body) delete body;
	asx_free_attribs(cattribs);
    }
    if(nref <= 0) {
	play_tree_free(pt_ref,1);
	return NULL;
    }
    return pt_ref;
}

play_tree_t* ASX_Parser::repeat(libinput_t*libinput,const char* buffer,char** _attribs) {
    char *element,*body,**cattribs;
    play_tree_t *pt_repeat, *list=NULL, *pt_entry;
    char* count;
    int r;

    pt_repeat = play_tree_new();

    count = asx_get_attrib("COUNT",_attribs);
    if(count == NULL) {
	MSG_DBG2("Setting element repeat loop to infinit\n");
	pt_repeat->loop = -1; // Infinit
    } else {
	pt_repeat->loop = atoi(count);
	delete count;
	if(pt_repeat->loop == 0) pt_repeat->loop = 1;
	MSG_DBG2("Setting element repeat loop to %d\n",pt_repeat->loop);
    }

    while(buffer && buffer[0] != '\0') {
	r = get_element(&buffer,&element,&body,&cattribs);
	if(r < 0) {
	    warning_body_parse_error("REPEAT");
	    return NULL;
	} else if (r == 0) break; // No more element
	if(strcasecmp(element,"ENTRY") == 0) {
	    pt_entry = entry(body,cattribs);
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		MSG_DBG2("Adding element %s to repeat\n",element);
	    }
	} else if(strcasecmp(element,"ENTRYREF") == 0) {
	    pt_entry = entryref(libinput,body,cattribs);
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		MSG_DBG2("Adding element %s to repeat\n",element);
	    }
	} else if(strcasecmp(element,"REPEAT") == 0) {
	    pt_entry = repeat(libinput,body,cattribs);
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		MSG_DBG2("Adding element %s to repeat\n",element);
	    }
	} else if(strcasecmp(element,"PARAM") == 0) {
	    param(cattribs,pt_repeat);
	} else MSG_DBG2("Ignoring element %s\n",element);
	if(body) delete body;
	asx_free_attribs(cattribs);
    }

    if(!list) {
	play_tree_free(pt_repeat,1);
	return NULL;
    }
    play_tree_set_child(pt_repeat,list);
    return pt_repeat;
}

play_tree_t* ASX_Parser::build_tree(libinput_t*libinput,const char* buffer,int deep) {
    char *element,*asx_body,**asx_attribs,*body, **attribs;
    int r;
    play_tree_t *asx,*pt_entry,*list = NULL;
    ASX_Parser& parser = *new(zeromem) ASX_Parser;

    parser.line = 1;
    parser.deep = deep;

    r = parser.get_element(&buffer,&element,&asx_body,&asx_attribs);
    if(r < 0) {
	MSG_ERR("At line %d : Syntax error ???",parser.line);
	delete &parser;
	return NULL;
    } else if(r == 0) { // No contents
	MSG_ERR("empty asx element");
	delete &parser;
	return NULL;
    }

    if(strcasecmp(element,"ASX") != 0) {
	MSG_ERR("first element isn't ASX, it's %s\n",element);
	asx_free_attribs(asx_attribs);
	if(body) delete body;
	delete &parser;
	return NULL;
    }

    if(!asx_body) {
	MSG_ERR("ASX element is empty");
	asx_free_attribs(asx_attribs);
	delete &parser;
	return NULL;
    }

    asx = play_tree_new();
    buffer = asx_body;
    while(buffer && buffer[0] != '\0') {
	r = parser.get_element(&buffer,&element,&body,&attribs);
	if(r < 0) {
	    parser.warning_body_parse_error("ASX");
	    delete &parser;
	    return NULL;
	} else if (r == 0) break; // No more element
	if(strcasecmp(element,"ENTRY") == 0) {
	    pt_entry = parser.entry(body,attribs);
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		MSG_DBG2("Adding element %s to asx\n",element);
	    }
	} else if(strcasecmp(element,"ENTRYREF") == 0) {
	    pt_entry = parser.entryref(libinput,body,attribs);
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		MSG_DBG2("Adding element %s to asx\n",element);
	    }
	} else if(strcasecmp(element,"REPEAT") == 0) {
	    pt_entry = parser.repeat(libinput,body,attribs);
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		MSG_DBG2("Adding element %s to asx\n",element);
	    }
	} else MSG_DBG2("Ignoring element %s\n",element);
	if(body) delete body;
	asx_free_attribs(attribs);
    }

    delete asx_body;
    asx_free_attribs(asx_attribs);
    delete &parser;

    if(!list) {
	play_tree_free(asx,1);
	return NULL;
    }
    play_tree_set_child(asx,list);
    return asx;
}
} // namespace mpxp
