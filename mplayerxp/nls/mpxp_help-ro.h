// Translated by:  Codre Adrian <codreadrian@softhome.net>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Folosire:   mplayerxp [opţiuni] [cale/]fişier\n"
"\n"
"Opţiuni:\n"
" -vo <drv[:disp]> Ieşirea video: driver&dispozitiv ('-vo help' pentru o listă)\n"
" -ao <drv[:disp]> Ieşirea audio: driver&dispozitiv ('-ao help' pentru o listă)\n"
" -play.ss <poziţia>sare la poziţia (secunde sau oo:mm:ss)\n"
" -audio.off       fără sunet\n"
" -video.fs       mod tot ecranul (tot ecranul,schimbă modul,scalat prin software)\n"
" -sub.file <fişier>specifică fişierul cu subtitrări\n"
" -sync.framedrop activează săritul cadrelor (pentru calculatoare lente)\n"
"\n"
"Taste:\n"
" <-  sau  ->      caută faţă/spate cu 10 secunde\n"
" sus sau jos      caută faţă/spate cu 1 minut\n"
" p sau SPACE      pune filmul pe pauză (orice tastă pentru a continua)\n"
" q sau ESC        opreşte filmul şi iese din program\n"
" o                roteşte modurile OSD: nimic / bară progres / bară progres+ceas\n"
" * sau /          creşte sau scade volumul (apăsaţi 'm' pentru principal/wav)\n"
"\n"
" * * * VEDEŢI MANUALUL PENTRU DETALII,(ALTE) OPŢIUNI AVANSATE ŞI TASTE ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "Ies"
#define MSGTR_Exit_frames "Numărul de cadre cerut a fost redat"
#define MSGTR_Exit_quit "Ieşire"
#define MSGTR_Exit_eof "Sfârşitul fişierului"
#define MSGTR_Fatal_error "Eroare fatală"
#define MSGTR_NoHomeDir "Nu găsesc directorul HOME"
#define MSGTR_Playing "Afişez"
