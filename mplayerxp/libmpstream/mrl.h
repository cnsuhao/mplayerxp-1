/* MRL - Media Resource Locator */
#ifndef __MPXP_MRL_H
#define __MPXP_MRL_H 1

#include "mp_config.h"
/** Parses line which contains MRL and splits it on components.
  * @param line		source line to be parsed
  * @param user		buffer which will contain username if present (maybe NULL)
  * @param pass		buffer which will contain user's password if present (maybe NULL)
  * @param ms		buffer which will contain media source if present (maybe NULL)
  * @param port		buffer which will contain port of media source if present (maybe NULL)
  * @return		pointer to tail of source line (arguments)
  *
  * @warning		All pointers will be allocated by mp_malloc() and should be
  *			destroyed with calling of mp_free() function.
  * @note		line must not contain initial MRL descriptor (like dvdnav:// ftp:// ...)
  * @remark		Description of MRL:
  *			UPR://~user*pass@media_source:port#arg1=value1,arg2=value2,arg3=value3
  *			Examples: vcdnav:///dev/cdrom2#T1,T2-T15,E0
  *				  alsa:///dev/dsp2:65#rate=48000,surround
  *				  x11://~username:passwd@localhost:8081#bpp=32
  * @see		mrl_parse_params
**/
extern const char *mrl_parse_line(const char *line,char **user,char **pass,char **ms,char **port);

enum {
    MRL_TYPE_PRINT	=0, /**< NoType! Just printout value of argument */
    MRL_TYPE_BOOL	=1, /**< Boolean type. Accepts "on" "off" "yes" "no" "1" "0" values */
    MRL_TYPE_INT	=2, /**< Integer type. Accepts any values in min-max range */
    MRL_TYPE_FLOAT	=3, /**< Float type. Accepts any values in min-max range */
    MRL_TYPE_STRING	=4  /**< String type. Accepts any values */
};
/** Structurizes argument's parsing
  * @note		all string arguments will be allocated by mp_malloc() function
**/
typedef struct mrl_config
{
    const char*	arg;		/**< Name of argument */
    any_t* const value;		/**< Pointer to buffer where value will be saved */
    unsigned	type;		/**< Type of value */
    float	min;		/**< Minimal value of argument */
    float	max;		/**< Maximal value of argument */
}mrl_config_t;

/** Parses parameters of MRL line and fill corresponded structure.
  * @param param	buffer with parameters (it's return value of mrl_parse_line())
  * @param args		structure to be filled after parameters parsing
  * @return		pointer to unparsed tail of parameter's line.
  *
  * @remark		This function was splitted from mrl_parse_line() because developer
  *			may want to use non-standard arguments for plugin.
  * @see		mrl_parse_line
**/
extern const char *	mrl_parse_params(const char *param,const mrl_config_t * args);
#endif
