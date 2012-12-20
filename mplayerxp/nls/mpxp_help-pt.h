// Translated by Fabio Pugliese Ornellas <fabio.ornellas@poli.usp.br>
// Portuguese from Brazil Translation
// UTF-8
#ifdef HELP_MPXP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Uso:   mplayerxp [opções] [url|caminho/]nome-do-arquivo",
"",
"Opções básicas: (lista completa na página do manual)",
" -vo <drv[:dev]> seleciona o driver de saída de vídeo & dispositivo",
"                 ('-vo help' para listar)",
" -ao <drv[:dev]> seleciona o driver de saída de audio & dispositivo",
"                 ('-vo help' para listar)",
" -play.ss <tempopos>busca para a posição dada (segundos ou hh:mm:ss)",
" -audio.off      não reproduz som",
" -video.fs       reprodução em tela cheia (ou -video.vm, -video.zoom, detalhes na página do",
"                 manual)",
" -sub.file <arquivo>especifica o arquivo de legenda a usar",
" -play.list<arquivo> especifica o aruqivo com a lista de reprodução",
" -sync.framedrop habilita descarte de quadros (para máquinas lentas)",
"",
"Teclas básicas: (lista completa na páginal do manual, cheque também input.conf)",
" <-  ou  ->      retorna/avança 10 segundos",
" cima ou baixo   retorna/avança 1 minuto",
" pgup ou pgdown  retorna/avança 10 minutos",
" < ou >          retorna/avança na lista de reprodução",
" p ou ESPAÇO     pausa o filme (pressione qualquer tecla para continuar)",
" q ou ESC        para a reprodução e sai do programa",
" o               alterna modo OSD: nenhum / busca / busca+cronômetro",
" * ou /          aumenta ou diminui o volume pcm",
"",
"* VEJA A PÁGINA DO MANUAL PARA DETALHES, FUTURAS (AVANÇADAS) OPÇÕES E TECLAS *",
NULL
};
#endif
#endif
