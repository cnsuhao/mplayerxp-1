// Translated by:  Gabucino <gabucino@mplayerhq.hu>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
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
" z vagy x        felirat késleltetése +/- 0.1 másodperccel\n"
"\n"
" * * * A MANPAGE TOVÁBBI RÉSZLETEKET, OPCIÓKAT, BILLENTYŰKET TARTALMAZ ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nKilépek... (%s)\n"
#define MSGTR_Exit_frames "Kért számú képkocka lejátszásra került"
#define MSGTR_Exit_quit "Kilépés"
#define MSGTR_Exit_eof "Vége a file-nak"
#define MSGTR_Exit_error "Végzetes hiba"
#define MSGTR_IntBySignal "\nAz MPlayerXP futása a %s modulban kapott %d szignál miatt megszakadt \n"
#define MSGTR_NoHomeDir "Nem találom a HOME konyvtárat\n"
#define MSGTR_GetpathProblem "get_path(\"config\") probléma\n"
#define MSGTR_CreatingCfgFile "Konfigurációs file létrehozása: %s\n"
#define MSGTR_InvalidVOdriver "Nem létező video drivernév: %s\nHasználd a '-vo help' opciót, hogy listát kapj a használhato vo meghajtókról.\n"
#define MSGTR_InvalidAOdriver "Nem létező audio drivernév: %s\nHasználd az '-ao help' opciót, hogy listát kapj a használhato ao meghajtókról.\n"
#define MSGTR_CopyCodecsConf "(másold/linkeld az etc/codecs.conf file-t ~/.mplayerxp/codecs.conf-ba)\n"
#define MSGTR_CantLoadFont "Nem tudom betölteni a következő fontot: %s\n"
#define MSGTR_CantLoadSub "Nem tudom betölteni a feliratot: %s\n"
#define MSGTR_ErrorDVDkey "Hiba a DVD-KULCS feldolgozása közben.\n"
#define MSGTR_CmdlineDVDkey "A parancssorban megadott DVD-kulcs további dekódolás céljából eltárolásra került.\n"
#define MSGTR_DVDauthOk "DVD-autentikációs folyamat, úgy tünik, sikerrel végződött.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: VÉGZETES HIBA: a kért stream nem található!\n"
#define MSGTR_CantOpenDumpfile "Nem tudom megnyitni a dump file-t!\n"
#define MSGTR_CoreDumped "Kinyomattam a cuccost, jól.\n"
#define MSGTR_FPSnotspecified "Az FPS (képkocka/mp) érték nincs megadva, vagy hibás! Használd az -fps opciót!\n"
#define MSGTR_NoVideoStream "Ebben nincs video stream... egyelőre lejátszhatatlan\n"
#define MSGTR_TryForceAudioFmt "Megpróbálom a(z) '%s' audio codec-családot használni ...\n"
#define MSGTR_CantFindAfmtFallback "A megadott audio codec-családban nem találtam idevaló meghajtót, próbálkozok más meghajtóval.\n"
#define MSGTR_CantFindAudioCodec "Nem találok codecet a(z) audio-formátumhoz:"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Frissítsd a %s-t az etc/codecs.conf-ból\n*** Ha még mindig nem jó, olvasd el a DOCS/CODECS-et!\n"
#define MSGTR_CouldntInitAudioCodec "Nem tudom indítani az audio codecet! -> nincshang ;)\n"
#define MSGTR_TryForceVideoFmt "Megpróbálom a(z) '%s' video codec-családot használni ...\n"
#define MSGTR_CantFindVfmtFallback "A megadott video codec-családban nem találtam idevaló meghajtót, próbálkozok más meghajtóval.\n"
#define MSGTR_CantFindVideoCodec "Nem találok codecet a(z) 0x%X video-formátumhoz"
#define MSGTR_VOincompCodec "A kiválasztott video_out meghajtó inkompatibilis ezzel a codec-kel.\n"
#define MSGTR_CouldntInitVideoCodec "VÉGZETES HIBA: Nem sikerült a video codecet elindítani :(\n"
#define MSGTR_EncodeFileExists "A %s file már létezik (nehogy letöröld a kedvenc AVI-dat!)\n"
#define MSGTR_CantCreateEncodeFile "Nem tudom enkódolás céljából létrehozni a filet\n"
#define MSGTR_CannotInitVO "VÉGZETES HIBA: Nem tudom elindítani a video-meghajtót!\n"
#define MSGTR_CannotInitAO "nem tudom megnyitni az audio-egységet -> NOSOUND\n"
#define MSGTR_StartPlaying "Lejátszás indítása...\n"

#define MSGTR_Playing "%s lejátszása\n"
#define MSGTR_NoSound "Audio: nincs hang!!!\n"
#define MSGTR_FPSforced "FPS kényszerítve %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "A CD-ROM meghajtó (%s) nem található!\n"
#define MSGTR_ErrTrackSelect "Hiba a VCD-sáv kiválasztásakor!"
#define MSGTR_ReadSTDIN "Olvasás a szabványos bemenetről (stdin)...\n"
#define MSGTR_UnableOpenURL "Nem megnyitható az URL: %s\n"
#define MSGTR_ConnToServer "Csatlakozom a szerverhez: %s\n"
#define MSGTR_FileNotFound "A file nem található: '%s'\n"

#define MSGTR_CantOpenDVD "Nem tudom megnyitni a DVD eszközt: %s\n"
#define MSGTR_DVDwait "A lemez struktúrájának olvasása, kérlek várj...\n"
#define MSGTR_DVDnumTitles "%d sáv van a DVD-n.\n"
#define MSGTR_DVDinvalidTitle "Helytelen DVD sáv: %d\n"
#define MSGTR_DVDinvalidChapter "Helytelen DVD fejezet: %d\n"
#define MSGTR_DVDnumAngles "%d darab kameraállás van ezen a DVD sávon.\n"
#define MSGTR_DVDinvalidAngle "Helytelen DVD kameraállás: %d\n"
#define MSGTR_DVDnoIFO "Nem tudom a(z) %d. DVD sávhoz megnyitni az IFO file-t.\n"
#define MSGTR_DVDnoVOBs "Nem tudom megnyitni a sávot (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD sikeresen megnyitva!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Vigyázat! Többszörösen definiált Audio-folyam! (Hibás file?)\n"
#define MSGTR_VideoStreamRedefined "Vigyázat! Többszörösen definiált Video-folyam! (Hibás file?)\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) audio-csomag a pufferben!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) video-csomag a pufferben!\n"
#define MSGTR_MaybeNI "(talán ez egy nem összefésült file vagy a CODEC nem működik jól)\n"
#define MSGTR_DetectedFILMfile "Ez egy FILM formátumú file!\n"
#define MSGTR_DetectedFLIfile "Ez egy FLI formátumú file!\n"
#define MSGTR_DetectedROQfile "Ez egy RoQ formátumú file!\n"
#define MSGTR_DetectedREALfile "Ez egy REAL formátumú file!\n"
#define MSGTR_DetectedAVIfile "Ez egy AVI formátumú file!\n"
#define MSGTR_DetectedASFfile "Ez egy ASF formátumú file!\n"
#define MSGTR_DetectedMPEGPESfile "Ez egy MPEG-PES formátumú file!\n"
#define MSGTR_DetectedMPEGPSfile "Ez egy MPEG-PS formátumú file!\n"
#define MSGTR_DetectedMPEGESfile "Ez egy MPEG-ES formátumú file!\n"
#define MSGTR_DetectedQTMOVfile "Ez egy QuickTime/MOV formátumú file! (ez még nem támogatott)\n"
#define MSGTR_MissingMpegVideo "Nincs MPEG video-folyam? Lépj kapcsolatba a készítőkkel, lehet, hogy hiba!\n"
#define MSGTR_InvalidMPEGES "Hibás MPEG-ES-folyam? Lépj kapcsolatba a készítőkkel, lehet, hogy hiba!\n"
#define MSGTR_FormatNotRecognized "========= Sajnos ez a fileformátum ismeretlen vagy nem támogatott ===========\n"\
				  "= Ha ez egy AVI, ASF vagy MPEG file, lépj kapcsolatba a készítőkkel (hiba)! =\n"
#define MSGTR_MissingVideoStream "Nincs képfolyam!\n"
#define MSGTR_MissingAudioStream "Nincs hangfolyam... -> hang nélkül\n"
#define MSGTR_MissingVideoStreamBug "Nincs képfolyam?! Írj a szerzőnek, lehet hogy hiba :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: a file nem tartalmazza a kért hang vagy kép folyamot\n"

#define MSGTR_NI_Forced "Kényszerítve"
#define MSGTR_NI_Detected "Detektálva"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI formátum!\n"

#define MSGTR_UsingNINI "NON-INTERLEAVED hibás AVI formátum használata!\n"
#define MSGTR_CouldntDetFNo "Nem tudom meghatározni a képkockák számát (abszolut tekeréshez)   \n"
#define MSGTR_CantSeekRawAVI "Nem tudok nyers .AVI-kban tekerni! (index kell, próbáld az -idx kapcsolóval!)\n"
#define MSGTR_CantSeekFile "Nem tudok ebben a fileban tekerni!  \n"

#define MSGTR_EncryptedVOB "Kódolt VOB file (libcss támogatás nincs befordítva!) Olvasd el a doksit\n"
#define MSGTR_EncryptedVOBauth "Kódolt folyam, de nem kértél autentikálást!!\n"

#define MSGTR_MOVcomprhdr "MOV: Tömörített fejlécek (még) nincsenek támogatva!\n"
#define MSGTR_MOVvariableFourCC "MOV: Vigyázat! változó FOURCC detektálva!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Vigyázat! túl sok sáv!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV formátum még nincs támogatva!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nem tudom megnyitni a kodeket\n"
#define MSGTR_CantCloseCodec "nem tudom lezárni a kodeket\n"

#define MSGTR_MissingDLLcodec "HIBA: Nem tudom megnyitni a kért DirectShow kodeket: %s\n"
#define MSGTR_ACMiniterror "Nem tudom betölteni/inicializálni a Win32/ACM kodeket (hiányzó DLL file?)\n"
#define MSGTR_MissingLAVCcodec "Nem találom a(z) '%s' nevű kodeket a libavcodec-ben...\n"

#define MSGTR_NoDShowSupport "Az MPlayerXP DirectShow támogatás NÉLKÜL lett fordítva!\n"
#define MSGTR_NoWfvSupport "A win32-es kodekek támogatása ki van kapcsolva, vagy nem létezik nem-x86-on!\n"
#define MSGTR_NoDivx4Support "Az MPlayerXP DivX4Linux támogatás (libdivxdecore.so) NÉLKÜL lett fordítva!\n"
#define MSGTR_NoLAVCsupport "Az MPlayerXP ffmpeg/libavcodec támogatás NÉLKÜL lett fordítva!\n"
#define MSGTR_NoACMSupport "Win32/ACM hang kodek támogatás ki van kapcsolva, vagy nem létezik nem-x86 CPU-n -> hang kikapcsolva :(\n"
#define MSGTR_NoDShowAudio "DirectShow támogatás nincs lefordítva -> hang kikapcsolva :(\n"
#define MSGTR_NoOggVorbis "OggVorbis hang kodek kikapcsolva -> hang kikapcsolva :(\n"

#define MSGTR_MpegNoSequHdr "MPEG: VÉGZETES: vége lett a filenak miközben a szekvencia fejlécet kerestem\n"
#define MSGTR_CannotReadMpegSequHdr "VÉGZETES: Nem tudom olvasni a szekvencia fejlécet!\n"
#define MSGTR_CannotReadMpegSequHdrEx "VÉGZETES: Nem tudom olvasni a szekvencia fejléc kiterjesztését!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Hibás szekvencia fejléc!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Hibás szekvencia fejléc kiterjesztés!\n"

#define MSGTR_ShMemAllocFail "Nem tudok megosztott memóriát lefoglalni\n"
#define MSGTR_OutOfMemory "elfogyott a memória\n"
#define MSGTR_CantAllocAudioBuf "Nem tudok kimeneti hangbuffer lefoglalni\n"
#define MSGTR_NoMemForDecodedImage "nincs elég memória a dekódolt képhez (%ld bájt)\n"

#define MSGTR_AC3notvalid "AC3 folyam hibás.\n"
#define MSGTR_AC3only48k "Csak 48000 Hz-es folyamok vannak támogatva.\n"
#define MSGTR_UnknownAudio "Ismeretlen/hiányzó hangformátum, hang kikapcsolva\n"

// LIRC:
#define MSGTR_SettingUpLIRC "lirc támogatás indítása...\n"
#define MSGTR_LIRCdisabled "Nem fogod tudni használni a távirányítót\n"
#define MSGTR_LIRCopenfailed "Nem tudtam megnyitni a lirc támogatást!\n"
#define MSGTR_LIRCsocketerr "Valami baj van a lirc socket-tel: %s\n"
#define MSGTR_LIRCcfgerr "Nem tudom olvasni a LIRC konfigurációs file-t : %s \n"
