#ifndef __NLS_NLS_H
#define __NLS_NLS_H 1

/** Returns pointer to screen's codepage
  * @return		pointer into environment.
**/
extern char *nls_get_screen_cp(void);

/** Recodes from given codepage into screen's codepage
  * @param src_cp	decribes source codepage
  * @param param	points buffer to be converted
  * @param len		length of buffer to be converted
  * @return		allocated buffer with result or performs strdup() function
  *			if convertion was failed.
**/
extern char *nls_recode2screen_cp(const char *src_cp,const char *param,unsigned len);

/** Recodes to given codepage from screen's codepage
  * @param to_cp	decribes given codepage
  * @param param	points buffer to be converted
  * @param size		points buffer where size of resulting buffer will be stored
  * @return		allocated buffer with result or performs strdup() function
  *			if convertion was failed.
**/
extern char *nls_recode_from_screen_cp(const char *to_cp,const char *param,size_t *size);

extern unsigned utf8_get_char(const char **str);
#endif
