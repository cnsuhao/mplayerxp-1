// Translated by:  Daniel Beňa, benad@centrum.cz
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (viď DOCS!)\n"
"\n";

// Preklad do slovenčiny

static char help_text[]=
"Použitie:   mplayerxp [prepínače] [cesta/]menosúboru\n"
"\n"
"Prepínače:\n"
" -vo <drv[:dev]> výber výstup. video ovládača&zariadenia (-vo help pre zoznam)\n"
" -ao <drv[:dev]> výber výstup. audio ovládača&zariadenia (-ao help pre zoznam)\n"
" -play.ss <timepos>posun na pozíciu (sekundy alebo hh:mm:ss)\n"
" -audio.off      prehrávať bez zvuku\n"
" -video.fs       voľby pre prehrávanie na celú obrazovku (celá obrazovka\n                 meniť videorežim, softvérový zoom)\n"
" -sub.file <file>voľba súboru s titulkami\n"
" -play.list<file> určenie súboru so zoznamom prehrávaných súborov\n"
" -sync.framedrop  povoliť zahadzovanie snímkov (pre pomalé stroje)\n"
"\n"
"Klávesy:\n"
" <-  alebo  ->   posun vzad/vpred o 10 sekund\n"
" hore / dole     posun vzad/vpred o  1 minútu\n"
" < alebo >       posun vzad/vpred v zozname prehrávaných súborov\n"
" p al. medzerník pauza pri prehrávaní (pokračovaní stlačením niektorej klávesy)\n"
" q alebo ESC     koniec prehrávania a ukončenie programu\n"
" o               cyklická zmena režimu OSD:  nič / pozícia / pozícia+čas\n"
" * alebo /       pridať alebo ubrať hlasitosť (stlačením 'm' výber master/pcm)\n"
"\n"
" * * * * PREČÍTAJTE SI MAN STRÁNKU PRE DETAILY (ĎALŠIE VOĽBY A KLÁVESY)! * * * *\n"
"\n";
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
