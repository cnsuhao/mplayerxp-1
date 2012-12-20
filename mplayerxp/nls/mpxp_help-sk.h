// Translated by:  Daniel Beňa, benad@centrum.cz
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (viď DOCS!)",
NULL
};

// Preklad do slovenčiny

static const char* help_text[]={
"",
"Použitie:   mplayerxp [prepínače] [cesta/]menosúboru",
"",
"Prepínače:",
" -vo <drv[:dev]> výber výstup. video ovládača&zariadenia (-vo help pre zoznam)",
" -ao <drv[:dev]> výber výstup. audio ovládača&zariadenia (-ao help pre zoznam)",
" -play.ss <timepos>posun na pozíciu (sekundy alebo hh:mm:ss)",
" -audio.off      prehrávať bez zvuku",
" -video.fs       voľby pre prehrávanie na celú obrazovku (celá obrazovka\n                 meniť videorežim, softvérový zoom)",
" -sub.file <file>voľba súboru s titulkami",
" -play.list<file> určenie súboru so zoznamom prehrávaných súborov",
" -sync.framedrop  povoliť zahadzovanie snímkov (pre pomalé stroje)",
"",
"Klávesy:",
" <-  alebo  ->   posun vzad/vpred o 10 sekund",
" hore / dole     posun vzad/vpred o  1 minútu",
" < alebo >       posun vzad/vpred v zozname prehrávaných súborov",
" p al. medzerník pauza pri prehrávaní (pokračovaní stlačením niektorej klávesy)",
" q alebo ESC     koniec prehrávania a ukončenie programu",
" o               cyklická zmena režimu OSD:  nič / pozícia / pozícia+čas",
" * alebo /       pridať alebo ubrať hlasitosť (stlačením 'm' výber master/pcm)",
"",
" * * * * PREČÍTAJTE SI MAN STRÁNKU PRE DETAILY (ĎALŠIE VOĽBY A KLÁVESY)! * * * *",
NULL
};
#endif

// ========================= MPlayer messages ===========================
// mplayer.c:

#define MSGTR_Exiting "Končím"
#define MSGTR_Exit_frames "Požadovaný počet snímkov prehraný"
#define MSGTR_Exit_quit "Koniec"
#define MSGTR_Exit_eof "Koniec súboru"
#define MSGTR_Fatal_error "Závažná chyba"
#define MSGTR_NoHomeDir "Nemôžem najsť domáci (HOME) adresár"
#define MSGTR_Playing "Prehrávam"
