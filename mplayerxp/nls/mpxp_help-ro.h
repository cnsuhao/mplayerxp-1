// Translated by:  Codre Adrian <codreadrian@softhome.net>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Folosire:   mplayerxp [opţiuni] [cale/]fişier",
"",
"Opţiuni:",
" -vo <drv[:disp]> Ieşirea video: driver&dispozitiv ('-vo help' pentru o listă)",
" -ao <drv[:disp]> Ieşirea audio: driver&dispozitiv ('-ao help' pentru o listă)",
" -play.ss <poziţia>sare la poziţia (secunde sau oo:mm:ss)",
" -audio.off       fără sunet",
" -video.fs       mod tot ecranul (tot ecranul,schimbă modul,scalat prin software)",
" -sub.file <fişier>specifică fişierul cu subtitrări",
" -sync.framedrop activează săritul cadrelor (pentru calculatoare lente)",
"",
"Taste:",
" <-  sau  ->      caută faţă/spate cu 10 secunde",
" sus sau jos      caută faţă/spate cu 1 minut",
" p sau SPACE      pune filmul pe pauză (orice tastă pentru a continua)",
" q sau ESC        opreşte filmul şi iese din program",
" o                roteşte modurile OSD: nimic / bară progres / bară progres+ceas",
" * sau /          creşte sau scade volumul (apăsaţi 'm' pentru principal/wav)",
"",
" * * * VEDEŢI MANUALUL PENTRU DETALII,(ALTE) OPŢIUNI AVANSATE ŞI TASTE ! * * *",
NULL
};
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
