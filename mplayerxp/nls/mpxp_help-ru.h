/* Translated by:  Nickols_K <nickols_k@mail.ru>
   Was synced with help_mp-en.h: rev 1.20
   UTF-8
 ========================= MPlayer help =========================== */

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (см. DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Запуск:   mplayerxp [опции] [path/]filename",
"",
"Опции:",
" -vo <drv[:dev]> выбор драйвера и устройства видео вывода (список см. с '-vo help')",
" -ao <drv[:dev]> выбор драйвера и устройства аудио вывода (список см. с '-ao help')",
" -play.ss <время>переместиться на заданную (секунды или ЧЧ:ММ:СС) позицию,",
" -audio.off      без звука",
" -video.fs       опции полноэкранного воспроизведения (fullscr,vidmode chg,softw.scale)",
" -sub.file <file>указать файл субтитров",
" -play.list<file> указать playlist",
" -sync.framedrop разрешить потерю кадров (для медленных машин)",
"",
"Ключи:",
" <-  или ->      перемещение вперёд/назад на 10 секунд",
" up или down     перемещение вперёд/назад на  1 минуту",
" < или >         перемещение вперёд/назад в playlist'е",
" p или ПРОБЕЛ    приостановить фильм (любая клавиша - продолжить)",
" q или ESC       остановить воспроизведение и выход",
" o               цикличный перебор OSD режимов:  нет / навигация / навигация+таймер",
" * или /         прибавить или убавить громкость (нажатие 'm' выбирает master/pcm)",
"",
" * * * ПОДРОБНЕЕ СМ. ДОКУМЕНТАЦИЮ, О ДОПОЛНИТЕЛЬНЫХ ОПЦИЯХ И КЛЮЧАХ ! * * *",
NULL
};

// ========================= MPlayer messages ===========================
#define MSGTR_SystemTooSlow_Text 1
static const char* MSGTR_SystemTooSlow[] = {
"",
"  *************************************************************",
"  **** Ваша система слишком МЕДЛЕННА для воспроизведения!  ****",
"  *************************************************************",
"!!! Возможные причины, проблемы, решения:",
"- Наиболее общее: плохой/глючный AUDIO драйвер. Решение: -ao sdl или -ao alsa",
"- Медленная VIDEO карта. Попробуйте различные -vo driver (список: -vo help)",
"   рекомендуется -vo (*)vidix (если доступен) или используйте ключ -framedrop !",
"- Медленный CPU. Не пытайтесь воспроизволить большие dvd/divx на медленных",
"  процессорах! Попробуйте ключ -hardframedrop",
"- Плохой файл. Поиграйте с комбинациями ключей: -nobps  -ni  -mc 0  -forceidx",
"*******************************************************************************",
" Также изучите эти ключи:",
" -video.bm (в настоящее время работает только на некоторых драйверах vidix) [ускоряет на 30%]",
" '/bin/sh hdparm -u1 -d1 -a8 /dev/hdX' [25%]",
" Откажитесь от OSD двойным нажатием клавиши 'O' [5-15%]",
" Также попытайтесь уменьшить число буферов упреждающего декодирования: '-core.da_buffs'",
"*******************************************************************************",
"На многопроцессорных машинах также может помочь: -lavc.slices=0",
NULL
};
#endif

// mplayer.c:

#define MSGTR_Exiting "Выходим"
#define MSGTR_Exit_frames "Запрошенное количество кадров воспроизведено"
#define MSGTR_Exit_quit "Выход"
#define MSGTR_Exit_eof "Конец файла"
#define MSGTR_Fatal_error "Фатальная ошибка"
#define MSGTR_NoHomeDir "Не могу найти HOME каталог"
#define MSGTR_Playing "Воспроизведение"

#define MSGTR_IntBySignal "\nMPlayerXP прерван сигналом %d в модуле: %s \n"
#define MSGTR_GetpathProblem "проблемы в get_path(\"config\")"
#define MSGTR_CreatingCfgFile "Создание файла конфигурации"
#define MSGTR_InvalidVOdriver "Недопустимое имя драйвера видео вывода"
#define MSGTR_InvalidAOdriver "Недопустимое имя драйвера аудио вывода"
#define MSGTR_CopyCodecsConf "(скопируйте etc/codecs.conf (из исходников MPlayerXP) в ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "Не могу загрузить шрифт"
#define MSGTR_CantLoadSub "Не могу загрузить субтитры"
#define MSGTR_ErrorDVDkey "Ошибка обработки DVD КЛЮЧА.\n"
#define MSGTR_CmdlineDVDkey "Коммандная строка DVD требует записанный ключ для дешифрования.\n"
#define MSGTR_DVDauthOk "Авторизация DVD выглядит OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: выбранный поток потерян!\n"
#define MSGTR_CantOpenDumpfile "Не могу открыть файл дампа"
#define MSGTR_StreamDumped "stream dumped :)"
#define MSGTR_FPSnotspecified "Кадр/сек не указаны (или недопустимые) в заголовке! Используйте -fps опцию!\n"
#define MSGTR_NoVideoStream "Видео поток не найден... это невоспроизводимо пока\n"
#define MSGTR_TryForceAudioFmt "Попытка форсировать семейство аудио кодеков"
#define MSGTR_CantFindAfmtFallback "Не могу найти аудио кодек для форсированного семейства, переход на другие драйвера.\n"
#define MSGTR_CantFindAudioCodec "Не могу найти кодек для аудио формата"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Попытайтесь обновить его из etc/codecs.conf*** Если не помогло - читайте DOCS/codecs.html!"
#define MSGTR_CouldntInitAudioCodec "Не смог проинициализировать аудио кодек! -> без звука\n"
#define MSGTR_TryForceVideoFmt "Попытка форсировать семейство видео кодеков"
#define MSGTR_CantFindVfmtFallback "Не могу найти видео кодек для форсированного семейства, переход на другие драйвера.\n"
#define MSGTR_CantFindVideoCodec "Не могу найти кодек для видео формата"
#define MSGTR_VOincompCodec "Извините, выбранное video_out устройство не совместимо с этим кодеком."
#define MSGTR_CouldntInitVideoCodec "FATAL: Не смог проинициализировать видео кодек :(\n"
#define MSGTR_EncodeFileExists "Файл уже существует: %s (не переписывайте Ваш любимый AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Не могу создать файл для кодирования\n"
#define MSGTR_CannotInitVO "FATAL: Не могу проинициализировать видео драйвер!"
#define MSGTR_CannotInitAO "не могу открыть/проинициализировать аудио устройство -> БЕЗ ЗВУКА\n"

#define MSGTR_CODEC_INITIAL_AV_RESYNC "i_bps==0!!! Возможна начальная A-V рассинхронизация\nИспользуйте '-' и '+' клавиши чтобы её подавить"
#define MSGTR_CODEC_CANT_INITA "Не смог проинициализировать ADecoder :(\n"
#define MSGTR_CODEC_CANT_INITV "Не смог проинициализировать VDecoder :("
#define MSGTR_CODEC_CANT_PREINITA "Не смог проинициализировать ADecoder :(\n"
#define MSGTR_CODEC_BAD_AFAMILY "Требуемое семейство аудио кодеков не доступно"
#define MSGTR_CODEC_XP_INT_ERR "decaudio: внутренняя ошибка xp-ядра. Подготовтесь к sig11"
#define MSGTR_CODEC_BUF_OVERFLOW "AC вызвал переполнение буфера. Подготовтесь к sig11"
#define MSGTR_CODEC_CANT_LOAD_DLL "Не могу загрузить библиотеку"
#define MSGTR_CODEC_DLL_HINT "Попытайтесь получить этот кодек с"
#define MSGTR_CODEC_DLL_OK "Библиотека загружена успешно"
#define MSGTR_CODEC_DLL_SYM_ERR "Не могу найти символ"

#define MSGTR_NoSound "Аудио: без звука!!!\n"
#define MSGTR_FPSforced "Кадры/сек форсированы в"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM '%s' не найден!\n"
#define MSGTR_ErrTrackSelect "Ошибка выбора трека VCD!"
#define MSGTR_ReadSTDIN "Чтение из stdin...\n"
#define MSGTR_UnableOpenURL "Не могу открыть URL"
#define MSGTR_ConnToServer "Соединение с сервером"
#define MSGTR_ConnTimeout "Задержка соединения\n"
#define MSGTR_ConnInterrupt "Соединение прервано пользователем\n"
#define MSGTR_ConnError "Ошибка связи: %s\n"
#define MSGTR_ConnAuthFailed "Ошибка аутентификации. Используйте опции -user и -passwd или URL в форме: http://username:password@hostname/file"
#define MSGTR_FileNotFound "Файл не найден: '%s'\n"

#define MSGTR_CantOpenDVD "Не смог открыть DVD"
#define MSGTR_DVDwait "Чтение структуры диска, подождите пожалуйста"
#define MSGTR_DVDnumTitles "Есть %d титров на этом DVD"
#define MSGTR_DVDinvalidTitle "Недопустимый номер DVD титра"
#define MSGTR_DVDnumChapters "Титр #%i имеет %d глав"
#define MSGTR_DVDinvalidChapter "Недопустимый номер DVD главы"
#define MSGTR_DVDnumAngles "Есть %d углов в этом DVD титре"
#define MSGTR_DVDinvalidAngle "Недопустимый номер DVD угла"
#define MSGTR_DVDnoIFO "Не могу открыть IFO файл для DVD титра"
#define MSGTR_DVDnoVOBs "Не могу открыть титр VOBS"
#define MSGTR_DVDopenOk "DVD успешно открыт"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Предупреждение! Заголовок аудио потока переопределён"
#define MSGTR_VideoStreamRedefined "Предупреждение! Заголовок видео потока переопределён"
#define MSGTR_SubStreamRedefined   "Предупреждение! Заголовок потока субтитров переопределён"
#define MSGTR_TooManyAudioInBuffer "Слишком много аудио пакетов в буфере"
#define MSGTR_TooManyVideoInBuffer "Слишком много видео пакетов в буфере"
#define MSGTR_MaybeNI "возможно Вы воспроизводите нечередованный поток/файл или неудачный кодек"
#define MSGTR_FormatNotRecognized "========= Извините, формат этого файла не распознан/не поддерживается ==========="
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
#define MSGTR_CantOpenCodec "Не смог открыть кодек"
#define MSGTR_CantCloseCodec "Не смог закрыть кодек"

#define MSGTR_MissingDLLcodec "ОШИБКА: Не смог открыть требующийся DirectShow кодек: %s\n"
#define MSGTR_ACMiniterror "Не смог загрузить/проинициализировать Win32/ACM AUDIO кодек (потерян DLL файл?)\n"
#define MSGTR_MissingLAVCcodec "[libavcodec] Не могу найти кодек "

#define MSGTR_NoDShowSupport "MPlayerXP был скомпилён БЕЗ поддержки directshow!\n"
#define MSGTR_NoWfvSupport "Поддержка для win32 кодеков запрещена или недоступна на не-x86 платформах!\n"
#define MSGTR_NoDivx4Support "MPlayerXP был скомпилён БЕЗ поддержки DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP был скомпилён БЕЗ поддержки lavc/libavcodec!\n"
#define MSGTR_NoACMSupport "Win32/ACM аудио кодек запрещён, или недоступен на не-x86 ЦПУ -> блокируйте звук :(\n"
#define MSGTR_NoDShowAudio "Скомпилён без поддержки DirectShow -> блокируйте звук :(\n"
#define MSGTR_NoOggVorbis "OggVorbis аудио кодек запрещён -> блокируйте звук :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP был скомпилён БЕЗ поддержки XAnim!\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: КОНЕЦ ФАЙЛА при поиске последовательности заголовков\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Не могу читать последовательность заголовков!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Не мочу читать расширение последовательности заголовов!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Плохая последовательность заголовков!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Плохое расширение последовательности заголовков!\n"

#define MSGTR_ShMemAllocFail "Не могу захватить разделяемую память"
#define MSGTR_OutOfMemory "Нехватает памяти"
#define MSGTR_CantAllocAudioBuf "Не могу захватить выходной буффер аудио"
#define MSGTR_NoMemForDecodedImage "Не достаточно памяти для буффера декодирования картинки (%ld байт)\n"

#define MSGTR_AC3notvalid "Не допустимый AC3 поток.\n"
#define MSGTR_AC3only48k "Поддерживается только 48000 Hz потоки.\n"
#define MSGTR_UnknownAudio "Неизвестный/потерянный аудио формат, отказ от звука"

// LIRC:
#define MSGTR_SettingUpLIRC "Установка поддержки lirc..."
#define MSGTR_LIRCdisabled "Вы не сможете использовать Ваше удалённое управление"
#define MSGTR_LIRCopenfailed "Неудачное открытие поддержки lirc!"
#define MSGTR_LIRCsocketerr "Что-то неправильно с сокетом lirc"
#define MSGTR_LIRCcfgerr "Неудачное чтение файла конфигурации LIRC"
