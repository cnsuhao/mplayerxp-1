// Translated by: Fabio Olimpieri <fabio.olimpieri@tin.it>

// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
// and send a notify message to mplayer-dev-eng maillist.
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (vedi DOCS!)\n"
"\n";

static char help_text[]=
"Uso:   mplayerxp [opzioni] [percorso/]nome_file\n"
"\n"
"Opzioni:\n"
" -vo <drv[:dev]> seleziona il driver ed il dispositivo video di output ('-vo help' per la lista)\n"
" -ao <drv[:dev]> seleziona il driver ed il dispositivo audio di output ('-ao help' per la lista)\n"
" -play.ss <timepos>cerca una determinata posizione (in secondi o in hh:mm:ss) \n"
" -audio.off      non riproduce l\'audio\n"
" -video.fs       opzioni di riproduzione a schermo intero (schermo int,cambia video,scalatura softw)\n"
" -sub.file <file>specifica il file dei sottotitoli da usare\n"
" -play.list<file> specifica il file della playlist\n"
" -sync.framedrop abilita lo scarto dei fotogrammi (per macchine lente)\n"
"\n"
"Tasti:\n"
" <-  o  ->       va indietro/avanti di 10 secondi\n"
" su o giù        va indietro/avanti di 1 minuto\n"
" < o >           va indietro/avanti nella playlist\n"
" p o SPAZIO      mette in pausa il filmato (premere un qualunque tasto per continuare)\n"
" q o ESC         ferma la riproduzione ed esce dal programma\n"
" o               cambia tra le modalità OSD: niente / barra di ricerca / barra di ricerca + tempo\n"
" * o /           incrementa o decrementa il volume (premere 'm' per selezionare master/pcm)\n"
"\n"
" * * * VEDI LA PAGINA MAN PER DETTAGLI, ULTERIORI OPZIONI AVANZATE E TASTI ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "In uscita"
#define MSGTR_Exit_frames "Numero di fotogrammi riprodotti richiesti"
#define MSGTR_Exit_quit "Uscita"
#define MSGTR_Exit_eof "Fine del file"
#define MSGTR_Fatal_error "Errore fatale"
#define MSGTR_NoHomeDir "Impossibile trovare la HOME directory"
#define MSGTR_Playing "In riproduzione"
