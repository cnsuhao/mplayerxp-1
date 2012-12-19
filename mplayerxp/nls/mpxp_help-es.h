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

#define MSGTR_Exiting "\nSaliendo... (%s)\n"
#define MSGTR_Exit_frames "Número de cuadros requeridos reproducidos"
#define MSGTR_Exit_quit "Salida"
#define MSGTR_Exit_eof "Fin del archivo"
#define MSGTR_Exit_error "Error fatal"
#define MSGTR_IntBySignal "\nMPlayerXP interrumpido por señal %d en el módulo: %s \n"
#define MSGTR_NoHomeDir "No se puede encontrar el directorio HOME\n"
#define MSGTR_GetpathProblem "problema en get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Creando archivo de configuración: %s\n"
#define MSGTR_InvalidVOdriver "Nombre del driver de salida de video incorrecto: %s\nUse '-vo help' para obtener la lista de drivers de salida de video disponibles.\n"
#define MSGTR_InvalidAOdriver "Nombre del driver de salida de audio incorrecto: %s\nUse '-ao help' para obtener la lista de drivers de salida de audio disponibles.\n"
#define MSGTR_CopyCodecsConf "(copie/ln etc/codecs.conf (en el árbol del codigo fuente de MPlayerXP) a ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "No se puede cargar la fuente: %s\n"
#define MSGTR_CantLoadSub "No se puede cargar el subtítulo: %s\n"
#define MSGTR_ErrorDVDkey "Error procesando la clave del DVD.\n"
#define MSGTR_CmdlineDVDkey "Clave de DVD requerida en la línea de comandos esta almacenada para 'descrambling'.\n"
#define MSGTR_DVDauthOk "La secuencia de autorización del DVD parece estar bien.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: no se encuentra el stream seleccionado!\n"
#define MSGTR_CantOpenDumpfile "No se puede abrir el archivo de dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS no especificado (o inválido) en la cabecera! Use la opción -fps!\n"
#define MSGTR_NoVideoStream "Disculpe, no tiene stream de video... no es reproducible todavía\n"
#define MSGTR_TryForceAudioFmt "Tratando de forzar la familia del codec de audio '%s' ...\n"
#define MSGTR_CantFindAfmtFallback "No se encuentra codec de audio para la familia forzada, se usan otros drivers.\n"
#define MSGTR_CantFindAudioCodec "No se encuentra codec para el formato de audio"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Intente actualizar %s en etc/codecs.conf\n*** Si todavía no funciona, lea DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "No se pudo inicializar el codec de audio! -> sin sonido\n"
#define MSGTR_TryForceVideoFmt "Tratando de forzar la familia del codec de video '%s' ...\n"
#define MSGTR_CantFindVfmtFallback "No se encuentra codec de video para la familia forzada, se usan otros drivers.\n"
#define MSGTR_CantFindVideoCodec "No se encuentra codec para el formato de video"
#define MSGTR_VOincompCodec "Disculpe, el dispositivo de salida de video es incompatible con este codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: No se puede inicializar el codec de video :(\n"
#define MSGTR_EncodeFileExists "El archivo ya existe: %s (no sobrescriba su AVI favorito!)\n"
#define MSGTR_CantCreateEncodeFile "No se puede crear el archivo para 'encodear'\n"
#define MSGTR_CannotInitVO "FATAL: No se puede inicializar el driver de video!\n"
#define MSGTR_CannotInitAO "no se puede abrir/inicializar dispositivo de audio -> SIN SONIDO\n"
#define MSGTR_StartPlaying "Empezando a reproducir...\n"

#define MSGTR_Playing "Reproduciendo %s\n"
#define MSGTR_NoSound "Audio: sin sonido!!!\n"
#define MSGTR_FPSforced "FPS forzado en %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispositivo de CD-ROM '%s' no encontrado!\n"
#define MSGTR_ErrTrackSelect "Error seleccionando la pista de VCD!"
#define MSGTR_ReadSTDIN "Leyendo desde la entrada estándar (stdin)...\n"
#define MSGTR_UnableOpenURL "No es posible abrir URL: %s\n"
#define MSGTR_ConnToServer "Connectado al servidor: %s\n"
#define MSGTR_FileNotFound "Archivo no encontrado: '%s'\n"

#define MSGTR_CantOpenDVD "No se puede abrir el dispositivo de DVD: %s\n"
#define MSGTR_DVDwait "Leyendo la estructura del disco, espere por favor...\n"
#define MSGTR_DVDnumTitles "Hay %d títulos en este DVD.\n"
#define MSGTR_DVDinvalidTitle "Número de título de DVD inválido: %d\n"
#define MSGTR_DVDinvalidChapter "Número de capítulo de DVD inválido: %d\n"
#define MSGTR_DVDnumAngles "Hay %d ángulos en este título de DVD.\n"
#define MSGTR_DVDinvalidAngle "Número de ángulo de DVD inválido: %d\n"
#define MSGTR_DVDnoIFO "No se puede abrir archivo IFO para el título de DVD %d.\n"
#define MSGTR_DVDnoVOBs "No se puede abrir VOBS del título (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD abierto existosamente!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Advertencia! Cabecera de stream de audio %d redefinida!\n"
#define MSGTR_VideoStreamRedefined "Advertencia! Cabecera de stream de video %d redefinida!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Demasiados (%d en %d bytes) paquetes de audio en el buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Demasiados (%d en %d bytes) paquetes de video en el buffer!\n"
#define MSGTR_MaybeNI "(tal vez está reproduciendo un stream/archivo 'non-interleaved' o falló el codec)\n"
#define MSGTR_DetectedFILMfile "Detectado formato de archivo FILM!\n"
#define MSGTR_DetectedFLIfile "Detectado formato de archivo FLI!\n"
#define MSGTR_DetectedROQfile "Detectado formato de archivo RoQ!\n"
#define MSGTR_DetectedREALfile "Detectado formato de archivo REAL!\n"
#define MSGTR_DetectedAVIfile "Detectado formato de archivo AVI!\n"
#define MSGTR_DetectedASFfile "Detectado formato de archivo ASF!\n"
#define MSGTR_DetectedMPEGPESfile "Detectado formato de archivo MPEG-PES!\n"
#define MSGTR_DetectedMPEGPSfile "Detectado formato de archivo MPEG-PS!\n"
#define MSGTR_DetectedMPEGESfile "Detectado formato de archivo MPEG-ES!\n"
#define MSGTR_DetectedQTMOVfile "Detectado formato de archivo QuickTime/MOV!\n"
#define MSGTR_MissingMpegVideo "Stream de video MPEG no encontrado!? contacte al autor, puede ser un bug :(\n"
#define MSGTR_InvalidMPEGES "Stream MPEG-ES inválido??? contacte al autor, puede ser un bug :(\n"
#define MSGTR_FormatNotRecognized "============ Disculpe, este formato no está soportado/reconocido =============\n"\
				  "==== Si este archivo es un AVI, ASF o MPEG, por favor contacte al autor! =====\n"
#define MSGTR_MissingVideoStream "No se encontró stream de video!\n"
#define MSGTR_MissingAudioStream "No se encontró stream de audio...  -> sin sonido\n"
#define MSGTR_MissingVideoStreamBug "Stream de video perdido!? Contacte al autor, puede ser un bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: el archivo no contiene el stream de audio o video seleccionado\n"

#define MSGTR_NI_Forced "Forzado"
#define MSGTR_NI_Detected "Detectado"
#define MSGTR_NI_Message "%s formato de AVI 'NON-INTERLEAVED'!\n"

#define MSGTR_UsingNINI "Usando formato de AVI roto 'NON-INTERLEAVED'!\n"
#define MSGTR_CouldntDetFNo "No se puede determinar el número de cuadros (para una búsqueda SOF)\n"
#define MSGTR_CantSeekRawAVI "No se puede avanzar/retroceder en un stream crudo .AVI! (se requiere índice, pruebe con -idx!)  \n"
#define MSGTR_CantSeekFile "No se puede avanzar/retroceder en este archivo!  \n"

#define MSGTR_MOVcomprhdr "MOV: Cabecera comprimida no suportada (por ahora)!\n"
#define MSGTR_MOVvariableFourCC "MOV: Advertencia! FOURCC variable detectada!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Advertencia! demasiadas pistas!"
#define MSGTR_MOVnotyetsupp "\n****** Formato Quicktime MOV todavía no soportado!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "no se pudo abrir codec\n"
#define MSGTR_CantCloseCodec "no se pudo cerrar codec\n"

#define MSGTR_MissingDLLcodec "ERROR: No se pudo abrir el codec DirectShow requerido: %s\n"
#define MSGTR_ACMiniterror "No se puede cargar/inicializar codecs de audio Win32/ACM (falta archivo DLL?)\n"
#define MSGTR_MissingLAVCcodec "No se encuentra codec '%s' en libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayerXP fue compilado SIN soporte para directshow!\n"
#define MSGTR_NoWfvSupport "Soporte para codecs win32 desactivado, o no disponible en plataformas no-x86!\n"
#define MSGTR_NoDivx4Support "MPlayerXP fue compilado SIN soporte para DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP fue compilado SIN soporte lavc/libavcodec!\n"
#define MSGTR_NoACMSupport "Codec Win32/ACM desactivado, o no disponible en plataformas no-x86 -> forzado sin sonido :(\n"
#define MSGTR_NoDShowAudio "Compilado sin soporte para DirectShow -> forzado sin sonido :(\n"
#define MSGTR_NoOggVorbis "Codec de audio OggVorbis desactivado -> forzado sin sonido :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP fue compilado SIN soporte para XAnim!\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF mientras buscaba la cabecera de secuencia\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: No se puede leer cabecera de secuencia!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: No se puede leer la extensión de la cabecera de secuencia!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Mala cabecera de secuencia!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Mala extensión de la cabecera de secuencia!\n"

#define MSGTR_ShMemAllocFail "No se puede alocar memoria compartida\n"
#define MSGTR_OutOfMemory "sin memoria\n"
#define MSGTR_CantAllocAudioBuf "No se puede alocar buffer de la salida de audio\n"
#define MSGTR_NoMemForDecodedImage "no hay memoria suficiente para decodificar el buffer de las imágenes (%ld bytes)\n"

#define MSGTR_AC3notvalid "Stream AC3 inválido.\n"
#define MSGTR_AC3only48k "Sólo streams de 48000 Hz soportados.\n"
#define MSGTR_UnknownAudio "Formato de audio desconocido/perdido, usando sin sonido\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Configurando soporte para lirc ...\n"
#define MSGTR_LIRCdisabled "No podrá usar el control remoto\n"
#define MSGTR_LIRCopenfailed "Falló al abrir el soporte para lirc!\n"
#define MSGTR_LIRCsocketerr "Algo falla con el socket de lirc: %s\n"
#define MSGTR_LIRCcfgerr "Falló al leer archivo de configuración de LIRC %s !\n"
