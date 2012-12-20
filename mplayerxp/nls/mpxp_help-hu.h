// Translated by:  Gabucino <gabucino@mplayerhq.hu>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (lásd DOCS!)\n"
"\n";

static char help_text[]=
"Indítás:   mplayerxp [opciók] [útvonal/]filenév\n"
"\n"
"Opciók:\n"
" -vo <drv[:dev]> videomeghajtó és -alegység kiválasztása (lista: '-vo help')\n"
" -ao <drv[:dev]> audiomeghajtó és -alegység kiválasztása (lista: '-ao help')\n"
" -play.ss <időpoz>a megadott (másodperc v. óra:perc:mperc) pozícióra tekerés\n"
" -audio.off      hanglejátszás kikapcsolása\n"
" -video.fs       teljesképernyős lejátszás opciói (teljkép,módvált,szoft.nagy)\n"
" -sub.file <file>felhasználandó felirat-file megadása\n"
" -sync.framedrop képkockák eldobásának engedélyezése (lassú gépekhez)\n"
"\n"
"Billentyűk:\n"
" <-  vagy  ->    10 másodperces hátra/előre ugrás\n"
" fel vagy le     1 percnyi hátra/előre ugrás\n"
" pgup v. pgdown  10 percnyi hátra/előre ugrás\n"
" p vagy SPACE    pillanatállj (bármely billentyűre továbbmegy)\n"
" q vagy ESC      kilépés\n"
" o               OSD-mód váltása:  nincs / keresősáv / keresősáv+idő\n"
" * vagy /        hangerő fel/le ('m' billentyű master/pcm között vált)\n"
"\n"
" * * * A MANPAGE TOVÁBBI RÉSZLETEKET, OPCIÓKAT, BILLENTYŰKET TARTALMAZ ! * * *\n"
"\n";
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
