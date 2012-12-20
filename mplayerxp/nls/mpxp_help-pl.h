// Translated by:  Bohdan Horst <nexus@hoth.amu.edu.pl>
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
"Użycie:   mplayerxp [opcje] [ścieżka/]nazwa",
"",
"Opcje:",
" -vo <drv[:dev]> wybór sterownika[:urządzenia] video (lista po '-vo help')",
" -ao <drv[:dev]> wybór sterownika[:urządzenia] audio (lista po '-ao help')",
" -play.ss <timepos>skok do podanej pozycji (sekundy albo hh:mm:ss)",
" -audio.off      odtwarzanie bez dźwięku",
" -video.fs       opcje pełnoekranowe (pełen ekran,zmiana trybu,skalowanie)",
" -sub.file <file>wybór pliku z napisami",
" -play.list<file>wybór pliku z playlistą",
" -sync.framedrop gubienie klatek (dla wolnych maszyn)",
"",
"Klawisze:",
" Right,Up,PgUp   skok naprzód o 10 sekund, 1 minutę, 10 minut",
" Left,Down,PgDn  skok do tyłu o 10 sekund, 1 minutę, 10 minut",
" < lub >         przeskok o jedną pozycję w playliście",
" p lub SPACE     zatrzymanie filmu (kontynuacja - dowolny klawisz)",
" q lub ESC       zatrzymanie odtwarzania i wyjście z programu",
" o               przełączanie trybów OSD: pusty / belka / belka i zegar",
" * lub /         zwiększenie lub zmniejszenie natężenia dźwięku",
"                 (naciśnij 'm' żeby wybrać master/pcm)",
"",
" **** DOKŁADNY SPIS WSZYSTKICH DOSTĘPNYCH OPCJI ZNAJDUJE SIĘ W MANUALU! ****",
NULL
};
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
