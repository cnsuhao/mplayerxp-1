// Translated by: Leandro Lucarella <leandro@lucarella.com.ar>
// Translated by: Jesús Climent <jesus.climent@hispalinux.es>
// UTF-8
// ========================= MPlayer help ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (vea DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Uso:   mplayerxp [opciones] [ruta/]archivo",
"",
"Opciones:",
" -vo <drv[:dev]> selecciona el driver de salida de video y el dispositivo ('-vo help' para obtener una lista)",
" -ao <drv[:dev]> selecciona el driver de salida de audio y el dispositivo ('-ao help' para obtener una lista)",
" -play.ss <timepos>busca una determindad posicion (en segundos o hh:mm:ss)",
" -audio.off      no reproduce el sonido",
" -video.fs       opciones de pantalla completa (pantalla completa,cambio de modo de video,escalado por software)",
" -sub.file <file> especifica el archivo de subtitulos a usar",
" -play.list<file> especifica el archivo con la lista de reproducción",
" -sync.framedrop  activa frame-dropping (para máquinas lentas)",
"",
"Teclas:",
" <-  o  ->      avanza/retrocede 10 segundos",
" arriba o abajo avanza/retrocede 1 minuto",
" < o >          avanza/retrocede en la lista de reproducción",
" p o ESPACIO    pausa el video (presione cualquier tecla para continuar)",
" q o ESC        detiene la reproducción y sale del programa",
" o              cambia el modo OSD:  nada / búsqueda / búsqueda+tiempo",
" * o /          aumenta o disminuye el volumen (presione 'm' para elegir entre master/pcm)",
"",
" * * * VEA LA PÁGINA DE MANUAL PARA MÁS DETALLES, OPCIONES AVANZADAS Y TECLAS ! * * *",
NULL
};
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
