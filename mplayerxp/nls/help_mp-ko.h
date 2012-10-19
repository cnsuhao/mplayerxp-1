// Translated by: DongCheon Park <pdc@kaist.ac.kr>

// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
// and send a notify message to mplayer-dev-eng maillist.
// UTF-8
// ========================= MPlayer 도움말 ===========================

#ifdef HELP_MP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (DOCS 참조!)\n"
"\n";

static char help_text[]=
"사용법:   mplayerxp [선택사항] [경로/]파일명\n"
"\n"
"선택사항들:\n"
" -vo <drv[:dev]>  비디오 출력 드라이버 및 장치 선택 (목록보기는 '-vo help')\n"
" -ao <drv[:dev]>  오디오 출력 드라이버 및 장치 선택 (목록보기는 '-ao help')\n"
" -play.ss <timepos>특정 위치로 찾아가기 (초 또는 시:분:초)\n"
" -audio.off       소리 재생 안함\n"
" -video.fs        화면 크기 지정 (전체화면, 비디오모드, s/w확대)\n"
" -sub.file <file> 사용할 자막파일 지정\n"
" -play.list<file> 재생목록파일 지정\n"
" -sync.framedrop  프레임 빠뜨리기 사용 (느린 machine용)\n"
"\n"
"조정키:\n"
" <-  또는  ->     10초 뒤로/앞으로 이동\n"
" up 또는 dn       1분 뒤로/앞으로 이동\n"
" < 또는 >         재생목록에서 뒤로/앞으로 이동\n"
" p 또는 SPACE     잠시 멈춤 (아무키나 누르면 계속)\n"
" q 또는 ESC       재생을 멈추고 프로그램을 끝냄\n"
" o                OSD모드 변경:  없음/탐색줄/탐색줄+타이머\n"
" * 또는 /         볼륨 높임/낮춤 ('m'을 눌러 master/pcm 선택)\n"
" z 또는 x         +/- 0.1초 자막 지연 조절\n"
"\n"
" * * * 자세한 사항(더 많은 선택사항 및 조정키등)은 MANPAGE를 참조하세요 ! * * *\n"
"\n";
#endif

// ========================= MPlayer 메세지 ===========================

// mplayer.c: 

#define MSGTR_Exiting "\n종료합니다... (%s)\n"
#define MSGTR_Exit_frames "요청한 프레임수를 재생하였습니다."
#define MSGTR_Exit_quit "종료"
#define MSGTR_Exit_eof "파일의 끝"
#define MSGTR_Exit_error "치명적 오류"
#define MSGTR_IntBySignal "\nMPlayerXP가 %s모듈에서 %d신호로 인터럽트되었습니다.\n"
#define MSGTR_NoHomeDir "홈디렉토리를 찾을 수 없습니다.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") 문제 발생\n"
#define MSGTR_CreatingCfgFile "설정파일 %s를 만듭니다.\n"
#define MSGTR_InvalidVOdriver "%s는 잘못된 비디오 출력 드라이버입니다.\n가능한 비디오 출력 드라이버 목록을 보려면 '-vo help' 하세요.\n"
#define MSGTR_InvalidAOdriver "%s는 잘못된 오디오 출력 드라이버입니다.\n가능한 오디오 출력 드라이버 목록을 보려면 '-ao help' 하세요.\n"
#define MSGTR_CopyCodecsConf "((MPlayerXP 소스 트리의) etc/codecs.conf를 ~/.mplayerxp/codecs.conf로 복사 또는 링크하세요.)\n"
#define MSGTR_CantLoadFont "%s 폰트를 찾을 수 없습니다.\n"
#define MSGTR_CantLoadSub "%s 자막을 찾을 수 없습니다.\n"
#define MSGTR_ErrorDVDkey "DVD 키를 처리하는 도중 오류가 발생했습니다.\n"
#define MSGTR_CmdlineDVDkey "요청한 DVD 명령줄 키를 해독을 위해 저장했습니다.\n"
#define MSGTR_DVDauthOk "DVD 인증 결과가 정상적인듯 합니다.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: 치명적: 선택된 스트림이 없습니다!\n"
#define MSGTR_CantOpenDumpfile "dump파일을 열 수 없습니다!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "헤더에 FPS가 지정되지않았거나 잘못되었습니다! -fps 옵션을 사용하세요!\n"
#define MSGTR_NoVideoStream "죄송합니다, 비디오 스트림이 없습니다... 아직 재생불가능합니다.\n"
#define MSGTR_TryForceAudioFmt "오디오 코덱 드라이버 %d류를 시도하고 있습니다...\n"
#define MSGTR_CantFindAfmtFallback "시도한 드라이버류에서 오디오 코덱을 찾을 수 없습니다. 다른 드라이버로 대체하세요.\n"
#define MSGTR_CantFindAudioCodec "오디오 형식를 위한 코덱을 찾을 수 없습니다:"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** etc/codecs.conf로 부터 %s를 업그레이드해보세요.\n*** 여전히 작동하지않으면, DOCS/codecs.html을 읽어보세요!\n"
#define MSGTR_CouldntInitAudioCodec "오디오 코덱을 초기화할 수 없습니다! -> 소리없음\n"
#define MSGTR_TryForceVideoFmt "비디오 코덱 드라이버 %d류를 시도하고 있습니다...\n"
#define MSGTR_CantFindVfmtFallback "시도한 드라이버류에서 비디오 코덱을 찾을 수 없습니다. 다른 드라이버로 대체하세요.\n"
#define MSGTR_CantFindVideoCodec "비디오 형식를 위한 코덱을 찾을 수 없습니다:"
#define MSGTR_VOincompCodec "죄송합니다, 선택한 비디오 출력 장치는 이 코덱과 호환되지 않습니다.\n"
#define MSGTR_CouldntInitVideoCodec "치명적: 비디오 코덱을 초기화할 수 없습니다. :(\n"
#define MSGTR_EncodeFileExists "파일이 이미 존재합니다.: %s (당신이 아끼는 AVI를 덮어쓰지마세요!)\n"
#define MSGTR_CantCreateEncodeFile "인코딩을 위한 파일을 만들 수 없습니다.\n"
#define MSGTR_CannotInitVO "치명적: 비디오 드라이버를 초기화할 수 없습니다!\n"
#define MSGTR_CannotInitAO "오디오 장치를 열거나 초기화할 수 없습니다. -> 소리없음\n"
#define MSGTR_StartPlaying "재생을 시작합니다...\n"

#define MSGTR_Playing "%s 재생중\n"
#define MSGTR_NoSound "오디오: 소리없음!!!\n"
#define MSGTR_FPSforced "FPS가 %5.3f (ftime: %5.3f)이 되도록 하였습니다.\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM 장치 '%s'를 찾을 수 없습니다!\n"
#define MSGTR_ErrTrackSelect "VCD 트랙을 선택하는 도중 에러가 발생했습니다!"
#define MSGTR_ReadSTDIN "표준입력(stdin)으로 부터 읽고 있습니다...\n"
#define MSGTR_UnableOpenURL "%s URL을 열 수 없습니다.\n"
#define MSGTR_ConnToServer "%s 서버에 연결되었습니다.\n"
#define MSGTR_FileNotFound "'%s'파일을 찾을 수 없습니다.\n"

#define MSGTR_CantOpenDVD "DVD 장치 %s를 열 수 없습니다.\n"
#define MSGTR_DVDwait "디스크 구조를 읽고있습니다, 기다려 주세요...\n"
#define MSGTR_DVDnumTitles "이 DVD에는 %d 타이틀이 있습니다.\n"
#define MSGTR_DVDinvalidTitle "잘못된 DVD 타이틀 번호: %d\n"
#define MSGTR_DVDinvalidChapter "잘못된 DVD 챕터 번호: %d\n"
#define MSGTR_DVDnumAngles "이 DVD 타이틀에는 %d 앵글이 있습니다.\n"
#define MSGTR_DVDinvalidAngle "잘못된 DVD 앵글 번호: %d\n"
#define MSGTR_DVDnoIFO "DVD 타이틀 %d를 위한 IFO파일을 열 수 없습니다.\n"
#define MSGTR_DVDnoVOBs "타이틀 VOBS (VTS_%02d_1.VOB)를 열 수 없습니다.\n"
#define MSGTR_DVDopenOk "성공적으로 DVD가 열렸습니다!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "경고! 오디오 스트림 헤더 %d가 재정의되었습니다!\n"
#define MSGTR_VideoStreamRedefined "경고! 비디오 스트림 헤더 %d가 재정의되었습니다!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: 버퍼에 너무 많은 (%d in %d bytes) 오디오 패킷이 있습니다!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: 버퍼에 너무 많은 (%d in %d bytes) 비디오 패킷이 있습니다!\n"
#define MSGTR_MaybeNI "(non-interleaved 스트림/파일을 재생하고있거나 코덱이 잘못되었습니다.)\n"
#define MSGTR_DetectedFILMfile "FILM 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedFLIfile "FLI 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedROQfile "RoQ 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedREALfile "REAL 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedAVIfile "AVI 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedASFfile "ASF 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedMPEGPESfile "MPEG-PES 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedMPEGPSfile "MPEG-PS 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedMPEGESfile "MPEG-ES 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedQTMOVfile "QuickTime/MOV 파일 형식을 발견했습니다!\n"
#define MSGTR_MissingMpegVideo "찾을 수 없는 MPEG 비디오 스트림!? 저작자에게 문의하세요, 버그일지도 모릅니다. :(\n"
#define MSGTR_InvalidMPEGES "잘못된 MPEG-ES 스트림??? 저작자에게 문의하세요, 버그일지도 모릅니다. :(\n"
#define MSGTR_FormatNotRecognized "============= 죄송합니다, 이 파일형식은 인식되지못했거나 지원되지않습니다 ===============\n"\
				  "=== 만약 이 파일이 AVI, ASF 또는 MPEG 스트림이라면, 저작자에게 문의하세요! ===\n"
#define MSGTR_MissingVideoStream "비디오 스트림을 찾지 못했습니다!\n"
#define MSGTR_MissingAudioStream "오디오 스트림을 찾지 못했습니다...  ->소리없음\n"
#define MSGTR_MissingVideoStreamBug "찾을 수 없는 비디오 스트림!? 저작자에게 문의하세요, 버그일지도 모릅니다. :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: 파일에 선택된 오디오 및 비디오 스트림이 없습니다.\n"

#define MSGTR_NI_Forced "강제로"
#define MSGTR_NI_Detected "발견됨"
#define MSGTR_NI_Message "%s는 NON-INTERLEAVED AVI 파일 형식입니다!\n"

#define MSGTR_UsingNINI "NON-INTERLEAVED 깨진 AVI 파일 형식을 사용합니다!\n"
#define MSGTR_CouldntDetFNo "프레임 수를 결정할 수 없습니다.\n"
#define MSGTR_CantSeekRawAVI "raw .AVI 스트림에서는 탐색할 수 없습니다! (인덱스가 필요합니다, -idx 스위치로 시도해보세요!)  \n"
#define MSGTR_CantSeekFile "이 파일에서는 탐색할 수 없습니다!  \n"

#define MSGTR_EncryptedVOB "암호화된 VOB 파일입니다(libcss 지원없이 컴파일되었음)! DOCS/cd-dvd.html을 참조하세요\n"
#define MSGTR_EncryptedVOBauth "암호화된 스트림인데, 인증 신청을 하지않았습니다!!\n"

#define MSGTR_MOVcomprhdr "MOV: 압축된 헤더는 (아직) 지원되지않습니다!\n"
#define MSGTR_MOVvariableFourCC "MOV: 경고! FOURCC 변수 발견!?\n"
#define MSGTR_MOVtooManyTrk "MOV: 경고! 트랙이 너무 많습니다!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV 형식은 아직 지원되지않습니다!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "코덱을 열 수 없습니다.\n"
#define MSGTR_CantCloseCodec "코덱을 닫을 수 없습니다.\n"

#define MSGTR_MissingDLLcodec "에러: 요청한 DirectShow 코덱 %s를 열 수 없습니다.\n"
#define MSGTR_ACMiniterror "Win32/ACM 오디오 코덱을 열거나 초기화할 수 없습니다. (DLL 파일이 없나요?)\n"
#define MSGTR_MissingLAVCcodec "libavcodec에서 '%s' 코덱을 찾을 수 없습니다...\n"

#define MSGTR_NoDShowSupport "MPlayerXP가 directshow 지원없이 컴파일되었습니다!\n"
#define MSGTR_NoWfvSupport "win32 코덱 지원이 불가능하거나, 비 x86플랫폼에서는 사용할 수 없습니다!\n"
#define MSGTR_NoDivx4Support "MPlayerXP가 DivX4Linux (libdivxdecore.so) 지원없이 컴파일되었습니다!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP가 ffmpeg/libavcodec 지원없이 컴파일되었습니다!\n"
#define MSGTR_NoACMSupport "Win32/ACM 오디오 코덱이 불가능하거나, 비 x86 CPU에서는 사용할 수 없습니다. -> 소리없음 :(\n"
#define MSGTR_NoDShowAudio "DirectShow 지원없이 컴파일되었습니다. -> 소리없음 :(\n"
#define MSGTR_NoOggVorbis "OggVorbis 오디오 코덱이 불가능합니다. -> f소리없음 :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP가 XAnim 지원없이 컴파일되었습니다!\n"

#define MSGTR_MpegNoSequHdr "MPEG: 치명적: 시퀀스 헤더를 찾는 도중 EOF.\n"
#define MSGTR_CannotReadMpegSequHdr "치명적: 시퀀스 헤더를 찾을 수 없습니다!\n"
#define MSGTR_CannotReadMpegSequHdrEx "치명적: 시퀀스 헤더 확장을 읽을 수 없습니다!\n"
#define MSGTR_BadMpegSequHdr "MPEG: 불량한 시퀀스 헤더!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: 불량한 시퀀스 헤더 확장!\n"

#define MSGTR_ShMemAllocFail "공유 메모리를 할당할 수 없습니다.\n"
#define MSGTR_CantAllocAudioBuf "오디오 출력 버퍼를 할당할 수 없습니다.\n"
#define MSGTR_NoMemForDecodedImage "발견된 picture 버퍼에 충분한 메모리가 없습니다(%ld bytes).\n"

#define MSGTR_AC3notvalid "AC3 스트림은 유효하지 않습니다.\n"
#define MSGTR_AC3only48k "48000 Hz 스트림만 지원됩니다.\n"
#define MSGTR_UnknownAudio "알 수 없는 오디오 형식입니다. -> 소리없음\n"

// LIRC:
#define MSGTR_SettingUpLIRC "lirc 지원을 시작합니다...\n"
#define MSGTR_LIRCdisabled "리모콘을 사용할 수 없습니다.\n"
#define MSGTR_LIRCopenfailed "lirc 지원 시작 실패!\n"
#define MSGTR_LIRCsocketerr "lirc 소켓에 문제가 있습니다.: %s\n"
#define MSGTR_LIRCcfgerr "LIRC 설정파일 %s를 읽는데 실패했습니다!\n"
