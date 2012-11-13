// Translated by:  Daniel Beňa, benad@centrum.cz
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
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

#define MSGTR_Exiting "\nKončím... (%s)\n"
#define MSGTR_Exit_frames "Požadovaný počet snímkov prehraný"
#define MSGTR_Exit_quit "Koniec"
#define MSGTR_Exit_eof "Koniec súboru"
#define MSGTR_Exit_error "Závažná chyba"
#define MSGTR_IntBySignal "\nMPlayerXP prerušený signálom %d v module: %s \n"
#define MSGTR_NoHomeDir "Nemôžem najsť domáci (HOME) adresár\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problém\n"
#define MSGTR_CreatingCfgFile "Vytváram konfiguračný súbor: %s\n"
#define MSGTR_InvalidVOdriver "Neplatné meno výstupného videoovládača: %s\nPoužite '-vo help' pre zoznam dostupných ovládačov.\n"
#define MSGTR_InvalidAOdriver "Neplatné meno výstupného audioovládača: %s\nPoužite '-ao help' pre zoznam dostupných ovládačov.\n"
#define MSGTR_CopyCodecsConf "(copy/ln etc/codecs.conf (zo zdrojových kódov MPlayerXPu) do ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "Nemôžem načítať font: %s\n"
#define MSGTR_CantLoadSub "Nemôžem načítať titulky: %s\n"
#define MSGTR_ErrorDVDkey "Chyba pri spracovaní kľúča DVD.\n"
#define MSGTR_CmdlineDVDkey "DVD kľúč požadovaný na príkazovom riadku je uschovaný pre rozkódovanie.\n"
#define MSGTR_DVDauthOk "DVD sekvencia overenia autenticity vypadá v poriadku.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: požadovaný prúd chýba!\n"
#define MSGTR_CantOpenDumpfile "Nejde otvoriť súbor pre dump!!!\n"
#define MSGTR_CoreDumped "jadro vypísané :)\n"
#define MSGTR_FPSnotspecified "V hlavičke súboru nie je udané (alebo je zlé) FPS! Použite voľbu -fps !\n"
#define MSGTR_NoVideoStream "Žiaľ, žiadny videoprúd... to sa zatiaľ nedá prehrať.\n"
#define MSGTR_TryForceAudioFmt "Pokúšam sa vynútiť rodinu audiokodeku '%s' ...\n"
#define MSGTR_CantFindAfmtFallback "Nemôžem nájsť audio kodek pre požadovanú rodinu, použijem ostatné.\n"
#define MSGTR_CantFindAudioCodec "Nemôžem nájsť kodek pre audio formát"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Pokúste sa upgradovať %s z etc/codecs.conf\n*** Pokiaľ problém pretrvá, prečítajte si DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Nejde inicializovať audio kodek! -> bez zvuku\n"
#define MSGTR_TryForceVideoFmt "Pokúšam se vnútiť rodinu videokodeku '%s' ...\n"
#define MSGTR_CantFindVfmtFallback "Nemôžem najsť video kodek pre požadovanú rodinu, použijem ostatné.\n"
#define MSGTR_CantFindVideoCodec "Nemôžem najsť kodek pre video formát"
#define MSGTR_VOincompCodec "Žiaľ, vybrané video_out zariadenie je nekompatibilné s týmto kodekom.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Nemôžem inicializovať videokodek :(\n"
#define MSGTR_EncodeFileExists "Súbor už existuje: %s (neprepíšte si svoj obľúbený AVI súbor!)\n"
#define MSGTR_CantCreateEncodeFile "Nemôžem vytvoriť súbor pre encoding\n"
#define MSGTR_CannotInitVO "FATAL: Nemôžem inicializovať video driver!\n"
#define MSGTR_CannotInitAO "nemôžem otvoriť/inicializovať audio driver -> TICHO\n"
#define MSGTR_StartPlaying "Začínam prehrávať...\n"

#define MSGTR_Playing "Prehrávam %s\n"
#define MSGTR_NoSound "Audio: bez zvuku!!!\n"
#define MSGTR_FPSforced "FPS vnútené na hodnotu %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM zariadenie '%s' nenájdené!\n"
#define MSGTR_ErrTrackSelect "Chyba pri výbere VCD stopy!"
#define MSGTR_ReadSTDIN "Čítam z stdin...\n"
#define MSGTR_UnableOpenURL "Nejde otvoriť URL: %s\n"
#define MSGTR_ConnToServer "Pripojený k servru: %s\n"
#define MSGTR_FileNotFound "Súbor nenájdený: '%s'\n"

#define MSGTR_CantOpenDVD "Nejde otvoriť DVD zariadenie: %s\n"
#define MSGTR_DVDwait "Čítam štruktúru disku, prosím čakajte...\n"
#define MSGTR_DVDnumTitles "Na tomto DVD je %d titulov.\n"
#define MSGTR_DVDinvalidTitle "Neplatné číslo DVD titulu: %d\n"
#define MSGTR_DVDinvalidChapter "Neplatné číslo kapitoly DVD: %d\n"
#define MSGTR_DVDnumAngles "Na tomto DVD je %d úhlov pohľadov.\n"
#define MSGTR_DVDinvalidAngle "Neplatné číslo uhlu pohľadu DVD: %d\n"
#define MSGTR_DVDnoIFO "Nemôžem otvoriť súbor IFO pre DVD titul %d.\n"
#define MSGTR_DVDnoVOBs "Nemôžem otvoriť VOB súbor (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD úspešne otvorené!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Upozornenie! Hlavička audio prúdu %d predefinovaná!\n"
#define MSGTR_VideoStreamRedefined "Upozornenie! Hlavička video prúdu %d predefinovaná!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Príliš mnoho (%d v %d bajtech) audio paketov v bufferi!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Príliš mnoho (%d v %d bajtech) video paketov v bufferi!\n"
#define MSGTR_MaybeNI "(možno prehrávate neprekladaný prúd/súbor alebo kodek zlyhal)\n"
#define MSGTR_DetectedFILMfile "Detekovaný FILM formát súboru!\n"
#define MSGTR_DetectedFLIfile "Detekovaný FLI formát súboru!\n"
#define MSGTR_DetectedROQfile "Detekovaný ROQ formát súboru!\n"
#define MSGTR_DetectedREALfile "Detekovaný REAL formát súboru!\n"
#define MSGTR_DetectedAVIfile "Detekovaný AVI formát súboru!\n"
#define MSGTR_DetectedASFfile "Detekovaný ASF formát súboru!\n"
#define MSGTR_DetectedMPEGPESfile "Detekovaný MPEG-PES formát súboru!\n"
#define MSGTR_DetectedMPEGPSfile "Detekovaný MPEG-PS formát súboru!\n"
#define MSGTR_DetectedMPEGESfile "Detekovaný MPEG-ES formát súboru!\n"
#define MSGTR_DetectedQTMOVfile "Detekovaný QuickTime/MOV formát súboru!\n"
#define MSGTR_MissingMpegVideo "Chýbajúci MPEG video prúd!? kontaktujte autora, možno je to chyba (bug) :(\n"
#define MSGTR_InvalidMPEGES "Neplatný MPEG-ES prúd??? kontaktujte autora, možno je to chyba (bug) :(\n"
#define MSGTR_FormatNotRecognized "========== Žiaľ, tento formát súboru nie je rozpoznaný/podporovaný =======\n"\
				  "==== Pokiaľ je tento súbor AVI, ASF alebo MPEG prúd, kontaktujte autora! ====\n"
#define MSGTR_MissingVideoStream "Žiadny video prúd nenájdený!\n"
#define MSGTR_MissingAudioStream "Žiadny audio prúd nenájdený...  -> bez zvuku\n"
#define MSGTR_MissingVideoStreamBug "Chýbajúci video prúd!? Kontaktujte autora, možno to je chyba (bug) :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: súbor neobsahuje vybraný audio alebo video prúd\n"

#define MSGTR_NI_Forced "Vnútený"
#define MSGTR_NI_Detected "Detekovaný"
#define MSGTR_NI_Message "%s NEPREKLADANÝ formát súboru AVI!\n"

#define MSGTR_UsingNINI "Používam NEPREKLADANÝ poškodený formát súboru AVI!\n"
#define MSGTR_CouldntDetFNo "Nemôžem určiť počet snímkov (pre absolútny posun)  \n"
#define MSGTR_CantSeekRawAVI "Nemôžem sa posúvať v surových (raw) .AVI prúdoch! (Potrebujem index, zkuste použíť voľbu -idx !)  \n"
#define MSGTR_CantSeekFile "Nemôžem sa posúvať v tomto súbore!  \n"

#define MSGTR_MOVcomprhdr "MOV: Komprimované hlavičky nie sú (ešte) podporované!\n"
#define MSGTR_MOVvariableFourCC "MOV: Upozornenie! premenná FOURCC detekovaná!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Upozornenie! Príliš veľa stôp!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV formát nie je ešte podporovaný !!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nemôžem otvoriť kodek\n"
#define MSGTR_CantCloseCodec "nemôžem uzavieť kodek\n"

#define MSGTR_MissingDLLcodec "CHYBA: Nemôžem otvoriť potrebný DirectShow kodek: %s\n"
#define MSGTR_ACMiniterror "Nemôžem načítať/inicializovať Win32/ACM AUDIO kodek (chýbajúci súbor DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nemôžem najsť kodek '%s' v libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayerXP bol preložený BEZ podpory directshow!\n"
#define MSGTR_NoWfvSupport "Podpora pre kodeky win32 neaktívna alebo nedostupná mimo platformy x86!\n"
#define MSGTR_NoDivx4Support "MPlayerXP bol preložený BEZ podpory DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP bol preložený BEZ podpory ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Win32/ACM audio kodek neaktívny nebo nedostupný mimo platformy x86 -> bez zvuku :(\n"
#define MSGTR_NoDShowAudio "Preložené BEZ podpory DirectShow -> bez zvuku :(\n"
#define MSGTR_NoOggVorbis "OggVorbis audio kodek neaktívny -> bez zvuku :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP bol preložený BEZ podpory XAnim!\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF - koniec súboru v priebehu vyhľadávania hlavičky sekvencie\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Nemôžem prečítať hlavičku sekvencie!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Nemôžem prečítať rozšírenie hlavičky sekvencie!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Zlá hlavička sekvencie!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Zlé rozšírenie hlavičky sekvencie!\n"

#define MSGTR_ShMemAllocFail "Nemôžem alokovať zdieľanú pamäť\n"
#define MSGTR_CantAllocAudioBuf "Nemôžem alokovať pamäť pre výstupný audio buffer\n"
#define MSGTR_NoMemForDecodedImage "nedostatok pamäte pre buffer na dekódovanie obrazu (%ld bytes)\n"

#define MSGTR_AC3notvalid "Neplatný AC3 prúd.\n"
#define MSGTR_AC3only48k "Iba prúdy o frekvencii 48000 Hz sú podporované.\n"
#define MSGTR_UnknownAudio "Neznámy/chýbajúci audio formát -> bez zvuku\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Nastavujem podporu lirc ...\n"
#define MSGTR_LIRCdisabled "Nebudete môcť používať diaľkový ovládač.\n"
#define MSGTR_LIRCopenfailed "Zlyhal pokus o otvorenie podpory LIRC!\n"
#define MSGTR_LIRCsocketerr "Nejaká chyba so soketom lirc: %s\n"
#define MSGTR_LIRCcfgerr "Zlyhalo čítanie konfiguračného súboru LIRC %s !\n"
