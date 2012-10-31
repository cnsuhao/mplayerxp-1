/* Translated by:  Nickols_K <nickols_k@mail.ru>
   Was synced with help_mp-en.h: rev 1.20
   UTF-8
 ========================= MPlayer help =========================== */

#ifdef HELP_MP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (см. DOCS!)\n"
"\n";

static const char help_text[]=
"Запуск:   mplayerxp [опции] [path/]filename\n"
"\n"
"Опции:\n"
" -vo <drv[:dev]> выбор драйвера и устройства видео вывода (список см. с '-vo help')\n"
" -ao <drv[:dev]> выбор драйвера и устройства аудио вывода (список см. с '-ao help')\n"
" -play.ss <время>переместиться на заданную (секунды или ЧЧ:ММ:СС) позицию\n"
" -audio.off      без звука\n"
" -video.fs       опции полноэкранного проигрывания (fullscr,vidmode chg,softw.scale)\n"
" -sub.file <file>указать файл субтитров\n"
" -play.list<file> указать playlist\n"
" -sync.framedrop разрешить потерю кадров (для медленных машин)\n"
"\n"
"Ключи:\n"
" <-  или ->      перемещение вперёд/назад на 10 секунд\n"
" up или down     перемещение вперёд/назад на  1 минуту\n"
" < или >         перемещение вперёд/назад в playlist'е\n"
" p или ПРОБЕЛ    приостановить фильм (любая клавиша - продолжить)\n"
" q или ESC       остановить воспроизведение и выход\n"
" o               цикличный перебор OSD режимов:  нет / навигация / навигация+таймер\n"
" * или /         прибавить или убавить громкость (нажатие 'm' выбирает master/pcm)\n"
"\n"
" * * * ПОДРОБНЕЕ СМ. ДОКУМЕНТАЦИЮ, О ДОПОЛНИТЕЛЬНЫХ ОПЦИЯХ И КЛЮЧАХ ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================
#define MSGTR_SystemTooSlow "\n\n"\
"  *************************************************************\n"\
"  **** Ваша система слишком МЕДЛЕННА для воспроизведения!  ****\n"\
"  *************************************************************\n"\
"!!! Возможные причины, проблемы, решения:\n"\
"- Наиболее общее: плохой/глючный AUDIO драйвер. Решение: -ao sdl или -ao alsa\n"\
"- Медленная VIDEO карта. Попробуйте различные -vo driver (список: -vo help)\n"\
"   рекомендуется -vo (*)vidix (если доступен) или используйте ключ -framedrop !\n"\
"- Медленный CPU. Не пытайтесь воспроизволить большие dvd/divx на медленных"\
"  процессорах! Попробуйте ключ -hardframedrop\n"\
"- Плохой файл. Поиграйте с комбинациями ключей: -nobps  -ni  -mc 0  -forceidx\n"\
"*******************************************************************************\n"\
" Также изучите эти ключи:\n"\
" -video.bm (в настоящее время работает только на некоторых драйверах vidix) [ускоряет на 30%]\n"\
" '/bin/sh hdparm -u1 -d1 -a8 /dev/hdX' [25%]\n"\
" Откажитесь от OSD двойным нажатием клавиши 'O' [5-15%]\n"\
" Также попытайтесь уменьшить число буферов упреждающего декодирования: '-core.da_buffs'\n"\
"*******************************************************************************\n"\
"На многопроцессорных машинах также может помочь: -ffmpeg.slices=0\n"

// mplayer.c:

#define MSGTR_Exiting "\nВыходим... (%s)\n"
#define MSGTR_Exit_frames "Запрошенное количество кадров проиграно"
#define MSGTR_Exit_quit "Выход"
#define MSGTR_Exit_eof "Конец файла"
#define MSGTR_Exit_error "Фатальная ошибка"
#define MSGTR_IntBySignal "\nMPlayerXP прерван сигналом %d в модуле: %s \n"
#define MSGTR_NoHomeDir "Не могу найти HOME каталог\n"
#define MSGTR_GetpathProblem "проблемы в get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Создание файла конфигурации: %s\n"
#define MSGTR_InvalidVOdriver "Недопустимое имя драйвера видео вывода: %s\nСм. '-vo help' чтобы получить список доступных драйверов.\n"
#define MSGTR_InvalidAOdriver "Недопустимое имя драйвера аудио вывода: %s\nСм. '-ao help' чтобы получить список доступных драйверов.\n"
#define MSGTR_CopyCodecsConf "(скопируйте etc/codecs.conf (из исходников MPlayerXP) в ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "Не могу загрузить шрифт: %s\n"
#define MSGTR_CantLoadSub "Не могу загрузить субтитры: %s\n"
#define MSGTR_ErrorDVDkey "Ошибка обработки DVD КЛЮЧА.\n"
#define MSGTR_CmdlineDVDkey "Коммандная строка DVD требует записанный ключ для дешифрования.\n"
#define MSGTR_DVDauthOk "Авторизация DVD выглядит OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: выбранный поток потерян!\n"
#define MSGTR_CantOpenDumpfile "Не могу открыть файл дампа!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "Кадр/сек не указаны (или недопустимые) в заголовке! Используйте -fps опцию!\n"
#define MSGTR_NoVideoStream "Видео поток не найден... это невоспроизводимо пока\n"
#define MSGTR_TryForceAudioFmt "Попытка форсировать семейство аудио кодеков '%s' ...\n"
#define MSGTR_CantFindAfmtFallback "Не могу найти аудио кодек для форсированного семейства, переход на другие драйвера.\n"
#define MSGTR_CantFindAudioCodec "Не могу найти кодек для аудио формата"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Попытайтесь обновить %s из etc/codecs.conf\n*** Если не помогло - читайте DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Не смог проинициализировать аудио кодек! -> без звука\n"
#define MSGTR_TryForceVideoFmt "Попытка форсировать семейство видео кодеков '%s' ...\n"
#define MSGTR_CantFindVfmtFallback "Не могу найти видео кодек для форсированного семейства, переход на другие драйвера.\n"
#define MSGTR_CantFindVideoCodec "Не могу найти кодек для видео формата"
#define MSGTR_VOincompCodec "Sorry, выбранное video_out устройство не совместимо с этим кодеком.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Не смог проинициализировать видео кодек :(\n"
#define MSGTR_EncodeFileExists "Файл уже существует: %s (не переписывайте Ваш любимый AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Не могу создать файл для кодирования\n"
#define MSGTR_CannotInitVO "FATAL: Не могу проинициализировать видео драйвер!\n"
#define MSGTR_CannotInitAO "не могу открыть/проинициализировать аудио устройство -> БЕЗ ЗВУКА\n"
#define MSGTR_StartPlaying "Начало воcпроизведения...\n"

#define MSGTR_CODEC_INITAL_AV_RESYNC "i_bps==0!!! Возможна начальная A-V рассинхронизация\nИспользуйте '-' и '+' клавиши чтобы её подавить\n"
#define MSGTR_CODEC_CANT_INITA "Не смог проинициализировать ADecoder :(\n"
#define MSGTR_CODEC_CANT_INITV "Не смог проинициализировать VDecoder :(\n"
#define MSGTR_CODEC_CANT_PREINITA "Не смог проинициализировать ADecoder :(\n"
#define MSGTR_CODEC_BAD_AFAMILY "Требуемое семейство аудио кодеков [%s] (afm=%s) не доступно (разрешите его во время компиляции!)\n"
#define MSGTR_CODEC_XP_INT_ERR "decaudio: внутренняя ошибка xp-ядра %i>%i. Подготовтесь к sig11...\n"
#define MSGTR_CODEC_BUF_OVERFLOW "AC [%s] вызвал переполнение буфера %i>%i. Подготовтесь к sig11...\n"
#define MSGTR_CODEC_CANT_LOAD_DLL "codec_ld: Не могу загрузить библиотеку: '%s' по причине: %s\n"
#define MSGTR_CODEC_DLL_HINT "codec_ld: Попытайтесь получить этот кодек с: %s\n"
#define MSGTR_CODEC_DLL_OK "codec_ld: Библиотека: '%s' загружена успешно\n"
#define MSGTR_CODEC_DLL_SYM_ERR "Не могу найти символ: %s\n"

#define MSGTR_Playing "Проигрывание %s\n"
#define MSGTR_NoSound "Аудио: без звука!!!\n"
#define MSGTR_FPSforced "Кадры/сек форсированы в %5.3f (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM '%s' не найден!\n"
#define MSGTR_ErrTrackSelect "Ошибка выбора трека VCD!"
#define MSGTR_ReadSTDIN "Чтение из stdin...\n"
#define MSGTR_UnableOpenURL "Не могу открыть URL: %s\n"
#define MSGTR_ConnToServer "Соединение с сервером: %s\n"
#define MSGTR_ConnTimeout "Задержка соединения\n"
#define MSGTR_ConnInterrupt "Соединение прервано пользователем\n"
#define MSGTR_ConnError "Ошибка связи: %s\n"
#define MSGTR_ConnAuthFailed "Ошибка аутентификации\n"\
			    "Используйте опции -user и -passwd чтобы предоставить Ваши username/password для списка URLs,\n"\
			    "или URL в форме: http://username:password@hostname/file\n"
#define MSGTR_FileNotFound "Файл не найден: '%s'\n"

#define MSGTR_CantOpenDVD "Не смог открыть DVD: %s\n"
#define MSGTR_DVDwait "Чтение структуры диска, подождите пожалуйста...\n"
#define MSGTR_DVDnumTitles "Есть %d титров на этом DVD.\n"
#define MSGTR_DVDinvalidTitle "Недопустимый номер DVD титра: %d\n"
#define MSGTR_DVDnumChapters "Титр #%i имеет %d глав.\n"
#define MSGTR_DVDinvalidChapter "Недопустимый номер DVD главы: %d\n"
#define MSGTR_DVDnumAngles "Есть %d углов в этом DVD титре.\n"
#define MSGTR_DVDinvalidAngle "Недопустимый номер DVD угла: %d\n"
#define MSGTR_DVDnoIFO "Не могу открыть IFO файл для DVD титра %d.\n"
#define MSGTR_DVDnoVOBs "Не могу открыть титр VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD успешно открыт!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Предупреждение! Заголовок аудио потока %d переопределён!\n"
#define MSGTR_VideoStreamRedefined "Предупреждение! Заголовок видео потока %d переопределён!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Слишком много (%d в %d байтах) аудио пакетов в буфере!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Слишком много (%d в %d байтах) видео пакетов в буфере!\n"
#define MSGTR_MaybeNI "(возможно Вы проигрываете нечередованный поток/файл или неудачный кодек)\n"
#define MSGTR_DetectedFILMfile "Обнаружен FILM формат файла!\n"
#define MSGTR_DetectedFLIfile "Обнаружен FLI формат файла!\n"
#define MSGTR_DetectedROQfile "Обнаружен RoQ формат файла!\n"
#define MSGTR_DetectedREALfile "Обнаружен REAL формат файла!\n"
#define MSGTR_DetectedAVIfile "Обнаружен AVI формат файла!\n"
#define MSGTR_DetectedASFfile "Обнаружен ASF формат файла!\n"
#define MSGTR_DetectedMPEGPESfile "Обнаружен MPEG-PES формат файла!\n"
#define MSGTR_DetectedMPEGPSfile "Обнаружен MPEG-PS формат файла!\n"
#define MSGTR_DetectedMPEGESfile "Обнаружен MPEG-ES формат файла!\n"
#define MSGTR_DetectedQTMOVfile "Обнаружен QuickTime/MOV формат файла!\n"
#define MSGTR_MissingMpegVideo "MPEG видео поток потерян!? свяжитесь с автором, это может быть багом :(\n"
#define MSGTR_InvalidMPEGES "Недопустимый MPEG-ES поток??? свяжитесь с автором, это может быть багом :(\n"
#define MSGTR_FormatNotRecognized "========= Sorry, формат этого файла не распознан/не поддерживается ===========\n"\
				  "===== Если это AVI, ASF или MPEG поток, пожалуйста свяжитесь с автором! ======\n"
#define MSGTR_MissingVideoStream "Видео поток не найден!\n"
#define MSGTR_MissingAudioStream "Аудио поток не найден...  ->без звука\n"
#define MSGTR_MissingVideoStreamBug "Видео поток потерян!? свяжитесь с автором, это может быть багом :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: файл не содержит выбранный аудио или видео поток\n"

#define MSGTR_NI_Forced "Форсирован"
#define MSGTR_NI_Detected "Обнаружен"
#define MSGTR_NI_Message "%s НЕЧЕРЕДОВАННЫЙ формат AVI файла!\n"

#define MSGTR_UsingNINI "Использование НЕЧЕРЕДОВАННОГО испорченного формата AVI файла!\n"
#define MSGTR_CouldntDetFNo "Не смог определить число кадров (для абсолютного перемещения)\n"
#define MSGTR_CantSeekRawAVI "Не могу переместиться в сыром потоке .AVI! (требуется индекс, попробуйте с ключом -idx!)\n"
#define MSGTR_CantSeekFile "Не могу перемещаться в этом файле!\n"

#define MSGTR_MOVcomprhdr "MOV: Сжатые заголовки (пока) не поддерживаются!\n"
#define MSGTR_MOVvariableFourCC "MOV: Предупреждение! Обнаружен переменный FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Предупреждение! слишком много треков!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV формат пока не поддерживается!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Не смог открыть кодек\n"
#define MSGTR_CantCloseCodec "Не смог закрыть кодек\n"

#define MSGTR_MissingDLLcodec "ОШИБКА: Не смог открыть требующийся DirectShow кодек: %s\n"
#define MSGTR_ACMiniterror "Не смог загрузить/проинициализировать Win32/ACM AUDIO кодек (потерян DLL файл?)\n"
#define MSGTR_MissingLAVCcodec "Не могу найти кодек '%s' в libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayerXP был скомпилён БЕЗ поддержки directshow!\n"
#define MSGTR_NoWfvSupport "Поддержка для win32 кодеков запрещена или недоступна на не-x86 платформах!\n"
#define MSGTR_NoDivx4Support "MPlayerXP был скомпилён БЕЗ поддержки DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP был скомпилён БЕЗ поддержки ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Win32/ACM аудио кодек запрещён, или недоступен на не-x86 ЦПУ -> блокируйте звук :(\n"
#define MSGTR_NoDShowAudio "Скомпилён без поддержки DirectShow -> блокируйте звук :(\n"
#define MSGTR_NoOggVorbis "OggVorbis аудио кодек запрещён -> блокируйте звук :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP был скомпилён БЕЗ поддержки XAnim!\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: КОНЕЦ ФАЙЛА при поиске последовательности заголовков\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Не могу читать последовательность заголовков!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Не мочу читать расширение последовательности заголовов!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Плохая последовательность заголовков!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Плохое расширение последовательности заголовков!\n"

#define MSGTR_ShMemAllocFail "Не могу захватить общую память\n"
#define MSGTR_OutOfMemory "Нехватает памяти\n"
#define MSGTR_CantAllocAudioBuf "Не могу захватить выходной буффер аудио\n"
#define MSGTR_NoMemForDecodedImage "Не достаточно памяти для буффера декодирования картинки (%ld байт)\n"

#define MSGTR_AC3notvalid "Не допустимый AC3 поток.\n"
#define MSGTR_AC3only48k "Поддерживается только 48000 Hz потоки.\n"
#define MSGTR_UnknownAudio "Неизвестный/потерянный аудио формат, отказ от звука\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Установка поддержки lirc...\n"
#define MSGTR_LIRCdisabled "Вы не сможете использовать Ваше удалённое управление\n"
#define MSGTR_LIRCopenfailed "Неудачное открытие поддержки lirc!\n"
#define MSGTR_LIRCsocketerr "Что-то неправильно с сокетом lirc: %s\n"
#define MSGTR_LIRCcfgerr "Неудачное чтение файла конфигурации LIRC %s !\n"
