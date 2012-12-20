// Transated by: Johannes Feigl, johannes.feigl@mcse.at
// UTF-8

// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (siehe DOCS!)\n"
"\n";

static char help_text[]=
"Verwendung:   mplayerxp [optionen] [verzeichnis/]dateiname\n"
"\n"
"Optionen:\n"
" -vo <drv[:dev]> Videoausgabetreiber & -Gerät (siehe '-vo help' für eine Liste)\n"
" -ao <drv[:dev]> Audioausgabetreiber & -Gerät (siehe '-ao help' für eine Liste)\n"
" -play.ss <timepos> Starte abspielen ab Position (Sekunden oder hh:mm:ss)\n"
" -audio.off      Spiele keinen Sound\n"
" -video.fs       Vollbild Optionen (Vollbild, Videomode, Softwareskalierung)\n"
" -sub.file <file>Benutze Untertitledatei\n"
" -sync.framedrop Benutze frame-dropping (für langsame Rechner)\n"
"\n"
"Tasten:\n"
" <- oder ->      Springe zehn Sekunden vor/zurück\n"
" rauf / runter   Springe eine Minute vor/zurück\n"
" p oder LEER     PAUSE (beliebige Taste zum Fortsetzen)\n"
" q oder ESC      Abspielen stoppen und Programm beenden\n"
" o               OSD Mode:  Aus / Suchleiste / Suchleiste + Zeit\n"
" * oder /        Lautstärke verstellen ('m' für Auswahl Master/Wave)\n"
"\n"
" * * * IN DER MANPAGE STEHEN WEITERE KEYS UND OPTIONEN ! * * *\n"
"\n";
#endif

// ========================= MPlayer Ausgaben ===========================

// mplayer.c:

#define MSGTR_Exiting "Beende"
#define MSGTR_Exit_frames "Angeforderte Anzahl an Frames gespielt"
#define MSGTR_Exit_quit "Ende"
#define MSGTR_Exit_eof "Ende der Datei"
#define MSGTR_Fatal_error "Schwerer Fehler"
#define MSGTR_NoHomeDir "Kann Homeverzeichnis nicht finden"
#define MSGTR_Playing "Spiele"
