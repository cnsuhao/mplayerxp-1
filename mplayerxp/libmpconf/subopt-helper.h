#ifndef MPLAYER_SUBOPT_HELPER_H
#define MPLAYER_SUBOPT_HELPER_H

#include "mpxp_config.h"
/**
 * \file subopt-helper.h
 *
 * \brief Datatype and functions declarations for usage
 *        of the suboption parser.
 *
 */
enum {
    OPT_ARG_BOOL	=0,
    OPT_ARG_INT		=1,
    OPT_ARG_STR		=2,
    OPT_ARG_MSTRZ	=3, ///< A malloced, zero terminated string, use mp_free()!
    OPT_ARG_FLOAT	=4
};
typedef int (*opt_test_f)(any_t*);

/** simple structure for defining the option name, type and storage location */
struct opt_t {
  const char * name; ///< string that identifies the option
  int type;    ///< option type as defined in subopt-helper.h
  any_t* valp; ///< pointer to the mem where the value should be stored
  opt_test_f test; ///< argument test func ( optional )
};

/** parses the string for the options specified in opt */
int subopt_parse( char const * const str, const opt_t * opts );


/*------------------ arg specific types and declaration -------------------*/
typedef struct strarg_t {
  int len; ///< length of the string determined by the parser
  char const * str;  ///< pointer to position inside the parse string
};

int int_non_neg( int * i );
int int_pos( int * i );

int strargcmp(strarg_t *arg, const char *str);
int strargcasecmp(strarg_t *arg, char *str);
#endif /* MPLAYER_SUBOPT_HELPER_H */
