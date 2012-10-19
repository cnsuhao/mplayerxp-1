// Translated by: Fabio Olimpieri <fabio.olimpieri@tin.it>

// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
// and send a notify message to mplayer-dev-eng maillist.
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
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

#define MSGTR_Exiting "\nIn uscita... (%s)\n"
#define MSGTR_Exit_frames "Numero di fotogrammi riprodotti richiesti"
#define MSGTR_Exit_quit "Uscita"
#define MSGTR_Exit_eof "Fine del file"
#define MSGTR_Exit_error "Errore fatale"
#define MSGTR_IntBySignal "\nMPlayerXP interrotto dal segnale %d nel modulo: %s \n"
#define MSGTR_NoHomeDir "Impossibile trovare la HOME directory\n"
#define MSGTR_GetpathProblem "Problema in get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Creo il file di configurazione: %s\n"
#define MSGTR_InvalidVOdriver "Nome del diver video di output non valido: %s\nUsa '-vo help' per avere una lista dei driver video disponibili.\n"
#define MSGTR_InvalidAOdriver "Nome del diver audio di output non valido: %s\nUsa '-ao help' per avere una lista dei driver audio disponibili.\n"
#define MSGTR_CopyCodecsConf "(copia/collega etc/codecs.conf (dall\'albero dei sorgenti di MPlayerXP) a ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "Impossibile caricare i font: %s\n"
#define MSGTR_CantLoadSub "Impossibile caricare i sottotitoli: %s\n"
#define MSGTR_ErrorDVDkey "Errore di elaborazione della chiave del DVD.\n"
#define MSGTR_CmdlineDVDkey "La chiave del DVD richiesta nella riga di comando è immagazzinata per il descrambling.\n"
#define MSGTR_DVDauthOk "La sequenza di autorizzazione del DVD sembra essere corretta.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: manca il flusso selezionato!\n"
#define MSGTR_CantOpenDumpfile "Impossibile aprire il file di dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS non specificato (o non valido) nell\'intestazione! Usa l\'opzione -fps !\n"
#define MSGTR_NoVideoStream "Mi dispiace, niente flusso video... Non si può ancora riprodurre\n"
#define MSGTR_TryForceAudioFmt "Cerco di forzare l\'uso della famiglia dei driver dei codec audio '%s' ...\n"
#define MSGTR_CantFindAfmtFallback "Impossibile trovare i codec audio per la famiglia dei driver richiesta, torno agli altri driver.\n"
#define MSGTR_CantFindAudioCodec "Impossibile trovare il codec per il formato audio"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Prova ad aggiornare %s da etc/codecs.conf\n*** Se non va ancora bene, allora leggi DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Impossibile inizializzare il codec audio! -> nessun suono\n"
#define MSGTR_TryForceVideoFmt "Cerco di forzare l\'uso della famiglia dei driver dei codec video '%s' ...\n"
#define MSGTR_CantFindVfmtFallback "Impossibile trovare i codec video per la famiglia dei driver richiesta, torno agli altri driver.\n"
#define MSGTR_CantFindVideoCodec "Impossibile trovare il codec per il formato video"
#define MSGTR_VOincompCodec "Mi dispiace, il dispositivo di video_out selezionato è incompatibile con questo codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Impossibile inizializzare il codec video :(\n"
#define MSGTR_EncodeFileExists "Il file già esiste: %s (non sovrascrivere il tuo AVI preferito!)\n"
#define MSGTR_CantCreateEncodeFile "Impossibile creare il file per la codifica\n"
#define MSGTR_CannotInitVO "FATAL: Impossibile inizializzare il driver video!\n"
#define MSGTR_CannotInitAO "Impossibile aprire/inizializzare il dispositivo audio -> NESSUN SUONO\n"
#define MSGTR_StartPlaying "Inizio la riproduzione...\n"

#define MSGTR_Playing "In riproduzione %s\n"
#define MSGTR_NoSound "Audio: nessun suono!!!\n"
#define MSGTR_FPSforced "FPS forzato a %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispositivo CD-ROM '%s' non trovato!\n"
#define MSGTR_ErrTrackSelect "Errore nella selezione della traccia del VCD!"
#define MSGTR_ReadSTDIN "Leggo da stdin...\n"
#define MSGTR_UnableOpenURL "Impossibile aprire la URL: %s\n"
#define MSGTR_ConnToServer "Connesso al server: %s\n"
#define MSGTR_FileNotFound "File non trovato: '%s'\n"

#define MSGTR_CantOpenDVD "Impossibile aprire il dispositivo DVD: %s\n"
#define MSGTR_DVDwait "Leggo la struttura del disco, per favore aspetta...\n"
#define MSGTR_DVDnumTitles "Ci sono %d titoli su questo DVD.\n"
#define MSGTR_DVDinvalidTitle "Numero del titolo del DVD non valido: %d\n"
#define MSGTR_DVDinvalidChapter "Numero del capitolo del DVD non valido: %d\n"
#define MSGTR_DVDnumAngles "Ci sono %d angolature in questo titolo del DVD.\n"
#define MSGTR_DVDinvalidAngle "Numero delle angolature del DVD non valido: %d\n"
#define MSGTR_DVDnoIFO "Impossibile aprire il file IFO per il titolo del DVD %d.\n"
#define MSGTR_DVDnoVOBs "Impossibile aprire il titolo VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD aperto con successo!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Avvertimento! Intestazione del flusso audio %d ridefinito!\n"
#define MSGTR_VideoStreamRedefined "Avvertimento! Intestazione del flusso video %d ridefinito!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Troppi (%d in %d byte) pacchetti audio nel buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Troppi (%d in %d byte) pacchetti video nel buffer!\n"
#define MSGTR_MaybeNI "(forse stai riproducendo un flusso/file non interlacciato o il codec non funziona)\n"
#define MSGTR_DetectedFILMfile "Rilevato formato file FILM !\n"
#define MSGTR_DetectedFLIfile "Rilevato formato file FLI !\n"
#define MSGTR_DetectedROQfile "Rilevato formato file RoQ !\n"
#define MSGTR_DetectedREALfile "Rilevato formato file REAL !\n"
#define MSGTR_DetectedAVIfile "Rilevato formato file AVI !\n"
#define MSGTR_DetectedASFfile "Rilevato formato file ASF !\n"
#define MSGTR_DetectedMPEGPESfile "Rilevato formato file MPEG-PES !\n"
#define MSGTR_DetectedMPEGPSfile "Rilevato formato file MPEG-PS !\n"
#define MSGTR_DetectedMPEGESfile "Rilevato formato file MPEG-ES !\n"
#define MSGTR_DetectedQTMOVfile "Rilevato formato file QuickTime/MOV !\n"
#define MSGTR_MissingMpegVideo "Manca il flusso video MPEG!? Contatta l\'autore, può essere un baco :(\n"
#define MSGTR_InvalidMPEGES "Flusso MPEG-ES non valido??? Contatta l\'autore, può essere un baco :(\n"
#define MSGTR_FormatNotRecognized "===== Mi dispiace, questo formato file non è riconosciuto/supportato ======\n"\
				  "=== Se questo è un file AVI, ASF o MPEG, per favore contatta l\'autore! ===\n"
#define MSGTR_MissingVideoStream "Nessun flusso video trovato!\n"
#define MSGTR_MissingAudioStream "Nessun flusso audio trovato...  ->nessun suono\n"
#define MSGTR_MissingVideoStreamBug "Manca il flusso video!? Contatta l\'autore, può essere un baco :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: il file non contiene il flusso audio o video selezionato\n"

#define MSGTR_NI_Forced "Forzato"
#define MSGTR_NI_Detected "Rilevato"
#define MSGTR_NI_Message "%s formato file AVI NON-INTERLACCIATO!\n"

#define MSGTR_UsingNINI "Uso di formato file AVI NON-INTERLACCIATO corrotto!\n"
#define MSGTR_CouldntDetFNo "Impossibile determinare il numero di fotogrammi (per lo spostamento in valore assoluto)  \n"
#define MSGTR_CantSeekRawAVI "Impossibile spostarsi nei flussi .AVI grezzi! (richiesto un indice, prova con l\'opzione -idx !)  \n"
#define MSGTR_CantSeekFile "Impossibile spostarsi in questo file!  \n"

#define MSGTR_MOVcomprhdr "MOV: Intestazioni compresse non (ancora) supportate!\n"
#define MSGTR_MOVvariableFourCC "MOV: Avvertimento! Rilevata variabile FOURCC !?\n"
#define MSGTR_MOVtooManyTrk "MOV: Avvertimento! troppe tracce!"
#define MSGTR_MOVnotyetsupp "\n****** Formato Quicktime MOV non ancora supportato!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "impossibile aprire il codec\n"
#define MSGTR_CantCloseCodec "impossibile chiudere il codec\n"

#define MSGTR_MissingDLLcodec "ERRORE: Impossibile aprire il codec DirectShow richiesto: %s\n"
#define MSGTR_ACMiniterror "Impossibile caricare/inizializzare il codec audio Win32/ACM (manca il file DLL ?)\n"
#define MSGTR_MissingLAVCcodec "Impossibile trovare il codec '%s' in libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayerXP è stato compilato SENZA il supporto per Directshow!\n"
#define MSGTR_NoWfvSupport "Supporto per i codec win32 disabilitato o non disponibile sulle piattaforme non-x86 !\n"
#define MSGTR_NoDivx4Support "MPlayerXP è stato compilato SENZA il supporto per DivX4Linux (libdivxdecore.so) !\n"
#define MSGTR_NoLAVCsupport "MPlayerXP è stato compilato SENZA il supporto per ffmpeg/libavcodec \n"
#define MSGTR_NoACMSupport "Audio codec Win32/ACM disabilitato o non disponibile sulle CPU non-x86 -> forzo a no audio :(\n"
#define MSGTR_NoDShowAudio "Compilato senza il supporto per DirectShow -> forzo a no audio :(\n"
#define MSGTR_NoOggVorbis "Audio codec OggVorbis disabilitato -> forzo a no audio :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP è stato compilato SENZA il supporto per XAnim !\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF mentre cercavo la sequenza di intestazione\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Impossibile leggere la sequenza di intestazione!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Impossibile leggere l\'estensione della sequenza di intestazione!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Sequenza di intestazione non valida!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Estensione della sequenza di intestazione non valida!\n"

#define MSGTR_ShMemAllocFail "Impossibile allocare la memoria condivisa\n"
#define MSGTR_CantAllocAudioBuf "Impossibile allocare il buffer di uscita dell\'audio\n"
#define MSGTR_NoMemForDecodedImage "memoria insufficiente per il buffer di decodifica dell\'immagine (%ld byte)\n"

#define MSGTR_AC3notvalid "Flusso AC3 non valido.\n"
#define MSGTR_AC3only48k "Supportati solo flussi a 48000 Hz.\n"
#define MSGTR_UnknownAudio "Formato audio sconosciuto/mancante, non uso l\'audio\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Configurazione del supporto per lirc...\n"
#define MSGTR_LIRCdisabled "Non potrai usare il tuo telecomando\n"
#define MSGTR_LIRCopenfailed "Apertura del supporto per lirc fallita!\n"
#define MSGTR_LIRCsocketerr "C'è qualcosa che non va nel socket di lirc: %s\n"
#define MSGTR_LIRCcfgerr "Fallimento nella lettura del file di configurazione di LIRC %s !\n"
