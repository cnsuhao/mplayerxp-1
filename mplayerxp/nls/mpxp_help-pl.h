// Translated by:  Bohdan Horst <nexus@hoth.amu.edu.pl>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Użycie:   mplayerxp [opcje] [ścieżka/]nazwa\n"
"\n"
"Opcje:\n"
" -vo <drv[:dev]> wybór sterownika[:urządzenia] video (lista po '-vo help')\n"
" -ao <drv[:dev]> wybór sterownika[:urządzenia] audio (lista po '-ao help')\n"
" -play.ss <timepos>skok do podanej pozycji (sekundy albo hh:mm:ss)\n"
" -audio.off      odtwarzanie bez dźwięku\n"
" -video.fs       opcje pełnoekranowe (pełen ekran,zmiana trybu,skalowanie)\n"
" -sub.file <file>wybór pliku z napisami\n"
" -play.list<file>wybór pliku z playlistą\n"
" -sync.framedrop gubienie klatek (dla wolnych maszyn)\n"
"\n"
"Klawisze:\n"
" Right,Up,PgUp   skok naprzód o 10 sekund, 1 minutę, 10 minut\n"
" Left,Down,PgDn  skok do tyłu o 10 sekund, 1 minutę, 10 minut\n"
" < lub >         przeskok o jedną pozycję w playliście\n"
" p lub SPACE     zatrzymanie filmu (kontynuacja - dowolny klawisz)\n"
" q lub ESC       zatrzymanie odtwarzania i wyjście z programu\n"
" o               przełączanie trybów OSD: pusty / belka / belka i zegar\n"
" * lub /         zwiększenie lub zmniejszenie natężenia dźwięku\n"
"                 (naciśnij 'm' żeby wybrać master/pcm)\n"
"\n"
" **** DOKŁADNY SPIS WSZYSTKICH DOSTĘPNYCH OPCJI ZNAJDUJE SIĘ W MANUALU! ****\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "Wychodzę"
#define MSGTR_Exit_frames "Zadana liczba klatek odtworzona"
#define MSGTR_Exit_quit "Wyjście"
#define MSGTR_Exit_eof "Koniec pliku"
#define MSGTR_Fatal_error "Błąd krytyczny"
#define MSGTR_NoHomeDir "Nie mogę znaleźć katalogu HOME"
#define MSGTR_Playing "Odtwarzam"
