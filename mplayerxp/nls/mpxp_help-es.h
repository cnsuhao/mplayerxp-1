// Translated by: Leandro Lucarella <leandro@lucarella.com.ar>
// Translated by: Jesús Climent <jesus.climent@hispalinux.es>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (vea DOCS!)\n"
"\n";

static char help_text[]=
"Uso:   mplayerxp [opciones] [ruta/]archivo\n"
"\n"
"Opciones:\n"
" -vo <drv[:dev]> selecciona el driver de salida de video y el dispositivo ('-vo help' para obtener una lista)\n"
" -ao <drv[:dev]> selecciona el driver de salida de audio y el dispositivo ('-ao help' para obtener una lista)\n"
" -play.ss <timepos>busca una determindad posicion (en segundos o hh:mm:ss)\n"
" -audio.off      no reproduce el sonido\n"
" -video.fs       opciones de pantalla completa (pantalla completa,cambio de modo de video,escalado por software)\n"
" -sub.file <file> especifica el archivo de subtitulos a usar\n"
" -play.list<file> especifica el archivo con la lista de reproducción\n"
" -sync.framedrop  activa frame-dropping (para máquinas lentas)\n"
"\n"
"Teclas:\n"
" <-  o  ->      avanza/retrocede 10 segundos\n"
" arriba o abajo avanza/retrocede 1 minuto\n"
" < o >          avanza/retrocede en la lista de reproducción\n"
" p o ESPACIO    pausa el video (presione cualquier tecla para continuar)\n"
" q o ESC        detiene la reproducción y sale del programa\n"
" o              cambia el modo OSD:  nada / búsqueda / búsqueda+tiempo\n"
" * o /          aumenta o disminuye el volumen (presione 'm' para elegir entre master/pcm)\n"
"\n"
" * * * VEA LA PÁGINA DE MANUAL PARA MÁS DETALLES, OPCIONES AVANZADAS Y TECLAS ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "Saliendo"
#define MSGTR_Exit_frames "Número de cuadros requeridos reproducidos"
#define MSGTR_Exit_quit "Salida"
#define MSGTR_Exit_eof "Fin del archivo"
#define MSGTR_Fatal_error "Error fatal"
#define MSGTR_NoHomeDir "No se puede encontrar el directorio HOME"
#define MSGTR_Playing "Reproduciendo"
