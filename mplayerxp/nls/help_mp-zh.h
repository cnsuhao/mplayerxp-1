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
#ifdef HELP_MP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"用法：            mplayerxp [选项] [URL|路径/]文件名\n"
"\n"
"基本选项：        （完整列表参见手册页）\n"
" -vo <drv>        选择视频输出驱动程序（查看驱动程序列表用“-vo help”）\n"
" -ao <drv>        选择音频输出驱动程序（查看驱动程序列表用“-ao help”）\n"
#ifdef CONFIG_VCD
" vcd://<曲目号>   播放 (S)VCD（超级VCD）曲目（无格式设备，无需装载）\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<标题号>   从设备而不是从普通文件上播放 DVD 标题\n"
#endif
" -alang/-slang    选择 DVD 音频/字幕的语言（使用两字符的国家代号）\n"
" -ss <位置>       定位至给定（秒数或时:分:秒 - hh:mm:ss）位置\n"
" -nosound         不播放声音\n"
" -fs              全屏播放（或用 -vm、-zoom，详见手册相关页面）\n"
" -x <x> -y <y>    设置显示的分辨率（用以与 -vm 或 -zoom 一起使用）\n"
" -sub <文件>      指定所使用的字幕文件（另见 -subfps、-subdelay）\n"
" -playlist <文件> 指定播放列表文件\n"
" -vid x -aid y    选择播放视频流（x）和音频流（y）\n"
" -fps x -srate y  改变视频帧率为 x（fps），音频采样率为 y（Hz）\n"
" -pp <质量>       启用后期处理过滤器（详见于手册相关页面）\n"
" -framedrop       启用丢帧（用于运行慢的机器）\n"
"\n"
"基本控制键：      （完整列表见手册相关页面，也请查阅 input.conf）\n"
" <-  或  ->       后退/快进 10 秒\n"
" 上 或 下         后退/快进 1 分钟\n"
" pgdown 或 pgup   后退/快进 10 分钟\n"
" < 或 >           跳到播放列表中的前一个/后一个\n"
" p 或 空格键      暂停影片（按任意键继续）\n"
" q 或 ESC         停止播放并退出程序\n"
" + 或 -           音频延迟 +/- 0.1 秒\n"
" o                循环切换 OSD 模式：无/定位条/定位条加计时器\n"
" * 或 /           增加或减少 PCM 音量\n"
" x 或 z           字幕延迟 +/- 0.1 秒\n"
" r 或 t           字幕位置上移/下移，另见“-vf expand”\n"
"\n"
" * * * 参见手册相关页面可获取具体内容，及更多（高级）选项和控制键的信息 * * *\n"
"\n";
#endif
#endif
