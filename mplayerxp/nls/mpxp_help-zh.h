// Reminder of hard terms which need better/final solution later:
//   (file links to be updated later if available!);
//   NAV; section/subsection;  XScreenSaver; keycolor;
//   AGP move failed on Y plane;
//   profile? demuxer? drain? flush?
//
// Translated by JRaSH <jrash06@163.com>
// (Translator before 2007-05-01)
// Lu Ran <hephooey@fastmail.fm>, Sheldon Jin <jinsh2 AT yahoo.com>
// (Translator before 2006-04-24)
// Emfox Zhou <EmfoxZhou@gmail.com>
// (Translator before 2005-10-12)
// Lu Ran <hephooey@fastmail.fm>
// UTF-8
#ifdef HELP_MPXP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)",
NULL
};

static const char help_text[]={
"",
"用法：            mplayerxp [选项] [URL|路径/]文件名",
"",
"基本选项：        （完整列表参见手册页）",
" -vo <drv>        选择视频输出驱动程序（查看驱动程序列表用“-vo help”）",
" -ao <drv>        选择音频输出驱动程序（查看驱动程序列表用“-ao help”）",
" -play.ss <位置>  定位至给定（秒数或时:分:秒 - hh:mm:ss）位置",
" -audio.off       不播放声音",
" -video.fs        全屏播放（或用 -video.vm、-video.zoom，详见手册相关页面）",
" -sub.file <文件> 指定所使用的字幕文件（另见",
" -play.list <文件>指定播放列表文件",
" -sync.framedrop  启用丢帧（用于运行慢的机器）",
"",
"基本控制键：      （完整列表见手册相关页面，也请查阅 input.conf）",
" <-  或  ->       后退/快进 10 秒",
" 上 或 下         后退/快进 1 分钟",
" pgdown 或 pgup   后退/快进 10 分钟",
" < 或 >           跳到播放列表中的前一个/后一个",
" p 或 空格键      暂停影片（按任意键继续）",
" q 或 ESC         停止播放并退出程序",
" o                循环切换 OSD 模式：无/定位条/定位条加计时器",
" * 或 /           增加或减少 PCM 音量",
"",
" * * * 参见手册相关页面可获取具体内容，及更多（高级）选项和控制键的信息 * * *",
NULL
};
#endif
#endif
