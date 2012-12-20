// Translated by:  Jiri Svoboda, jiri.svoboda@seznam.cz
// Updated by:     Tomas Blaha,  tomas.blaha at kapsa.club.cz
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (viz DOCS!)\n"
"\n";

// Překlad do češtiny Jiří Svoboda

static char help_text[]=
"Použití:   mplayerxp [přepínače] [cesta/]jmenosouboru\n"
"\n"
"Přepínače:\n"
" -vo <drv[:dev]> výběr výstupního video ovladače&zařízení (-vo help pro seznam)\n"
" -ao <drv[:dev]> výběr výstupního audio ovladače&zařízení (-ao help pro seznam)\n"
" -play.ss <timepos> posun na pozici (sekundy nebo hh:mm:ss)\n"
" -audio.off      přehrávat beze zvuku\n"
" -video.fs       volby pro přehrávání přes celou obrazovku (celá obrazovka\n                 měnit videorežim, softwarový zoom)\n"
" -sub.file <file> volba souboru s titulky\n"
" -play.list <file>určení souboru se seznamem přehrávaných souborů\n"
" -sync.framedrop  povolit zahazování snímků (pro pomale stroje)\n"
"\n"
"Klávesy:\n"
" <-  nebo  ->    posun vzad/vpřed o 10 sekund\n"
" nahoru či dolů  posun vzad/vpřed o  1 minutu\n"
" < nebo >        posun vzad/vpřed v seznamu přehrávaných souborů\n"
" p nebo mezerník pauza při přehrávání (pokračování stiskem kterékoliv klávesy)\n"
" q nebo ESC      konec přehrávání a ukončení programu\n"
" o               cyklická změna režimu OSD:  nic / pozice / pozice+čas\n"
" * nebo /        přidat nebo ubrat hlasitost (stiskem 'm' výběr master/pcm)\n"
"\n"
" * * * * PŘEČTĚTE SI MAN STRÁNKU PRO DETAILY (DALŠÍ VOLBY A KLÁVESY)! * * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

#define MSGTR_Exiting "Končím"
#define MSGTR_Exit_frames "Požadovaný počet snímků přehrán"
#define MSGTR_Exit_quit "Konec"
#define MSGTR_Exit_eof "Konec souboru"
#define MSGTR_Fatal_error "Závažná chyba"
#define MSGTR_NoHomeDir "Nemohu nalézt domácí (HOME) adresář"
#define MSGTR_Playing "Přehrávám"
