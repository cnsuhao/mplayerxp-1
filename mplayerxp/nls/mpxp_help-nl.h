// Translated by:  Panagiotis Issaris <takis@lumumba.luc.ac.be>
// UTF-8
#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (zie DOCS!)\n"
"\n";

static char help_text[]=
"Gebruik:   mplayerxp [opties] [pad/]bestandsnaam\n"
"\n"
"Opties:\n"
" -vo <drv[:dev]>  selecteer video uitvoer driver & device (zie '-vo help' voor lijst)\n"
" -ao <drv[:dev]>  selecteer audio uitvoer driver & device (zie '-ao help' voor lijst)\n"
" -play.ss <timepos>ga naar opgegeven (seconden of hh:mm:ss) positie\n"
" -audio.off       speel het geluid niet af\n"
" -video.fs        volledig scherm afspeel opties (fullscr,vidmode chg,softw.scale)\n"
" -sub.file <bestand>specificeer het te gebruiken ondertitel bestand\n"
" -play.list<file> specificeer het te gebruiken playlist bestand\n"
" -sync.framedrop  activeer frame-dropping (voor trage machines)\n"
"\n"
"Toetsen:\n"
" <-  of  ->       ga 10 seconden achterwaarts/voorwaarts\n"
" omhoog of omlaag ga 1 minuut achterwaarts/voorwaarts\n"
" PGUP of PGDOWN   ga 10 minuten achterwaarts/voorwaarts\n"
" < or >           ga naar vorige/volgende item in playlist\n"
" p of SPACE       pauzeer film (druk eender welke toets om verder te gaan)\n"
" q of ESC         stop afspelen en sluit programma af\n"
" o                cycle OSD mode:  geen / zoekbalk / zoekbalk+tijd\n"
" * of /           verhoog of verlaag volume (druk 'm' om master/pcm te selecteren)\n"
"\n"
" * * * ZIE MANPAGE VOOR DETAILS, OVERIGE (GEAVANCEERDE) OPTIES EN TOETSEN ! * * *\n"
"\n";
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
