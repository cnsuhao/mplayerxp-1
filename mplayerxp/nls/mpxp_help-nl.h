// Translated by:  Panagiotis Issaris <takis@lumumba.luc.ac.be>
// UTF-8
#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (zie DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Gebruik:   mplayerxp [opties] [pad/]bestandsnaam",
"",
"Opties:",
" -vo <drv[:dev]>  selecteer video uitvoer driver & device (zie '-vo help' voor lijst)",
" -ao <drv[:dev]>  selecteer audio uitvoer driver & device (zie '-ao help' voor lijst)",
" -play.ss <timepos>ga naar opgegeven (seconden of hh:mm:ss) positie",
" -audio.off       speel het geluid niet af",
" -video.fs        volledig scherm afspeel opties (fullscr,vidmode chg,softw.scale)",
" -sub.file <bestand>specificeer het te gebruiken ondertitel bestand",
" -play.list<file> specificeer het te gebruiken playlist bestand",
" -sync.framedrop  activeer frame-dropping (voor trage machines)",
"",
"Toetsen:",
" <-  of  ->       ga 10 seconden achterwaarts/voorwaarts",
" omhoog of omlaag ga 1 minuut achterwaarts/voorwaarts",
" PGUP of PGDOWN   ga 10 minuten achterwaarts/voorwaarts",
" < or >           ga naar vorige/volgende item in playlist",
" p of SPACE       pauzeer film (druk eender welke toets om verder te gaan)",
" q of ESC         stop afspelen en sluit programma af",
" o                cycle OSD mode:  geen / zoekbalk / zoekbalk+tijd",
" * of /           verhoog of verlaag volume (druk 'm' om master/pcm te selecteren)",
"",
" * * * ZIE MANPAGE VOOR DETAILS, OVERIGE (GEAVANCEERDE) OPTIES EN TOETSEN ! * * *",
NULL
};
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "Bezig met afsluiten"
#define MSGTR_Exit_frames "Gevraagde aantal frames afgespeeld"
#define MSGTR_Exit_quit "Stop"
#define MSGTR_Exit_eof "Einde van bestand"
#define MSGTR_Fatal_error "Fatale fout"
#define MSGTR_NoHomeDir "Kan HOME dir niet vinden"
#define MSGTR_Playing "Bezig met het afspelen van"
