// Translated by:  Bohdan Horst <nexus@hoth.amu.edu.pl>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Użycie:   mplayerxp [opcje] [ścieżka/]nazwa\n"
"\n"
"Opcje:\n"
" -vo <drv[:dev]> wybór sterownika[:urządzenia] video (lista po '-vo help')\n"
" -ao <drv[:dev]> wybór sterownika[:urządzenia] audio (lista po '-ao help')\n"
" -play.ss <timepos>skok do podanej pozycji (sekundy albo hh:mm:ss)\n"
" -audio.off      odtwarzanie bez dźwięku\n"
" -video.fs       opcje pełnoekranowe (pełen ekran,zmiana trybu,skalowanie)\n"
" -sub.file <file>wybór pliku z napisami\n"
" -play.list<file>wybór pliku z playlistą\n"
" -sync.framedrop gubienie klatek (dla wolnych maszyn)\n"
"\n"
"Klawisze:\n"
" Right,Up,PgUp   skok naprzód o 10 sekund, 1 minutę, 10 minut\n"
" Left,Down,PgDn  skok do tyłu o 10 sekund, 1 minutę, 10 minut\n"
" < lub >         przeskok o jedną pozycję w playliście\n"
" p lub SPACE     zatrzymanie filmu (kontynuacja - dowolny klawisz)\n"
" q lub ESC       zatrzymanie odtwarzania i wyjście z programu\n"
" o               przełączanie trybów OSD: pusty / belka / belka i zegar\n"
" * lub /         zwiększenie lub zmniejszenie natężenia dźwięku\n"
"                 (naciśnij 'm' żeby wybrać master/pcm)\n"
" z lub x         regulacja opóźnienia napisów o +/- 0.1 sekundy\n"
"\n"
" **** DOKŁADNY SPIS WSZYSTKICH DOSTĘPNYCH OPCJI ZNAJDUJE SIĘ W MANUALU! ****\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nWychodzę... (%s)\n"
#define MSGTR_Exit_frames "Zadana liczba klatek odtworzona"
#define MSGTR_Exit_quit "Wyjście"
#define MSGTR_Exit_eof "Koniec pliku"
#define MSGTR_Exit_error "Błąd krytyczny"
#define MSGTR_IntBySignal "\nMPlayerXP przerwany sygnałem %d w module: %s \n"
#define MSGTR_NoHomeDir "Nie mogę znaleźć katalogu HOME\n"
#define MSGTR_GetpathProblem "problem z get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Stwarzam plik z konfiguracją: %s\n"
#define MSGTR_InvalidVOdriver "Nieprawidłowa nazwa sterownika video: %s\nUżyj '-vo help' aby dostać listę dostępnych streowników video.\n"
#define MSGTR_InvalidAOdriver "Nieprawidłowa nazwa sterownika audio: %s\nUżyj '-ao help' aby dostać listę dostępnych sterowników audio.\n"
#define MSGTR_CopyCodecsConf "(skopiuj/zlinkuj etc/codecs.conf do ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "Nie mogę załadować fontu: %s\n"
#define MSGTR_CantLoadSub "Nie mogę załadować napisów: %s\n"
#define MSGTR_ErrorDVDkey "Błąd w przetwarzaniu DVD KEY.\n"
#define MSGTR_CmdlineDVDkey "Linia komend DVD wymaga zapisanego klucza do descramblingu.\n"
#define MSGTR_DVDauthOk "Sekwencja autoryzacji DVD wygląda OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: nie ma wybranego strumienia!\n"
#define MSGTR_CantOpenDumpfile "Nie mogę otworzyć pliku dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS nie podane (lub błędne) w nagłówku! Użyj opcji -fps!\n"
#define MSGTR_NoVideoStream "Przepraszam, brak strumienia video... nie działa to na razie\n"
#define MSGTR_TryForceAudioFmt "Wymuszam zastosowanie kodeka audio z rodziny '%s' ...\n"
#define MSGTR_CantFindAfmtFallback "Nie mogę znaleźć kodeka audio dla wymuszonej rodziny, wracam do standardowych.\n"
#define MSGTR_CantFindAudioCodec "Nie mogę znaleźć kodeka dla formatu audio"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Spróbuj uaktualnić %s etc/codecs.conf\n*** Jeśli to nie pomaga, przeczytaj DOCS/codecs.html !\n"
#define MSGTR_CouldntInitAudioCodec "Nie moge zainicjować sterownika audio! -> nosound\n"
#define MSGTR_TryForceVideoFmt "Wymuszam zastosowanie kodeka video z rodziny '%s' ...\n"
#define MSGTR_CantFindVfmtFallback "Nie mogę znaleźć kodeka video dla wymuszonej rodziny, wracam do standardowych..\n"
#define MSGTR_CantFindVideoCodec "Nie mogę znaleźć kodeka dla formatu video"
#define MSGTR_VOincompCodec "Przepraszam, wybrany sterownik video_out jest niekompatybilny z tym kodekiem.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Nie mogę zainicjować kodeka video :(\n"
#define MSGTR_EncodeFileExists "Plik już istnieje: %s (nie nadpisz swojego ulubionego AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Nie mogę stworzyć pliku do zakodowania\n"
#define MSGTR_CannotInitVO "FATAL: Nie mogę zainicjować sterownika video!\n"
#define MSGTR_CannotInitAO "Nie mogę otworzyć/zainicjować urządzenia audio -> NOSOUND\n"
#define MSGTR_StartPlaying "Początek odtwarzania...\n"

#define MSGTR_Playing "Odtwarzam %s\n"
#define MSGTR_NoSound "Audio: brak dźwięku!!!\n"
#define MSGTR_FPSforced "FPS wymuszone na %5.3f  (ftime: %5.3f)\n"

// open.c:, stream.c
#define MSGTR_CdDevNotfound "Urządzenie CD-ROM '%s' nie znalezione!\n"
#define MSGTR_ErrTrackSelect "Błąd wyboru ścieżki VCD!"
#define MSGTR_ReadSTDIN "Odczytuję ze stdin...\n"
#define MSGTR_UnableOpenURL "Nie mogę otworzyć URL: %s\n"
#define MSGTR_ConnToServer "Połączony z serwerem: %s\n"
#define MSGTR_FileNotFound "Plik nie znaleziony: '%s'\n"

#define MSGTR_CantOpenDVD "Nie mogę otworzyć urządzenia DVD: %s\n"
#define MSGTR_DVDwait "Odczytuję strukturę dysku, proszę czekać...\n"
#define MSGTR_DVDnumTitles "Na tym DVD znajduje się %d tytułów.\n"
#define MSGTR_DVDinvalidTitle "Nieprawidłowy numer tytułu DVD: %d\n"
#define MSGTR_DVDinvalidChapter "Nieprawidłowy numer rozdziału DVD: %d\n"
#define MSGTR_DVDnumAngles "W tym tytule DVD znajduje się %d ustawień kamery.\n"
#define MSGTR_DVDinvalidAngle "Nieprawidłowy numer ustawienia kamery DVD: %d\n"
#define MSGTR_DVDnoIFO "Nie mogę otworzyć pliku IFO dla tytułu DVD %d.\n"
#define MSGTR_DVDnoVOBs "Nie mogę otworzyć tytułu VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD otwarte poprawnie!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Uwaga! Nagłówek strumienia audio %d przedefiniowany!\n"
#define MSGTR_VideoStreamRedefined "Uwaga! Nagłówek strumienia video %d przedefiniowany!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Zbyt wiele (%d w %d bajtach) pakietów audio w buforze!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Zbyt wiele (%d w %d bajtach) pakietów video w buforze!\n"
#define MSGTR_MaybeNI "(może odtwarzasz strumień/plik non-interleaved lub kodek nie zadziałał)\n"
#define MSGTR_DetectedFILMfile "Wykryto format FILM!\n"
#define MSGTR_DetectedFLIfile "Wykryto format FLI!\n"
#define MSGTR_DetectedROQfile "Wykryto format RoQ!\n"
#define MSGTR_DetectedREALfile "Wykryto format REAL!\n"
#define MSGTR_DetectedAVIfile "Wykryto format AVI!\n"
#define MSGTR_DetectedASFfile "Wykryto format ASF!\n"
#define MSGTR_DetectedMPEGPESfile "Wykryto format MPEG-PES!\n"
#define MSGTR_DetectedMPEGPSfile "Wykryto format MPEG-PS!\n"
#define MSGTR_DetectedMPEGESfile "Wykryto format MPEG-ES!\n"
#define MSGTR_DetectedQTMOVfile "Wykryto format QuickTime/MOV!\n"
#define MSGTR_MissingMpegVideo "Zagubiony strumień video MPEG !? skontaktuj się z autorem, może to błąd:(\n"
#define MSGTR_InvalidMPEGES "Błędny strumień MPEG-ES ??? skontaktuj się z autorem, może to błąd:(\n"
#define MSGTR_FormatNotRecognized "========== Przepraszam,  format pliku nierozpoznany/nieobsługiwany ==========\n"\
				  "=== Jeśli to strumień AVI, ASF lub MPEG, proszę skontaktuj się z autorem! ===\n"
#define MSGTR_MissingVideoStream "Nie znaleziono strumienia video!\n"
#define MSGTR_MissingAudioStream "Nie znaleziono strumienia audio... -> nosound\n"
#define MSGTR_MissingVideoStreamBug "Zgubiony strumień video!? skontaktuj się z autorem, może to błąd:(\n"

#define MSGTR_DoesntContainSelectedStream "demux: plik nie zawiera wybranego strumienia audio lub video\n"

#define MSGTR_NI_Forced "Wymuszony"
#define MSGTR_NI_Detected "Wykryty"
#define MSGTR_NI_Message "%s format pliku NON-INTERLEAVED AVI !\n"

#define MSGTR_UsingNINI "Używa uszkodzonego formatu pliku NON-INTERLEAVED AVI !\n"
#define MSGTR_CouldntDetFNo "Nie mogę określić liczby klatek (dla przeszukiwania)\n"
#define MSGTR_CantSeekRawAVI "Nie mogę przeszukiwać nieindeksowanych strumieni .AVI! (sprawdź opcję -idx !)\n"
#define MSGTR_CantSeekFile "Nie mogę przeszukiwać tego pliku!  \n"

#define MSGTR_MOVcomprhdr "MOV: Spakowane nagłówki nie są obsługiwane (na razie)!\n"
#define MSGTR_MOVvariableFourCC "MOV: Uwaga! wykryto zmienną FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Uwaga! zbyt dużo scieżek!"
#define MSGTR_MOVnotyetsupp "\n**** Format Quicktime MOV nie jest na razie obsługiwany !!!!!!! ****\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nie mogę otworzyć kodeka\n"
#define MSGTR_CantCloseCodec "nie mogę zamknąć kodeka\n"

#define MSGTR_MissingDLLcodec "ERROR: Nie mogę otworzyć wymaganego kodeka DirectShow: %s\n"
#define MSGTR_ACMiniterror "Nie mogę załadować/zainicjalizować kodeka Win32/ACM AUDIO (brakuje pliku DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nie moge znaleźć w libavcodec kodeka '%s' ...\n"

#define MSGTR_NoDShowSupport "MPlayerXP skompilowany BEZ obsługi directshow!\n"
#define MSGTR_NoWfvSupport "Obsługa kodeków win32 wyłączona lub niedostępna na platformach nie-x86!\n"
#define MSGTR_NoDivx4Support "MPlayerXP skompilowany BEZ obsługi DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP skompilowany BEZ obsługi ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Kodek audio Win32/ACM wyłączony lub niedostępny dla nie-x86 CPU -> wymuszam brak dźwięku :(\n"
#define MSGTR_NoDShowAudio "Skompilowane bez obsługi DirectShow -> wymuszam brak dźwięku :(\n"
#define MSGTR_NoOggVorbis "Kodek audio OggVorbis wyłączony -> wymuszam brak dźwięku :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP skompilowany BEZ obsługi XAnim!\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF podczas przeszukiwania nagłówka sekwencji\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Nie mogę odczytać nagłówka sekwencji!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Nie mogę odczytać rozszerzenia nagłówka sekwencji!!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Nieprawidłowy nagłówek sekwencji!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Nieprawidłowe rozszerzenie nagłówka sekwencji!\n"

#define MSGTR_ShMemAllocFail "Nie mogę zaalokować pamięci dzielonej\n"
#define MSGTR_OutOfMemory "brak pamięci\n"
#define MSGTR_CantAllocAudioBuf "Nie mogę zaalokować buforu wyjściowego audio\n"
#define MSGTR_NoMemForDecodedImage "Za mało pamięci dla zdekodowanego bufora obrazu (%ld bajtów)\n"

#define MSGTR_AC3notvalid "Nieprawidłowy strumień AC3.\n"
#define MSGTR_AC3only48k "Obsługiwane są tylko strumienie 48000 Hz.\n"
#define MSGTR_UnknownAudio "Nieznany/zgubiony format audio, nie używam dźwięku\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Włączam obsługę lirc...\n"
#define MSGTR_LIRCdisabled "Nie będziesz mógł używać twojego pilota\n"
#define MSGTR_LIRCopenfailed "Nieudane otwarcie obsługi lirc!\n"
#define MSGTR_LIRCsocketerr "Coś jest nie tak z socketem lirc: %s\n"
#define MSGTR_LIRCcfgerr "Nieudane odczytanie pliku konfiguracyjnego LIRC %s !\n"
