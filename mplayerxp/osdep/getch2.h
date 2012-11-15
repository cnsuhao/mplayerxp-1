/* GyS-TermIO v2.0 (for GySmail v3)          (C) 1999 A'rpi/ESP-team */
/* a very small replacement of ncurses library */

/* Get screen-size using IOCTL call. */
extern void get_screen_size();

/* Load key definitions from the TERMCAP database. 'termtype' can be NULL */
extern int load_termcap(char *termtype);

/* Enable and disable STDIN line-buffering */
extern void getch2_enable();
extern void getch2_disable();

/* Read a character or a special key code (see keycodes.h) */
extern int getch2(int halfdelay_time);

