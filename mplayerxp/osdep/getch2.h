#ifndef __GETCH2_H_INCLUDED
#define __GETCH2_H_INCLUDED 1
namespace	usr {
    /* GyS-TermIO v2.0 (for GySmail v3)          (C) 1999 A'rpi/ESP-team */
    /* a very small replacement of ncurses library */
    /* Load key definitions from the TERMCAP database. 'termtype' can be NULL */
    int load_termcap(const std::string& termtype,const std::map<std::string,std::string>& envm);

    /* Enable and disable STDIN line-buffering */
    void getch2_enable();
    void getch2_disable();

    /* Read a character or a special key code (see keycodes.h) */
    int getch2(int halfdelay_time);
} // namespace	usr
#endif
