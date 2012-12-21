#include "mpxp_config.h"
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
#include "playtree_msg.h"

namespace mpxp {

void ASX_Parser::warning_attrib_required(const char *e, const char *a) const {
    mpxp_warn<<"At line "<<line<<" : element "<<e<<" don't have the required attribute "<<a;
}
void ASX_Parser::warning_body_parse_error(const char *e) const {
    mpxp_warn<<"At line "<<line<<" : error while parsing "<<e<<" body";
}

ASX_Parser::ASX_Parser() {}
ASX_Parser::~ASX_Parser() { if(ret_stack) delete ret_stack; }

int ASX_Parser::parse_attribs(const char* buffer,ASX_Attrib& _attribs) const {
    const char *ptr1, *ptr2, *ptr3;
    int n_attrib = 0;
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
		mpxp_err<<"At line "<<line<<" : this should never append, back to attribute begin while skipping end space"<<std::endl;
		goto pa_end;
	    }
	}
	attrib = new char[ptr2-ptr1+2];
	strncpy(attrib,ptr1,ptr2-ptr1+1);
	attrib[ptr2-ptr1+1] = '\0';

	ptr1 = strchr(ptr3,'"');
	if(ptr1 == NULL || ptr1[1] == '\0') {
	    mpxp_warn<<"At line "<<line<<" : can't find attribute "<<attrib<<" value"<<std::endl;
	    delete attrib;
	    break;
	}
	ptr1++;
	ptr2 = strchr(ptr1,'"');
	if (ptr2 == NULL) {
	    mpxp_warn<<"At line "<<line<<" : value of attribute "<<attrib<<" isn't finished"<<std::endl;
	    delete attrib;
	    break;
	}
	val = new char[ptr2-ptr1+1];
	strncpy(val,ptr1,ptr2-ptr1);
	val[ptr2-ptr1] = '\0';
	n_attrib++;

	_attribs.set(attrib,val);

	ptr1 = ptr2+1;
    }
pa_end:
    return n_attrib;
}

/*
 * Return -1 on error, 0 when nothing is found, 1 on sucess
 */
int ASX_Parser::get_element(const char** _buffer, ASX_Element& _element) {
    const char *ptr1,*ptr2, *ptr3, *ptr4;
    char *cattribs = NULL;
    char *element = NULL, *body = NULL;
    const char *ret = NULL;
    const char *buffer;
    int n_attrib = 0;
    int body_line = 0,attrib_line,ret_line,in = 0;

    if(_buffer == NULL) {
	mpxp_err<<"At line "<<line<<" : asx_get_element called with invalid value"<<std::endl;
	return -1;
    }

    _element.clear();
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
		mpxp_err<<"At line "<<line<<" : unfinished comment"<<std::endl;
		return -1;
	    }
	} else break;
    }

    // Is this space skip very useful ??
    for(ptr1++; isspace(ptr1[0]); ptr1++) { // Skip space
	if(ptr1[0] == '\0') {
	    mpxp_err<<"At line "<<line<<" : EOB reached while parsing element start"<<std::endl;
	    return -1;
	}
	if(ptr1[0] == '\n') line++;
    }

    for(ptr2 = ptr1; isalpha(*ptr2); ptr2++) { // Go to end of name
	if(*ptr2 == '\0'){
	    mpxp_err<<"At line "<<line<<" : EOB reached while parsing element start"<<std::endl;
	    return -1;
	}
	if(ptr2[0] == '\n') line++;
    }

    element = new char[ptr2-ptr1+1];
    strncpy(element,ptr1,ptr2-ptr1);
    element[ptr2-ptr1] = '\0';

    for( ; isspace(*ptr2); ptr2++) { // Skip space
	if(ptr2[0] == '\0') {
	    mpxp_err<<"At line "<<line<<" : EOB reached while parsing element start"<<std::endl;
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
	mpxp_err<<"At line "<<line<<" : EOB reached while parsing element start"<<std::endl;
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
		mpxp_err<<"At line "<<line<<" : EOB reached while parsing "<<element<<" element body"<<std::endl;
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
		mpxp_err<<"At line "<<line<<" : EOB reached while parsing "<<element<<" element body"<<std::endl;
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
	n_attrib = parse_attribs(cattribs,_element.attribs());
	delete cattribs;
	if(n_attrib < 0) {
	    mpxp_warn<<"At line "<<line<<" : error while parsing element "<<element<<" attributes"<<std::endl;
	    delete element;
	    delete body;
	    return -1;
	}
    } else _element.attribs().clear();

    _element.name(element?element:"");
    _element.body(body?body:"");

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

void ASX_Parser::param(ASX_Attrib& cattribs, play_tree_t* pt) const {
    std::string name,val;

    name = cattribs.get("NAME");
    if(name.empty()) {
	warning_attrib_required("PARAM" ,"NAME" );
	return;
    }
    val = cattribs.get("VALUE");
    if(m_config_get_option(*mpxp_context().mconfig,name.c_str()) == NULL) {
	mpxp_warn<<"Found unknow param in asx: "<<name<<std::endl;
	if(!val.empty())mpxp_warn<<"="<<val<<std::endl;
	else		mpxp_warn<<std::endl;
	return;
    }
    play_tree_set_param(pt,mp_strdup(name.c_str()),mp_strdup(val.c_str()));
}

void ASX_Parser::ref(ASX_Attrib& cattribs, play_tree_t* pt) const {
    std::string href;

    href = cattribs.get("HREF");
    if(href.empty()) {
	warning_attrib_required("REF" ,"HREF" );
	return;
    }
    // replace http my mmshttp to avoid infinite loops
    if (href.substr(0,7)=="http://") {
	href = "mms"+href;
    }
    play_tree_add_file(pt,mp_strdup(href.c_str()));
    mpxp_v<<"Adding file "<<href<<" to element entry"<<std::endl;
}

play_tree_t* ASX_Parser::entryref(libinput_t& libinput,const char* buffer,ASX_Attrib& _attribs) const {
    play_tree_t* pt;
    std::string href;
    Stream* stream;
    play_tree_parser_t* ptp;
    int f;
    UNUSED(buffer);

    if(deep > 0) return NULL;

    href = _attribs.get("HREF");
    if(href.empty()) {
	warning_attrib_required("ENTRYREF" ,"HREF" );
	return NULL;
    }
    stream=new(zeromem) Stream;
    if(stream->open(libinput,href,&f)!=MPXP_Ok) {
	mpxp_warn<<"Can't open playlist "<<href<<std::endl;
	delete stream;
	return NULL;
    }
    if(!(stream->type() & Stream::Type_Text)) {
	mpxp_warn<<"URL "<<href<<" dont point to a playlist"<<std::endl;
	delete stream;
	return NULL;
    }
    mpxp_v<<"Adding playlist "<<href<<" to element entryref"<<std::endl;
    ptp = play_tree_parser_new(stream,deep+1);
    pt = play_tree_parser_get_play_tree(libinput,ptp);
    play_tree_parser_free(ptp);
    delete stream;
    return pt;
}

play_tree_t* ASX_Parser::entry(const char* buffer,ASX_Attrib& _attribs) {
    ASX_Element element;
    int r,nref=0;
    play_tree_t *pt_ref;
    UNUSED(_attribs);

    pt_ref = play_tree_new();

    while(buffer && buffer[0] != '\0') {
	r = get_element(&buffer,element);
	if(r < 0) {
	    warning_body_parse_error("ENTRY");
	    return NULL;
	} else if (r == 0) break; // No more element
	if(strcasecmp(element.name().c_str(),"REF") == 0) {
	    ref(element.attribs(),pt_ref);
	    mpxp_dbg2<<"Adding element "<<element.name()<<" to entry"<<std::endl;
	    nref++;
	} else mpxp_dbg2<<"Ignoring element "<<element.name()<<std::endl;
    }
    if(nref <= 0) {
	play_tree_free(pt_ref,1);
	return NULL;
    }
    return pt_ref;
}

play_tree_t* ASX_Parser::repeat(libinput_t&libinput,const char* buffer,ASX_Attrib& _attribs) {
    ASX_Element element;
    play_tree_t *pt_repeat, *list=NULL, *pt_entry;
    std::string count;
    int r;

    pt_repeat = play_tree_new();

    count = _attribs.get("COUNT");
    if(count.empty()) {
	mpxp_dbg2<<"Setting element repeat loop to infinit"<<std::endl;
	pt_repeat->loop = -1; // Infinit
    } else {
	pt_repeat->loop = ::atoi(count.c_str());
	if(pt_repeat->loop == 0) pt_repeat->loop = 1;
	mpxp_dbg2<<"Setting element repeat loop to "<<pt_repeat->loop<<std::endl;
    }

    while(buffer && buffer[0] != '\0') {
	r = get_element(&buffer,element);
	if(r < 0) {
	    warning_body_parse_error("REPEAT");
	    return NULL;
	} else if (r == 0) break; // No more element
	if(strcasecmp(element.name().c_str(),"ENTRY") == 0) {
	    pt_entry = entry(element.body().c_str(),element.attribs());
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		mpxp_dbg2<<"Adding element "<<element.name()<<" to repeat"<<std::endl;
	    }
	} else if(strcasecmp(element.name().c_str(),"ENTRYREF") == 0) {
	    pt_entry = entryref(libinput,element.body().c_str(),element.attribs());
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		mpxp_dbg2<<"Adding element "<<element.name()<<" to repeat"<<std::endl;
	    }
	} else if(strcasecmp(element.name().c_str(),"REPEAT") == 0) {
	    pt_entry = repeat(libinput,element.body().c_str(),element.attribs());
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		mpxp_dbg2<<"Adding element "<<element.name()<<" to repeat"<<std::endl;
	    }
	} else if(strcasecmp(element.name().c_str(),"PARAM") == 0) {
	    param(element.attribs(),pt_repeat);
	} else mpxp_dbg2<<"Ignoring element "<<element.name()<<std::endl;
    }

    if(!list) {
	play_tree_free(pt_repeat,1);
	return NULL;
    }
    play_tree_set_child(pt_repeat,list);
    return pt_repeat;
}

play_tree_t* ASX_Parser::build_tree(libinput_t&libinput,const char* buffer,int deep) {
    ASX_Element asx_element,element;
    int r;
    play_tree_t *asx,*pt_entry,*list = NULL;
    ASX_Parser& parser = *new(zeromem) ASX_Parser;

    parser.line = 1;
    parser.deep = deep;

    r = parser.get_element(&buffer,asx_element);
    if(r < 0) {
	mpxp_err<<"At line "<<parser.line<<" : Syntax error ???"<<std::endl;
	delete &parser;
	return NULL;
    } else if(r == 0) { // No contents
	mpxp_err<<"empty asx element"<<std::endl;
	delete &parser;
	return NULL;
    }

    if(strcasecmp(element.name().c_str(),"ASX") != 0) {
	mpxp_err<<"first element isn't ASX, it's "<<element.name()<<std::endl;
	delete &parser;
	return NULL;
    }

    if(asx_element.body().empty()) {
	mpxp_err<<"ASX element is empty"<<std::endl;
	delete &parser;
	return NULL;
    }

    asx = play_tree_new();
    buffer = asx_element.body().c_str();
    while(buffer && buffer[0] != '\0') {
	r = parser.get_element(&buffer,element);
	if(r < 0) {
	    parser.warning_body_parse_error("ASX");
	    delete &parser;
	    return NULL;
	} else if (r == 0) break; // No more element
	if(strcasecmp(element.name().c_str(),"ENTRY") == 0) {
	    pt_entry = parser.entry(element.body().c_str(),element.attribs());
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		mpxp_dbg2<<"Adding element "<<element.name()<<" to asx"<<std::endl;
	    }
	} else if(strcasecmp(element.name().c_str(),"ENTRYREF") == 0) {
	    pt_entry = parser.entryref(libinput,element.body().c_str(),element.attribs());
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		mpxp_dbg2<<"Adding element "<<element.name()<<" to asx"<<std::endl;
	    }
	} else if(strcasecmp(element.name().c_str(),"REPEAT") == 0) {
	    pt_entry = parser.repeat(libinput,element.body().c_str(),element.attribs());
	    if(pt_entry) {
		if(!list) list = pt_entry;
		else play_tree_append_entry(list,pt_entry);
		mpxp_dbg2<<"Adding element "<<element.name()<<" to asx"<<std::endl;
	    }
	} else mpxp_dbg2<<"Ignoring element "<<element.name()<<std::endl;
    }

    delete &parser;

    if(!list) {
	play_tree_free(asx,1);
	return NULL;
    }
    play_tree_set_child(asx,list);
    return asx;
}
} // namespace mpxp
