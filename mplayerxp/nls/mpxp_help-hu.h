// Translated by:  Gabucino <gabucino@mplayerhq.hu>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (lásd DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Indítás:   mplayerxp [opciók] [útvonal/]filenév",
"",
"Opciók:",
" -vo <drv[:dev]> videomeghajtó és -alegység kiválasztása (lista: '-vo help')",
" -ao <drv[:dev]> audiomeghajtó és -alegység kiválasztása (lista: '-ao help')",
" -play.ss <időpoz>a megadott (másodperc v. óra:perc:mperc) pozícióra tekerés",
" -audio.off      hanglejátszás kikapcsolása",
" -video.fs       teljesképernyős lejátszás opciói (teljkép,módvált,szoft.nagy)",
" -sub.file <file>felhasználandó felirat-file megadása",
" -sync.framedrop képkockák eldobásának engedélyezése (lassú gépekhez)",
"",
"Billentyűk:",
" <-  vagy  ->    10 másodperces hátra/előre ugrás",
" fel vagy le     1 percnyi hátra/előre ugrás",
" pgup v. pgdown  10 percnyi hátra/előre ugrás",
" p vagy SPACE    pillanatállj (bármely billentyűre továbbmegy)",
" q vagy ESC      kilépés",
" o               OSD-mód váltása:  nincs / keresősáv / keresősáv+idő",
" * vagy /        hangerő fel/le ('m' billentyű master/pcm között vált)",
"",
" * * * A MANPAGE TOVÁBBI RÉSZLETEKET, OPCIÓKAT, BILLENTYŰKET TARTALMAZ ! * * *",
NULL
};
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "Kilépek"
#define MSGTR_Exit_frames "Kért számú képkocka lejátszásra került"
#define MSGTR_Exit_quit "Kilépés"
#define MSGTR_Exit_eof "Vége a file-nak"
#define MSGTR_Fatal_error "Végzetes hiba"
#define MSGTR_NoHomeDir "Nem találom a HOME konyvtárat"
#define MSGTR_Playing "lejátszása"
