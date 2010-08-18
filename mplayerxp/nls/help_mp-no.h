// Transated by:  Andreas Berntsen  <andreasb@kvarteret.org>
// Updated for 0.60 by: B. Johannessen <bob@well.com>
// UTF-8
// ========================= MPlayer hjelp ===========================

#ifdef HELP_MP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (se DOCS!)\n"
"\n";

static char help_text[]=
"Bruk:    mplayerxp [valg] [sti/]filnavn\n"
"\n"
"Valg:\n"
" -vo <drv[:dev]> velg video-ut driver og enhet (se '-vo help' for liste)\n"
" -ao <drv[:dev]> velg lyd-ut driver og enhet (se '-ao help' for liste)\n"
" -play.ss <timepos>søk til gitt (sekunder eller hh:mm:ss) posisjon\n"
" -audio.off      ikke spill av lyd\n"
" -video.fs       fullskjerm avspillings valg (fullscr,vidmode chg,softw.scale)\n"
" -sub.file <fil> spesifiser hvilken subtitle fil som skal brukes\n"
" -sync.framedrop slå på bilde-dropping (for trege maskiner)\n"
"\n"
"Tastatur:\n"
" <- eller ->       søk bakover/fremover 10 sekunder\n"
" opp eller ned     søk bakover/fremover 1 minutt\n"
" < or >            søk bakover/fremover i playlisten\n"
" p eller MELLOMROM pause filmen (trykk en tast for å fortsette)\n"
" q eller ESC       stopp avspilling og avslutt programmet\n"
" + eller -         juster lyd-forsinkelse med +/- 0.1 sekund\n"
" o                 gå gjennom OSD modi:  ingen / søkelinje / søkelinje+tidsvisning\n"
" * eller /         øk eller mink volumet (trykk 'm' for å velge master/pcm)\n"
" z or x            juster undertittelens forsinkelse med +/- 0.1 sekund\n"
"\n"
" * * * SE PÅ MANSIDE FOR DETALJER, FLERE (AVANSERTE) VALG OG TASTER! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nAvslutter... (%s)\n"
#define MSGTR_Exit_frames "Antall forespurte bilder vist"
#define MSGTR_Exit_quit "Avslutt"
#define MSGTR_Exit_eof "Slutt på filen"
#define MSGTR_Exit_error "Fatal feil"
#define MSGTR_IntBySignal "\nMPlayerXP avbrutt av signal %d i modul: %s \n"
#define MSGTR_NoHomeDir "Kan ikke finne HOME katalog\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problem\n"
#define MSGTR_CreatingCfgFile "Oppretter konfigurasjonsfil: %s\n"
#define MSGTR_InvalidVOdriver "Ugyldig video-ut drivernavn: %s\nBruk '-vo help' for en liste over mulige video-drivere.\n"
#define MSGTR_InvalidAOdriver "Ugyldig lyd-ut drivernavn: %s\nBruk '-ao help' for en liste over mulige lyd-ut drivere.\n"
#define MSGTR_CopyCodecsConf "(kopier eller link etc/codecs.conf (fra MPlayerXP kildekode) til ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "Kan ikke laste skrifttype: %s\n"
#define MSGTR_CantLoadSub "Kan ikke laste undertitler: %s\n"
#define MSGTR_ErrorDVDkey "Feil under bearbeiding av DVD KEY.\n"
#define MSGTR_CmdlineDVDkey "Etter spurte DVD kommandolinje nøkkel er lagret for descrambling.\n"
#define MSGTR_DVDauthOk "DVD auth sekvense ser ut til å være OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATALT: valgte stream mangler!\n"
#define MSGTR_CantOpenDumpfile "Kan ikke åpne dump fil!!!\n"
#define MSGTR_CoreDumped "core dumpet :)\n"
#define MSGTR_FPSnotspecified "FPS ikke spesifisert (eller ugyldig) i headeren! Bruk -fps valget!\n"
#define MSGTR_NoVideoStream "Sorry, ingen video stream... ikke spillbar for øyeblikket\n"
#define MSGTR_TryForceAudioFmt "Prøver å tvinge lyd-codec driver familie '%s' ...\n"
#define MSGTR_CantFindAfmtFallback "Kan ikke finne lyd-codec for tvunget driver familie, faller tilbake til andre drivere.\n"
#define MSGTR_CantFindAudioCodec "Kan ikke finne codec for lydformat"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Prøv å oppgrader %s fra etc/codecs.conf\n*** Hvis det fortsatt ikke virker, les DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Greide ikke å initialisere lyd-codec! -> nosound\n"
#define MSGTR_TryForceVideoFmt "Prøver å tvingte video-codec driver familie '%s' ...\n"
#define MSGTR_CantFindVfmtFallback "Kan ikke finne video-codec for tvungen driver familie, faller tilbake til andre drivere.\n"
#define MSGTR_CantFindVideoCodec "Kan ikke finne codec for videoformat"
#define MSGTR_VOincompCodec "Desverre, valgt video_out enhet er inkompatibel med denne codec'en.\n"
#define MSGTR_CouldntInitVideoCodec "FATALT: Kan ikke initialisere video codec :(\n"
#define MSGTR_EncodeFileExists "Filen finnes allerede: %s (ikke overskriv favoritt AVI'en din!)\n"
#define MSGTR_CantCreateEncodeFile "Kan ikke opprette fil for koding\n"
#define MSGTR_CannotInitVO "FATALT: Kan ikke initialisere video driver!\n"
#define MSGTR_CannotInitAO "kunne ikke åpne/initialisere lyd-enhet -> NOSOUND\n"
#define MSGTR_StartPlaying "Starter avspilling...\n"

#define MSGTR_Playing "Spiller %s\n"
#define MSGTR_NoSound "Lyd: ingen lyd!!!\n"
#define MSGTR_FPSforced "FPS tvunget til %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM enhet '%s' ikke funnet!\n"
#define MSGTR_ErrTrackSelect "Feil under valg av VCD spor!"
#define MSGTR_ReadSTDIN "Leser fra stdin...\n"
#define MSGTR_UnableOpenURL "Kan ikke åpne URL: %s\n"
#define MSGTR_ConnToServer "Koblet til server: %s\n"
#define MSGTR_FileNotFound "Finner ikke filen: '%s'\n"

#define MSGTR_CantOpenDVD "Kan ikke åpne DVD enhet: %s\n"
#define MSGTR_DVDwait "Leser disk-struktur, vennligst vent...\n"
#define MSGTR_DVDnumTitles "Det er %d titler på denne DVD.\n"
#define MSGTR_DVDinvalidTitle "Ugyldig DVD tittelnummer: %d\n"
#define MSGTR_DVDinvalidChapter "Ugyldig DVD kapittelnummer: %d\n"
#define MSGTR_DVDnumAngles "Det er %d vinkler i denne DVD tittelen.\n"
#define MSGTR_DVDinvalidAngle "Ugyldig DVD vinkel nummer: %d\n"
#define MSGTR_DVDnoIFO "Kan ikke åpne IFO filen for DVD tittel %d.\n"
#define MSGTR_DVDnoVOBs "Kan ikke åpne VOBS tittel (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD åpnet ok!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Advarsel! lyd stream header %d redefinert!\n"
#define MSGTR_VideoStreamRedefined "Advarsel! video stream header %d redefinert!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: For mange (%d i %d bytes) lyd pakker i bufferen!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: For mange (%d i %d bytes) video pakker i bufferen!\n"
#define MSGTR_MaybeNI "(kanskje du spiller av en ikke-interleaved stream/fil eller codec'en feilet)\n"
#define MSGTR_DetectedFILMfile "Detekterte FILM filformat!\n"
#define MSGTR_DetectedFLIfile "Detekterte FLI filformat!\n"
#define MSGTR_DetectedROQfile "Detekterte RoQ filformat!\n"
#define MSGTR_DetectedREALfile "Detekterte REAL filformat!\n"
#define MSGTR_DetectedAVIfile "Detekterte AVI filformat!\n"
#define MSGTR_DetectedASFfile "Detekterte ASF filformat!\n"
#define MSGTR_DetectedMPEGPESfile "Detected MPEG-PES filformat!\n"
#define MSGTR_DetectedMPEGPSfile "Detekterte MPEG-PS filformat!\n"
#define MSGTR_DetectedMPEGESfile "Detekterte MPEG-ES filformat!\n"
#define MSGTR_DetectedQTMOVfile "Detekterte QuickTime/MOV filformat!\n"
#define MSGTR_MissingMpegVideo "Manglende MPEG video stream!? kontakt utvikleren, det kan være en feil :(\n"
#define MSGTR_InvalidMPEGES "Ugyldig MPEG-ES stream??? kontakt utvikleren, det kan være en feil :(\n"
#define MSGTR_FormatNotRecognized "======== Beklager, dette filformatet er ikke gjenkjent/støttet ===============\n"\
				  "=== Hvis det er en AVI, ASF eller MPEG stream, kontakt utvikleren! ===\n"
#define MSGTR_MissingVideoStream "Ingen video stream funnet!\n"
#define MSGTR_MissingAudioStream "Ingen lyd stream funnet...  ->nosound\n"
#define MSGTR_MissingVideoStreamBug "Manglende video stream!? Kontakt utvikleren, det kan være en  feil :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: filen inneholder ikke valgte lyd eller video stream\n"

#define MSGTR_NI_Forced "Tvunget"
#define MSGTR_NI_Detected "Detekterte"
#define MSGTR_NI_Message "%s IKKE-INTERLEAVED AVI filformat!\n"

#define MSGTR_UsingNINI "Bruker NON-INTERLEAVED Ødelagt AVI filformat!\n"
#define MSGTR_CouldntDetFNo "Kan ikke bestemme antall frames (for SOF søk)  \n"
#define MSGTR_CantSeekRawAVI "Kan ikke søke i rå .AVI streams! (index behøves, prøv med -idx valget!)  \n"
#define MSGTR_CantSeekFile "Kan ikke søke i denne filen!  \n"

#define MSGTR_EncryptedVOB "Kryptert VOB fil (ikke kompilert med libcss støtte)! Les filen DOCS/DVD\n"
#define MSGTR_EncryptedVOBauth "Kryptert stream men autentikasjon var ikke forespurt av deg!!\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimerte headere ikke støttet (enda)!\n"
#define MSGTR_MOVvariableFourCC "MOV: Advarsel! variabel FOURCC detektert!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Advarsel! for mange sport!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV format ikke støttet enda!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "kunne ikke åpne codec\n"
#define MSGTR_CantCloseCodec "kunne ikke lukke codec\n"

#define MSGTR_MissingDLLcodec "FEIL: Kunne ikke åpne nødvendig DirectShow codec: %s\n"
#define MSGTR_ACMiniterror "Kunne ikke laste/initialisere Win32/ACM AUDIO codec (manglende DLL fil?)\n"
#define MSGTR_MissingLAVCcodec "Kan ikke finne codec '%s' i libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayerXP ble kompilert UTEN directshow støtte!\n"
#define MSGTR_NoWfvSupport "Støtte for win32 codecs slått av, eller ikke tilgjengelig på ikke-x86 plattformer!\n"
#define MSGTR_NoDivx4Support "MPlayerXP ble kompilert UTEN DivX4Linux (libdivxdecore.so) støtte!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP ble kompilert UTEN ffmpeg/libavcodec støtte!\n"
#define MSGTR_NoACMSupport "Win32/ACM lyd codec slått av eller ikke tilgjengelig på ikke-x86 CPU -> tvinger nosound :(\n"
#define MSGTR_NoDShowAudio "Kompilert uten DirectShow støtte -> tvinger nosound :(\n"
#define MSGTR_NoOggVorbis "OggVorbis lyd codec slått av -> tvinger nosound :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP er kompilert uten XAnim støtte!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATALT: EOF under søking etter sekvens header\n"
#define MSGTR_CannotReadMpegSequHdr "FATALT: Kan ikke lese sekvens header!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATALT: Kan ikke lese sekvens header tillegg!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Feil i sekvens header!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Feil i sekvens header tillegg!\n"

#define MSGTR_ShMemAllocFail "Kan ikke allokere delt minne\n"
#define MSGTR_CantAllocAudioBuf "Kan ikke allokere lyd-ut buffer\n"
#define MSGTR_NoMemForDecodedImage "ikke nok minne for dekodet bilde buffer (%ld bytes)\n"

#define MSGTR_AC3notvalid "AC3 stream ikke riktig.\n"
#define MSGTR_AC3only48k "Bare 48000 Hz streams støttet.\n"
#define MSGTR_UnknownAudio "Ukjent/manglende lydformat, bruker nosound\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Setter opp lirc støtte...\n"
#define MSGTR_LIRCdisabled "Du vil ikke kunne bruke fjernkontrollen din\n"
#define MSGTR_LIRCopenfailed "Feil under åpning av lirc!\n"
#define MSGTR_LIRCsocketerr "Det er feil i lirc socket: %s\n"
#define MSGTR_LIRCcfgerr "Feil under lesing av lirc konfigurasjonsfil %s !\n"
