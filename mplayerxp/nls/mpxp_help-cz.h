// Translated by:  Jiri Svoboda, jiri.svoboda@seznam.cz
// Updated by:     Tomas Blaha,  tomas.blaha at kapsa.club.cz
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text[]={
"\n",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (viz DOCS!)",
NULL
};

// Překlad do češtiny Jiří Svoboda

static const char* help_text[]={
"",
"Použití:   mplayerxp [přepínače] [cesta/]jmenosouboru",
"",
"Přepínače:",
" -vo <drv[:dev]> výběr výstupního video ovladače&zařízení (-vo help pro seznam)",
" -ao <drv[:dev]> výběr výstupního audio ovladače&zařízení (-ao help pro seznam)",
" -play.ss <timepos> posun na pozici (sekundy nebo hh:mm:ss)",
" -audio.off      přehrávat beze zvuku",
" -video.fs       volby pro přehrávání přes celou obrazovku (celá obrazovka\n                 měnit videorežim, softwarový zoom)",
" -sub.file <file> volba souboru s titulky",
" -play.list <file>určení souboru se seznamem přehrávaných souborů",
" -sync.framedrop  povolit zahazování snímků (pro pomale stroje)",
"",
"Klávesy:",
" <-  nebo  ->    posun vzad/vpřed o 10 sekund",
" nahoru či dolů  posun vzad/vpřed o  1 minutu",
" < nebo >        posun vzad/vpřed v seznamu přehrávaných souborů",
" p nebo mezerník pauza při přehrávání (pokračování stiskem kterékoliv klávesy)",
" q nebo ESC      konec přehrávání a ukončení programu",
" o               cyklická změna režimu OSD:  nic / pozice / pozice+čas",
" * nebo /        přidat nebo ubrat hlasitost (stiskem 'm' výběr master/pcm)",
"",
" * * * * PŘEČTĚTE SI MAN STRÁNKU PRO DETAILY (DALŠÍ VOLBY A KLÁVESY)! * * * *",
NULL
};
#endif

// ========================= MPlayer messages ===========================

#define MSGTR_Exiting "Končím"
#define MSGTR_Exit_frames "Požadovaný počet snímků přehrán"
#define MSGTR_Exit_quit "Konec"
#define MSGTR_Exit_eof "Konec souboru"
#define MSGTR_Fatal_error "Závažná chyba"
#define MSGTR_NoHomeDir "Nemohu nalézt domácí (HOME) adresář"
#define MSGTR_Playing "Přehrávám"
