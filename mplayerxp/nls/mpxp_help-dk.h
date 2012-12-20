// Translated by:  Anders Rune Jensen <root@gnulinux.dk>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (se DOCS!)\n"
"\n";

static char help_text[]=
"Brug:   mplayerxp [muligheder] [sti/]filnavn\n"
"\n"
"Muligheder:\n"
" -vo <drv[:dev]> vælger video driver og enhed (se '-vo help for en komplet liste')\n"
" -ao <drv[:dev]> vælger lyd driver og enhed (se '-ao help for en komplet liste')\n"
" -play.ss <timepos> søger til en given (sekunder eller hh:mm:ss) position\n"
" -audio.off      afspiller uden lyd\n"
" -video.fs       type af afspilning i fuldskærm (fuldskærm, video mode, software skalering)\n"
" -sub.file <file>specificer undertekst-fil\n"
" -play.list<file>specificer afspilningsliste-fil\n"
" -sync.framedrop slår billede-skip til (kan hjælpe langsomme maskiner)\n"
"\n"
"Keys:\n"
" <-  or  ->      søger 10 sekunder frem eller tilbage\n"
" up or down      søger 1 minut frem eller tilbage \n"
" < or >          søger frem og tilbage i en afspilningsliste\n"
" p or SPACE      pause filmen (starter igen ved en vilkårlig tast)\n"
" q or ESC        stop afspilning og afslut program\n"
" o               vælger OSD typer:  ingen / søgebar / søgebar+tid\n"
" * or /          forøjer eller formindsker volumen (tryk 'm' for at vælge master/pcm)\n"
"\n"
" * * * SE MANPAGE FOR FLERE DETALJER, YDERLIGERE (AVANCEREDE) MULIGHEDER OG TASTER ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "Afslutter"
#define MSGTR_Exit_frames "Anmoder om et antal billeder bliver afspillet"
#define MSGTR_Exit_quit "Afslut"
#define MSGTR_Exit_eof "Slutningen af filen"
#define MSGTR_Fatal_error "Fatal fejl"
#define MSGTR_NoHomeDir "Kan ikke finde hjemmekatalog (HOME)"
#define MSGTR_Playing "Afspiller"

