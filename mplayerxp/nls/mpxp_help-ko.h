// Translated by: DongCheon Park <pdc@kaist.ac.kr>

// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
// and send a notify message to mplayer-dev-eng maillist.
// UTF-8
// ========================= MPlayer 도움말 ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
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
"\n"
" * * * 자세한 사항(더 많은 선택사항 및 조정키등)은 MANPAGE를 참조하세요 ! * * *\n"
"\n";
#endif

// ========================= MPlayer 메세지 ===========================

// mplayer.c:

#define MSGTR_Exiting "종료합니다"
#define MSGTR_Exit_frames "요청한 프레임수를 재생하였습니다"
#define MSGTR_Exit_quit "종료"
#define MSGTR_Exit_eof "파일의 끝"
#define MSGTR_Fatal_error "치명적 오류"
#define MSGTR_NoHomeDir "홈디렉토리를 찾을 수 없습니다"
#define MSGTR_Playing "재생중"
