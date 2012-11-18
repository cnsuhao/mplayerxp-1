#ifndef ASXPARSER_H
#define ASXPARSER_H 1

#include "osdep/mplib.h"
#include "playtree.h"

typedef struct _ASX_Parser_t ASX_Parser_t;

typedef struct {
  const char* buffer;
  int line;
} ASX_LineSave_t;

struct _ASX_Parser_t {
  int line; // Curent line
  ASX_LineSave_t *ret_stack;
  int ret_stack_size;
  char* last_body;
  int deep;
};

extern play_tree_t* __FASTCALL__ asx_parser_build_tree(any_t* libinput,const char* buffer, int ref);

extern ASX_Parser_t* asx_parser_new(void);

extern void __FASTCALL__ asx_parser_free(ASX_Parser_t* parser);

/*
 * Return -1 on error, 0 when nothing is found, 1 on sucess
 */
extern int __FASTCALL__ asx_get_element(ASX_Parser_t* parser,const char** _buffer,
		char** _element,char** _body,char*** _attribs);

extern int __FASTCALL__ asx_parse_attribs(ASX_Parser_t* parser,char* buffer,char*** _attribs);

/////// Attribs utils

extern char* __FASTCALL__ asx_get_attrib(const char* attrib,char** attribs);

extern int __FASTCALL__ asx_attrib_to_enum(const char* val,char** valid_vals);

////// List utils

typedef void (* __FASTCALL__ ASX_FreeFunc)(any_t* arg);

extern void __FASTCALL__ asx_list_free(any_t* list_ptr,ASX_FreeFunc free_func);

static inline void asx_free_attribs(any_t*a) { asx_list_free(&a,mp_free); }

#endif
