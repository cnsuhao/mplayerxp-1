// Transated by: Johannes Feigl, johannes.feigl@mcse.at
// UTF-8

// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (siehe DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Verwendung:   mplayerxp [optionen] [verzeichnis/]dateiname",
"",
"Optionen:",
" -vo <drv[:dev]> Videoausgabetreiber & -Gerät (siehe '-vo help' für eine Liste)",
" -ao <drv[:dev]> Audioausgabetreiber & -Gerät (siehe '-ao help' für eine Liste)",
" -play.ss <timepos> Starte abspielen ab Position (Sekunden oder hh:mm:ss)",
" -audio.off      Spiele keinen Sound",
" -video.fs       Vollbild Optionen (Vollbild, Videomode, Softwareskalierung)",
" -sub.file <file>Benutze Untertitledatei",
" -sync.framedrop Benutze frame-dropping (für langsame Rechner)",
"",
"Tasten:",
" <- oder ->      Springe zehn Sekunden vor/zurück",
" rauf / runter   Springe eine Minute vor/zurück",
" p oder LEER     PAUSE (beliebige Taste zum Fortsetzen)",
" q oder ESC      Abspielen stoppen und Programm beenden",
" o               OSD Mode:  Aus / Suchleiste / Suchleiste + Zeit",
" * oder /        Lautstärke verstellen ('m' für Auswahl Master/Wave)",
"",
" * * * IN DER MANPAGE STEHEN WEITERE KEYS UND OPTIONEN ! * * *",
NULL
};
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
