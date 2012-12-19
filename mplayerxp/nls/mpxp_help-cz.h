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

// mplayer.c:

#define MSGTR_Exiting "Končím"
#define MSGTR_Exit_frames "Požadovaný počet snímků přehrán"
#define MSGTR_Exit_quit "Konec"
#define MSGTR_Exit_eof "Konec souboru"
#define MSGTR_Exit_error "Závažná chyba"
#define MSGTR_IntBySignal "\nMPlayerXP přerušen signálem %d v modulu: %s \n"
#define MSGTR_NoHomeDir "Nemohu nalézt domácí (HOME) adresář\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problém\n"
#define MSGTR_CreatingCfgFile "Vytvářím konfigurační soubor"
#define MSGTR_InvalidVOdriver "Neplané jméno výstupního videoovladače"
#define MSGTR_InvalidAOdriver "Neplané jméno výstupního audioovladače"
#define MSGTR_CopyCodecsConf "(copy/ln etc/codecs.conf (ze zdrojových kódů MPlayerXPu) do ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "Nemohu načíst font"
#define MSGTR_CantLoadSub "Nemohu načíst titulky"
#define MSGTR_ErrorDVDkey "Chyba při zpracování klíče DVD.\n"
#define MSGTR_CmdlineDVDkey "DVD klíč požadovaný na příkazové řádce je uschován pro rozkódování.\n"
#define MSGTR_DVDauthOk "DVD autentikační sekvence vypadá vpořádku.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: požadovaný proud chybí!\n"
#define MSGTR_CantOpenDumpfile "Nelze otevřít soubor pro dump!!!\n"
#define MSGTR_CoreDumped "jádro vypsáno :)\n"
#define MSGTR_FPSnotspecified "V hlavičce souboru není udáno (nebo je špatné) FPS! Použijte volbu -fps !\n"
#define MSGTR_NoVideoStream "Bohužel, žádný videoproud... to se zatím nedá přehrát.\n"
#define MSGTR_TryForceAudioFmt "Pokouším se vynutit rodinu audiokodeku"
#define MSGTR_CantFindAfmtFallback "Nemohu nalézt audio kodek pro požadovanou rodinu, použiji ostatní.\n"
#define MSGTR_CantFindAudioCodec "Nemohu nalézt kodek pro audio formát"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Pokuste se upgradovat %s z etc/codecs.conf\n*** Pokud problém přetrvá, pak si přečtěte DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Nelze inicializovat audio kodek! -> beze zvuku\n"
#define MSGTR_TryForceVideoFmt "Pokuším se vynutit rodinu videokodeku"
#define MSGTR_CantFindVfmtFallback "Nemohu nalézt video kodek pro požadovanou rodinu, použiji ostatní.\n"
#define MSGTR_CantFindVideoCodec "Nemohu nalézt kodek pro video formát"
#define MSGTR_VOincompCodec "Bohužel, vybrané video_out zařízení je nekompatibilní s tímto kodekem.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Nemohu inicializovat videokodek :(\n"
#define MSGTR_EncodeFileExists "Soubor již existuje: %s (nepřepište si svůj oblíbený AVI soubor!)\n"
#define MSGTR_CantCreateEncodeFile "Nemohu vytvořit soubor\n" // toto doopravit - need to be corrected
#define MSGTR_CannotInitVO "FATAL: Nemohu inicializovat video driver!\n"
#define MSGTR_CannotInitAO "nemohu otevřít/inicializovat audio driver -> TICHO\n"
#define MSGTR_StartPlaying "Začínám přehrávat...\n"

#define MSGTR_Playing "Přehrávám"
#define MSGTR_NoSound "Audio: beze zvuku!!!\n"
#define MSGTR_FPSforced "FPS vynuceno na hodnotu %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM zařízení '%s' nenalezeno!\n"
#define MSGTR_ErrTrackSelect "Chyba při výběru VCD stopy!"
#define MSGTR_ReadSTDIN "Čtu ze stdin...\n"
#define MSGTR_UnableOpenURL "Nelze otevřít URL: %s\n"
#define MSGTR_ConnToServer "Připojen k serveru: %s\n"
#define MSGTR_FileNotFound "Soubor nenalezen: '%s'\n"

#define MSGTR_CantOpenDVD "Nelze otevřít DVD zařízení: %s\n"
#define MSGTR_DVDwait "Čtu strukturu disku, prosím čekejte...\n"
#define MSGTR_DVDnumTitles "Na tomto DVD je %d titulů.\n"
#define MSGTR_DVDinvalidTitle "Neplatné číslo DVD titulu: %d\n"
#define MSGTR_DVDinvalidChapter "Neplatné číslo kapitoly DVD: %d\n"
#define MSGTR_DVDnumAngles "Na tomto DVD je %d úhlů pohledu.\n"
#define MSGTR_DVDinvalidAngle "Neplatné číslo úhlu pohledu DVD: %d\n"
#define MSGTR_DVDnoIFO "Nemohu otevřít soubor IFO pro DVD titul %d.\n"
#define MSGTR_DVDnoVOBs "Nemohu otevřít VOB soubor (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD úspěšně otevřeno!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Upozornění! Hlavička audio proudu %d předefinována!\n"
#define MSGTR_VideoStreamRedefined "Upozornění! Hlavička video proudu %d předefinována!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Příliš mnoho (%d v %d bajtech) audio paketů v bufferu!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Příliš mnoho (%d v %d bajtech) video paketů v bufferu!\n"
#define MSGTR_MaybeNI "(možná přehráváte neprokládaný proud/soubor nebo kodek selhal)\n"
#define MSGTR_DetectedFILMfile "Detekován FILM formát souboru!\n"
#define MSGTR_DetectedFLIfile "Detekován FLI formát souboru!\n"
#define MSGTR_DetectedROQfile "Detekován RoQ formát souboru!\n"
#define MSGTR_DetectedREALfile "Detekován REAL formát souboru!\n"
#define MSGTR_DetectedAVIfile "Detekován AVI formát souboru!\n"
#define MSGTR_DetectedASFfile "Detekován ASF formát souboru!\n"
#define MSGTR_DetectedMPEGPESfile "Detekován MPEG-PES formát souboru!\n"
#define MSGTR_DetectedMPEGPSfile "Detekován MPEG-PS formát souboru!\n"
#define MSGTR_DetectedMPEGESfile "Detekován MPEG-ES formát souboru!\n"
#define MSGTR_DetectedQTMOVfile "Detekován QuickTime/MOV formát souboru!\n"
#define MSGTR_MissingMpegVideo "Chybějící MPEG video proud!? Kontaktujte autora, možná to je chyba (bug) :(\n"
#define MSGTR_InvalidMPEGES "Neplatný MPEG-ES proud!? Kontaktuje autora, možná to je chyba (bug) :(\n"
#define MSGTR_FormatNotRecognized "========== Bohužel, tento formát souboru není rozpoznán/podporován =========\n"\
				 "==== Pokud je tento soubor AVI, ASF nebo MPEG proud, kontaktuje autora! ====\n"
#define MSGTR_MissingVideoStream "Žádný video proud nenalezen!\n"
#define MSGTR_MissingAudioStream "Žádný audio proud nenalezen...  ->beze zvuku\n"
#define MSGTR_MissingVideoStreamBug "Chybějící video proud!? Kontaktuje autora, možná to je chyba (bug) :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: soubor neobsahuje vybraný audio nebo video proud\n"

#define MSGTR_NI_Forced "Vynucen"
#define MSGTR_NI_Detected "Detekován"
#define MSGTR_NI_Message "%s NEPROKLÁDANÝ formát souboru AVI!\n"

#define MSGTR_UsingNINI "Používám NEPROKLÁDANÝ poškozený formát souboru AVI!\n" //tohle taky nějak opravit
#define MSGTR_CouldntDetFNo "Nemohu určit počet snímků (pro SOFní posun)  \n"
#define MSGTR_CantSeekRawAVI "Nelze se posouvat v surových (raw) .AVI proudech! (Potřebuji index, zkuste použít volbu -idx !)  \n"
#define MSGTR_CantSeekFile "Nemohu posouvat v tomto souboru!  \n"

#define MSGTR_MOVcomprhdr "MOV: Komprimované hlavičky nejsou (ještě) podporovány!\n"
#define MSGTR_MOVvariableFourCC "MOV: Upozornění! proměnná FOURCC detekována!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Upozornění! Příliš mnoho stop!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV formát není ještě podporován !!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nemohu otevřít kodek\n"
#define MSGTR_CantCloseCodec "nemohu uzavřít kodek\n"

#define MSGTR_MissingDLLcodec "CHYBA: Nemohu otevřít potřebný DirectShow kodek: %s\n"
#define MSGTR_ACMiniterror "Nemohu načíst/inicializovat Win32/ACM AUDIO kodek (chybějící soubor DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nemohu najít kodek '%s' v libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayerXP byl přeložen BEZ podpory directshow!\n"
#define MSGTR_NoWfvSupport "Podpora pro kodeky win32 neaktivní nebo nedostupná mimo platformy x86!\n"
#define MSGTR_NoDivx4Support "MPlayerXP byl přeložen BEZ podpory DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP byl přeložen BEZ podpory lavc/libavcodec!\n"
#define MSGTR_NoACMSupport "Win32/ACM audio kodek neaktivní nebo nedostupný mimo platformy x86 -> vynuceno beze zvuku :(\n"
#define MSGTR_NoDShowAudio "Přeloženo BEZ podpory DirectShow -> vynuceno beze zvuku :(\n"
#define MSGTR_NoOggVorbis "OggVorbis audio kodek neaktivní -> vynuceno beze zvuku :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP byl přeložen BEZ podpory XAnim!\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF - konec souboru v průběhu vyhledávání hlavičky sekvence\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Nelze přečíst hlavičku sekvence!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Nelze přečíst rozšíření hlavičky sekvence!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Špatná hlavička sekvence!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Špatné rozšíření hlavičky sekvence!\n"

#define MSGTR_ShMemAllocFail "Nemohu alokovat sdílenou paměť\n"
#define MSGTR_OutOfMemory "nedostatek paměti\n"
#define MSGTR_CantAllocAudioBuf "Nemohu alokovat paměť pro výstupní audio buffer\n"
#define MSGTR_NoMemForDecodedImage "nedostatek paměti pro buffer pro dekódování obrazu (%ld bytes)\n"

#define MSGTR_AC3notvalid "Neplatný AC3 proud.\n"
#define MSGTR_AC3only48k "Pouze proudy o frekvenci 48000 Hz podporovány.\n"
#define MSGTR_UnknownAudio "Neznámý/chybějící audio formát -> beze zvuku\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Nastavuji podporu lirc ...\n"
#define MSGTR_LIRCdisabled "Nebudete moci používat dálkový ovladač.\n"
#define MSGTR_LIRCopenfailed "Selhal pokus o otevření podpory LIRC!\n"
#define MSGTR_LIRCsocketerr "Nějaká chyba se soketem lirc: %s\n"
#define MSGTR_LIRCcfgerr "Selhalo čtení konfiguračního souboru LIRC %s !\n"
