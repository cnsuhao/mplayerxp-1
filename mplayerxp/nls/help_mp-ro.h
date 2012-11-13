// Translated by:  Codre Adrian <codreadrian@softhome.net>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Folosire:   mplayerxp [opţiuni] [cale/]fişier\n"
"\n"
"Opţiuni:\n"
" -vo <drv[:disp]> Ieşirea video: driver&dispozitiv ('-vo help' pentru o listă)\n"
" -ao <drv[:disp]> Ieşirea audio: driver&dispozitiv ('-ao help' pentru o listă)\n"
" -play.ss <poziţia>sare la poziţia (secunde sau oo:mm:ss)\n"
" -audio.off       fără sunet\n"
" -video.fs       mod tot ecranul (tot ecranul,schimbă modul,scalat prin software)\n"
" -sub.file <fişier>specifică fişierul cu subtitrări\n"
" -sync.framedrop activează săritul cadrelor (pentru calculatoare lente)\n"
"\n"
"Taste:\n"
" <-  sau  ->      caută faţă/spate cu 10 secunde\n"
" sus sau jos      caută faţă/spate cu 1 minut\n"
" p sau SPACE      pune filmul pe pauză (orice tastă pentru a continua)\n"
" q sau ESC        opreşte filmul şi iese din program\n"
" o                roteşte modurile OSD: nimic / bară progres / bară progres+ceas\n"
" * sau /          creşte sau scade volumul (apăsaţi 'm' pentru principal/wav)\n"
"\n"
" * * * VEDEŢI MANUALUL PENTRU DETALII,(ALTE) OPŢIUNI AVANSATE ŞI TASTE ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nIes... (%s)\n"
#define MSGTR_Exit_frames "Numărul de cadre cerut a fost redat"
#define MSGTR_Exit_quit "Ieşire"
#define MSGTR_Exit_eof "Sfârşitul fişierului"
#define MSGTR_Exit_error "Eroare fatală"
#define MSGTR_IntBySignal "\nMPlayerXP a fost intrerupt de semnalul %d în modulul: %s \n"
#define MSGTR_NoHomeDir "Nu găsesc directorul HOME\n"
#define MSGTR_GetpathProblem "get_path(\"config\") cu probleme\n"
#define MSGTR_CreatingCfgFile "Creez fişierul de configurare: %s\n"
#define MSGTR_InvalidVOdriver "Ieşire video invalidă: %s\nFolosiţi '-vo help' pentru o listă de ieşiri video disponibile.\n"
#define MSGTR_InvalidAOdriver "Ieşire audio invalidă: %s\nFolosiţi '-ao help' pentru o listă de ieşiri audio disponibile.\n"
#define MSGTR_CopyCodecsConf "(copiaţi etc/codecs.conf (din directorul sursă MPlayerXP) în  ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "Nu pot incărca fontul: %s\n"
#define MSGTR_CantLoadSub "Nu pot incarcă subtitrarea: %s\n"
#define MSGTR_ErrorDVDkey "Eroare la procesarea cheii DVD.\n"
#define MSGTR_CmdlineDVDkey "Cheia DVD specificată în linia de comandă este păstrată pentru decodificare.\n"
#define MSGTR_DVDauthOk "Secvenţa de autentificare DVD pare să fie OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATALA: pista selectată lipseşte!\n"
#define MSGTR_CantOpenDumpfile "Nu pot deschide fişierul (dump)!!!\n"
#define MSGTR_CoreDumped "core aruncat :)\n"
#define MSGTR_FPSnotspecified "FPS nespecificat (sau invalid) în antet! Folosiţi opţiunea -fps!\n"
#define MSGTR_NoVideoStream "Îmi pare rău, nici o pistă video... este de neafişat încă\n"
#define MSGTR_TryForceAudioFmt "Încerc să forţez utilizarea unui codec audio din familia '%s' ...\n"
#define MSGTR_CantFindAfmtFallback "Nu pot să găsesc un codec audio pentru familia forţată, revin la alte drivere.\n"
#define MSGTR_CantFindAudioCodec "Nu găsesc un codec audio pentru formatul"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Încercaţi să înnoiţi %s din etc/codecs.conf\n*** Dacă nu ajută citiţi DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Nu pot să iniţializez codec-ul audio! -> fără sunet\n"
#define MSGTR_TryForceVideoFmt "Încerc să forţez utilizarea unui codec video din familia '%s' ...\n"
#define MSGTR_CantFindVfmtFallback "Nu pot să găsesc un codec video pentru familia forţată, revin la alte drivere.\n"
#define MSGTR_CantFindVideoCodec "Nu găsesc un codec video pentru formatul"
#define MSGTR_VOincompCodec "Îmi pare rău, ieşirea video selectată este incompatibilă cu acest codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATALĂ: Nu pot iniţializa codec-ul video :(\n"
#define MSGTR_EncodeFileExists "Fişierul există deja: %s (nu vă suprascrieţi fişierul AVI preferat!)\n"
#define MSGTR_CantCreateEncodeFile "Nu pot creea fişierul pentru codare\n"
#define MSGTR_CannotInitVO "FATALĂ: Nu pot iniţializa diver-ul video!\n"
#define MSGTR_CannotInitAO "nu pot deschide/iniţializa dispozitivul audio -> fără sunet\n"
#define MSGTR_StartPlaying "Încep afişarea...\n"

#define MSGTR_Playing "Afişez %s\n"
#define MSGTR_NoSound "Audio: fără sunet!!!\n"
#define MSGTR_FPSforced "FPS forţat la %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispozitivul CD-ROM '%s' nu a fost găsit!\n"
#define MSGTR_ErrTrackSelect "Eroare la selectarea pistei VCD!"
#define MSGTR_ReadSTDIN "Citesc de la intrarea standard...\n"
#define MSGTR_UnableOpenURL "Nu pot accesa adresa: %s\n"
#define MSGTR_ConnToServer "Conectat la serverul: %s\n"
#define MSGTR_FileNotFound "Fişier negăsit: '%s'\n"

#define MSGTR_CantOpenDVD "Nu am putut deschide dispozitivul DVD: %s\n"
#define MSGTR_DVDwait "Citesc structura discului, vă rog aşteptaţi...\n"
#define MSGTR_DVDnumTitles "Pe acest DVD sunt %d titluri.\n"
#define MSGTR_DVDinvalidTitle "Număr titlu DVD invalid: %d\n"
#define MSGTR_DVDinvalidChapter "Număr capitol DVD invalid: %d\n"
#define MSGTR_DVDnumAngles "Sunt %d unghiuri în acest titlu DVD.\n"
#define MSGTR_DVDinvalidAngle "Număr unghi DVD invalid: %d\n"
#define MSGTR_DVDnoIFO "Nu pot deschide fişierul IFO pentru titlul DVD %d.\n"
#define MSGTR_DVDnoVOBs "Nu pot deschide fişierul titlu (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD deschis cu succes!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Atenţie! Antet pistă audio %d redefinit!\n"
#define MSGTR_VideoStreamRedefined "Atenţie! Antet pistă video %d redefinit!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Prea multe (%d în %d bytes) pachete audio în tampon!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Prea multe (%d în %d bytes) pachete video în tampon!\n"
#define MSGTR_MaybeNI "(poate afişaţi un film/pistă ne-întreţesut sau codec-ul a dat eroare)\n"
#define MSGTR_DetectedFILMfile "Format fişier detectat: FILM\n"
#define MSGTR_DetectedFLIfile "Format fişier detectat: FLI\n"
#define MSGTR_DetectedROQfile "Format fişier detectat: RoQ\n"
#define MSGTR_DetectedREALfile "Format fişier detectat: REAL\n"
#define MSGTR_DetectedAVIfile "Format fişier detectat: AVI\n"
#define MSGTR_DetectedASFfile "Format fişier detectat: ASF\n"
#define MSGTR_DetectedMPEGPESfile "Format fişier detectat: MPEG-PES\n"
#define MSGTR_DetectedMPEGPSfile "Format fişier detectat: MPEG-PS\n"
#define MSGTR_DetectedMPEGESfile "Format fişier detectat: MPEG-ES\n"
#define MSGTR_DetectedQTMOVfile "Format fişier detectat: QuickTime/MOV\n"
#define MSGTR_MissingMpegVideo "Lipseşte pista video MPEG!? contactaţi autorul, poate fi un bug :(\n"
#define MSGTR_InvalidMPEGES "Pistă MPEG-ES invalidă??? contactaţi autorul, poate fi un bug :(\n"
#define MSGTR_FormatNotRecognized "============= Îmi pare rău, acest format de fişier nu este recunoscut/suportat ===============\n"\
				  "======== Dacă acest fişier este o pistă AVI, ASF sau MPEG , contactaţi vă rog autorul! ========\n"
#define MSGTR_MissingVideoStream "Nu am găsit piste video!\n"
#define MSGTR_MissingAudioStream "Nu am găsit piste audio...  -> fără sunet\n"
#define MSGTR_MissingVideoStreamBug "Lipseşte pista video!? Contactaţi autorul, poate fi un bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: fişierul nu conţine pista audio sau video specificată\n"

#define MSGTR_NI_Forced "Forţat"
#define MSGTR_NI_Detected "Detectat"
#define MSGTR_NI_Message "%s fişier AVI NE-ÎNTREŢESUT!\n"

#define MSGTR_UsingNINI "Folosesc fişier AVI NE-ÎNTREŢESUT eronat!\n"
#define MSGTR_CouldntDetFNo "Nu pot determina numărul de cadre (pentru căutare SOFă)\n"
#define MSGTR_CantSeekRawAVI "Nu pot căuta în fişiere .AVI neindexate! (am nevoie de index, încercaţi cu -idx!)  \n"
#define MSGTR_CantSeekFile "Nu pot căuta în fişier!  \n"

#define MSGTR_MOVcomprhdr "MOV: Antetele compresate nu sunt (încă) suportate!\n"
#define MSGTR_MOVvariableFourCC "MOV: Atenţie! variabilă FOURCC detectată!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Atenţie! prea multe piste!"
#define MSGTR_MOVnotyetsupp "\n****** Formatul Quicktime MOV nu este înca suportat!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nu pot deschide codec-ul audio\n"
#define MSGTR_CantCloseCodec "nu pot deschide codec-ul video\n"

#define MSGTR_MissingDLLcodec "EROARE: Nu pot deschide codec-ul DirectShow necesar: %s\n"
#define MSGTR_ACMiniterror "Nu pot încărca/iniţializa codec-ul audio Win32/ACM (lipseşte fişierul DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nu găsesc codec-ul '%s' în libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayerXP a fost compilat FĂRĂ suport directshow!\n"
#define MSGTR_NoWfvSupport "Suport pentru codec-urile win32 dezactivat, sau nedisponibil pe platformele ne-x86!\n"
#define MSGTR_NoDivx4Support "MPlayerXP a fost compilat FĂRĂ suport DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP a fost compilat FĂRĂ suport ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Codec-ul audio Win32/ACM dezactivat, sau nedisponibil pe procesoare ne-x86 -> fortare fara sunet :(\n"
#define MSGTR_NoDShowAudio "Compilat fără suport DirectShow -> forţare fără sunet :(\n"
#define MSGTR_NoOggVorbis "Codec-ul audio OggVorbis dezactivat -> forţare fără sunet :(\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATALĂ: EOF în timpul căutării antetului secvenţei\n"
#define MSGTR_CannotReadMpegSequHdr "FATALĂ: Nu pot citi antetul secvenţei!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATALĂ: Nu pot citi extensia antetului secvenţei!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Antet secvenţă eronat!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Extensie antet secvenţă eronată!\n"

#define MSGTR_ShMemAllocFail "Nu pot aloca memoria partajată\n"
#define MSGTR_CantAllocAudioBuf "Nu pot aloca tamponul pentru ieşirea audio\n"
#define MSGTR_NoMemForDecodedImage "memorie insuficientă pentru tamponul imaginii decodate (%ld bytes)\n"

#define MSGTR_AC3notvalid "pistă AC3 invalidă.\n"
#define MSGTR_AC3only48k "Doar piste de 48000 Hz sunt suportate.\n"
#define MSGTR_UnknownAudio "Format audio necunoscut/lipsă, folosesc fără sunet\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Setez suportul pentru LIRC...\n"
#define MSGTR_LIRCdisabled "Nu veţi putea utiliza telecomanda\n"
#define MSGTR_LIRCopenfailed "Nu pot deschide suportul pentru LIRC!\n"
#define MSGTR_LIRCsocketerr "Ceva nu este în regula cu portul LIRC: %s\n"
#define MSGTR_LIRCcfgerr "Nu pot citi fişierul de configurare LIRC %s !\n"
