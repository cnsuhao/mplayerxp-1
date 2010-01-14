// Translated to Japanese.
// Translated by smoker <http://smokerz.net/~smoker/>
// UTF-8
#ifdef HELP_MP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"使い方:   mplayerxp [オプション] [url|パス/]ファイル名\n"
"\n"
"基本的なオプション: (man page に全て網羅されています)\n"
" -vo <drv[:dev]>  映像出力ドライバ及びデバイスを選択します ('-vo help'で一覧表示されます)\n"
" -ao <drv[:dev]>  音声出力ドライバ及びデバイスを選択します ('-ao help'で一覧表示されます)\n"
" -play.ss <timepos>timeposに与えられた場所から再生します(seconds or hh:mm:ss)\n"
" -audio.off     音声出力を抑止します\n"
" -video.fs      フルスクリーン表示します(もしくは -video.vm, -video.zoom, 詳細はmanにあります)\n"
" -sub.file <file> 利用する subtitle ファイルを選択する\n"
" -play.list<file> playlistファイルを選択する\n"
" -sync.framedrop   frame droppingを有効にする (低速なマシン向きです)\n"
"\n"
"基本的なコマンド: (man pageに全て網羅されています。事前にinput.confも確認して下さい)\n"
" <-  or  ->       10秒単位で前後にシークします\n"
" up or down       1分単位で前後にシークします\n"
" pgup or pgdown   10分単位で前後にシークします\n"
" < or >           プレイリストを元に前後のファイルに遷移します\n"
" p or SPACE       再生を静止します(何かボタンを押下すると再生を開始します)\n"
" q or ESC         再生を静止し、プログラムを停止します\n"
" + or -           音声を 0.1 秒単位で早めたり遅れさせたり調整する\n"
" o                cycle OSD mode:  none / seekbar / seekbar + timer\n"
" * or /           PCM 音量を上げたり下げたりする\n"
" z or x           subtitleを 0.1 秒単位で早めたり遅れさせたり調整する\n"
" r or t           subtitleの位置を上げたり下げたり調整する, -vfオプションも確認して下さい\n"
"\n"
" * * * man pageに詳細がありますので、確認して下さい。さらに高度で進んだオプションやキーも記載してます * * *\n"
"\n";
#endif
#endif
