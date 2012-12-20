// Translated by:  Anders Rune Jensen <root@gnulinux.dk>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (se DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Brug:   mplayerxp [muligheder] [sti/]filnavn",
"",
"Muligheder:",
" -vo <drv[:dev]> vælger video driver og enhed (se '-vo help for en komplet liste')",
" -ao <drv[:dev]> vælger lyd driver og enhed (se '-ao help for en komplet liste')",
" -play.ss <timepos> søger til en given (sekunder eller hh:mm:ss) position",
" -audio.off      afspiller uden lyd",
" -video.fs       type af afspilning i fuldskærm (fuldskærm, video mode, software skalering)",
" -sub.file <file>specificer undertekst-fil",
" -play.list<file>specificer afspilningsliste-fil",
" -sync.framedrop slår billede-skip til (kan hjælpe langsomme maskiner)",
"",
"Keys:",
" <-  or  ->      søger 10 sekunder frem eller tilbage",
" up or down      søger 1 minut frem eller tilbage ",
" < or >          søger frem og tilbage i en afspilningsliste",
" p or SPACE      pause filmen (starter igen ved en vilkårlig tast)",
" q or ESC        stop afspilning og afslut program",
" o               vælger OSD typer:  ingen / søgebar / søgebar+tid",
" * or /          forøjer eller formindsker volumen (tryk 'm' for at vælge master/pcm)",
"",
" * * * SE MANPAGE FOR FLERE DETALJER, YDERLIGERE (AVANCEREDE) MULIGHEDER OG TASTER ! * * *",
NULL
};
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

