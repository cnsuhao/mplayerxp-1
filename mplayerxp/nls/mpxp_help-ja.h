// Translated to Japanese.
// Translated by smoker <http://smokerz.net/~smoker/>
// UTF-8
#ifdef HELP_MPXP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static const char* banner_text={
"",
"",
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)",
NULL
};

static const char* help_text[]={
"",
"使い方:   mplayerxp [オプション] [url|パス/]ファイル名",
"",
"基本的なオプション: (man page に全て網羅されています)",
" -vo <drv[:dev]>  映像出力ドライバ及びデバイスを選択します ('-vo help'で一覧表示されます)",
" -ao <drv[:dev]>  音声出力ドライバ及びデバイスを選択します ('-ao help'で一覧表示されます)",
" -play.ss <timepos>timeposに与えられた場所から再生します(seconds or hh:mm:ss)",
" -audio.off     音声出力を抑止します",
" -video.fs      フルスクリーン表示します(もしくは -video.vm, -video.zoom, 詳細はmanにあります)",
" -sub.file <file> 利用する subtitle ファイルを選択する",
" -play.list<file> playlistファイルを選択する",
" -sync.framedrop   frame droppingを有効にする (低速なマシン向きです)",
"",
"基本的なコマンド: (man pageに全て網羅されています。事前にinput.confも確認して下さい)",
" <-  or  ->       10秒単位で前後にシークします",
" up or down       1分単位で前後にシークします",
" pgup or pgdown   10分単位で前後にシークします",
" < or >           プレイリストを元に前後のファイルに遷移します",
" p or SPACE       再生を静止します(何かボタンを押下すると再生を開始します)",
" q or ESC         再生を静止し、プログラムを停止します",
" o                cycle OSD mode:  none / seekbar / seekbar + timer",
" * or /           PCM 音量を上げたり下げたりする",
"",
" * * * man pageに詳細がありますので、確認して下さい。さらに高度で進んだオプションやキーも記載してます * * *",
NULL
};
#endif
#endif
