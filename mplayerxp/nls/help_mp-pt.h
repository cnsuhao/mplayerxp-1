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
#ifdef CONFIG_VCD
" vcd://<numtrilha> reproduz trilha de VCD (Video CD) do dispositivo em vez de um\n"
"                 arquivo\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<numtítilo> reproduz título de DVD do dispositivo em vez de um arquivo\n"
" -alang/-slang   seleciona o idioma/legenda do DVD (pelo código país de duas\n"
"                 letras)\n"
#endif
" -ss <tempopos>  busca para a posição dada (segundos ou hh:mm:ss)\n"
" -nosound        não reproduz som\n"
" -fs             reprodução em tela cheia (ou -vm, -zoom, detalhes na página do\n"
"                 manual)\n"
" -x <x> -y <y>   especifica a resolução da tela (para uso com -vm ou -zoom)\n"
" -sub <arquivo>  especifica o arquivo de legenda a usar (veja também -subfps,\n"
"                 -subdelay)\n"
" -playlist <arquivo> especifica o aruqivo com a lista de reprodução\n"
" -vid x -aid y   seleciona a trilha de vídeo (x) e audio (y) a reproduzir\n"
" -fps x -srate y muda a taxa do vídeo (x quadros por segundo) e audio (y Hz)\n"
" -pp <qualidade> habilita filtro de pós processamento (veja detalhes na página\n"
"                 do manual)\n"
" -framedrop      habilita descarte de quadros (para máquinas lentas)\n"
"\n"
"Teclas básicas: (lista completa na páginal do manual, cheque também input.conf)\n"
" <-  ou  ->      retorna/avança 10 segundos\n"
" cima ou baixo   retorna/avança 1 minuto\n"
" pgup ou pgdown  retorna/avança 10 minutos\n"
" < ou >          retorna/avança na lista de reprodução\n"
" p ou ESPAÇO     pausa o filme (pressione qualquer tecla para continuar)\n"
" q ou ESC        para a reprodução e sai do programa\n"
" + ou -          ajusta o atraso do audio de +/- 0.1 segundo\n"
" o               alterna modo OSD: nenhum / busca / busca+cronômetro\n"
" * ou /          aumenta ou diminui o volume pcm\n"
" z ou x          ajusta o atraso da legenda de +/- 0.1 segundo\n"
" r ou t          posição da legenda para cima/baixo, veja também -vf expand\n"
"\n"
"* VEJA A PÁGINA DO MANUAL PARA DETALHES, FUTURAS (AVANÇADAS) OPÇÕES E TECLAS *\n"
"\n";
#endif
#endif
