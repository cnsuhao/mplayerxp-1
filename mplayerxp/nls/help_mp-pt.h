// Translated by Fabio Pugliese Ornellas <fabio.ornellas@poli.usp.br>
// Portuguese from Brazil Translation
// UTF-8
#ifdef HELP_MP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Uso:   mplayerxp [opções] [url|caminho/]nome-do-arquivo\n"
"\n"
"Opções básicas: (lista completa na página do manual)\n"
" -vo <drv[:dev]> seleciona o driver de saída de vídeo & dispositivo\n"
"                 ('-vo help' para listar)\n"
" -ao <drv[:dev]> seleciona o driver de saída de audio & dispositivo\n"
"                 ('-vo help' para listar)\n"
" -play.ss <tempopos>busca para a posição dada (segundos ou hh:mm:ss)\n"
" -audio.off      não reproduz som\n"
" -video.fs       reprodução em tela cheia (ou -video.vm, -video.zoom, detalhes na página do\n"
"                 manual)\n"
" -sub.file <arquivo>especifica o arquivo de legenda a usar\n"
" -play.list<arquivo> especifica o aruqivo com a lista de reprodução\n"
" -sync.framedrop habilita descarte de quadros (para máquinas lentas)\n"
"\n"
"Teclas básicas: (lista completa na páginal do manual, cheque também input.conf)\n"
" <-  ou  ->      retorna/avança 10 segundos\n"
" cima ou baixo   retorna/avança 1 minuto\n"
" pgup ou pgdown  retorna/avança 10 minutos\n"
" < ou >          retorna/avança na lista de reprodução\n"
" p ou ESPAÇO     pausa o filme (pressione qualquer tecla para continuar)\n"
" q ou ESC        para a reprodução e sai do programa\n"
" o               alterna modo OSD: nenhum / busca / busca+cronômetro\n"
" * ou /          aumenta ou diminui o volume pcm\n"
"\n"
"* VEJA A PÁGINA DO MANUAL PARA DETALHES, FUTURAS (AVANÇADAS) OPÇÕES E TECLAS *\n"
"\n";
#endif
#endif
