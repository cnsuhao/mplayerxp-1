#ifndef __MPXP_EN_HELP_INCLUDED
#define __MPXP_EN_HELP_INCLUDED 1
/* MASTER FILE. Use this file as base for translation!

 Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
 and send a notify message to mplayer-dev-eng maillist.

 ========================= MPlayerXP help =========================== */

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
"Usage:   mplayerxp [options] [path/]filename",
"",
"Options:",
" -vo <drv[:dev]> select video output driver & device (see '-vo help' for list)",
" -ao <drv[:dev]> select audio output driver & device (see '-ao help' for list)",
" -play.ss <timepos> seek to given (seconds or hh:mm:ss) position",
" -audio.off      don't play sound",
" -video.fs       fullscreen playing options (fullscr,vidmode chg,softw.scale),"
" -sub.file <file> specify subtitle file to use",
" -play.list<file> specify playlist file",
" -sync.framedrop  enable frame-dropping (for slow machines)",
"",
"Keys:",
" <-  or  ->      seek backward/forward 10 seconds",
" up or down      seek backward/forward  1 minute",
" < or >          seek backward/forward in playlist",
" p or SPACE      pause movie (press any key to continue)",
" q or ESC        stop playing and quit program",
" o               cycle OSD mode:  none / seekbar / seekbar+timer",
" * or /          increase or decrease volume (press 'm' to select master/pcm)",
"",
" * * * SEE MANPAGE FOR DETAILS, FURTHER (ADVANCED) OPTIONS AND KEYS ! * * *",
NULL
};
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#ifndef MSGTR_SystemTooSlow_Text
static const char* MSGTR_SystemTooSlow[] = {
"",
"         ************************************************",
"         **** Your system is too SLOW to play this!  ****",
"         ************************************************",
"!!! Possible reasons, problems, workaround:",
"- Most common: broken/buggy AUDIO driver. Workaround: -ao sdl or -ao alsa",
"- Slow VIDEO card. Try different -vo driver (for list: -vo help)",
"   -vo (*)vidix is recommended (if available) or try with -framedrop !",
"- Slow cpu. Don't try to playback big dvd/divx on slow cpu! try -hardframedrop",
"- Broken file. Try various combinations of these: -nobps  -ni  -mc 0  -forceidx",
"*******************************************************************************",
" Also try study these keys:",
" -video.bm (currently may work with some vidix drivers only) [speedup upto 30%]",
" '/bin/sh hdparm -u1 -d1 -a8 /dev/hdX' [25%]",
" Try disable OSD by pressing 'O' key twice [5-15%]",
"Also try to decrease the number of buffers for decoding ahead: '-core.da_buffs'",
"*******************************************************************************",
"On SMP -lavc.slices=0 may help too",
NULL
};
#endif
#endif

#ifndef MSGTR_Exiting
#define MSGTR_Exiting "Exiting"
#endif
#ifndef MSGTR_Exit_frames
#define MSGTR_Exit_frames "Requested number of frames played"
#endif
#ifndef MSGTR_Exit_quit
#define MSGTR_Exit_quit "Quit"
#endif
#ifndef MSGTR_Exit_eof
#define MSGTR_Exit_eof "End of file"
#endif
#ifndef MSGTR_Fatal_error
#define MSGTR_Fatal_error "Fatal error"
#endif
#ifndef MSGTR_NoHomeDir
#define MSGTR_NoHomeDir "Can't find HOME dir"
#endif
#ifndef MSGTR_Playing
#define MSGTR_Playing "Playing"
#endif


#ifndef MSGTR_IntBySignal
#define MSGTR_IntBySignal "\nMPlayerXP interrupted by signal %d in module: %s \n"
#endif
#ifndef MSGTR_GetpathProblem
#define MSGTR_GetpathProblem "get_path(\"config\") problem"
#endif
#ifndef MSGTR_CreatingCfgFile
#define MSGTR_CreatingCfgFile "Creating config file"
#endif
#ifndef MSGTR_InvalidVOdriver
#define MSGTR_InvalidVOdriver "Invalid video output driver name"
#endif
#ifndef MSGTR_InvalidAOdriver
#define MSGTR_InvalidAOdriver "Invalid audio output driver name"
#endif
#ifndef MSGTR_CopyCodecsConf
#define MSGTR_CopyCodecsConf "(copy/ln etc/codecs.conf (from MPlayerXP source tree) to ~/.mplayerxp/codecs.conf)\n"
#endif
#ifndef MSGTR_CantLoadFont
#define MSGTR_CantLoadFont "Can't load font"
#endif
#ifndef MSGTR_CantLoadSub
#define MSGTR_CantLoadSub "Can't load subtitles"
#endif
#ifndef MSGTR_ErrorDVDkey
#define MSGTR_ErrorDVDkey "Error processing DVD KEY.\n"
#endif
#ifndef MSGTR_CmdlineDVDkey
#define MSGTR_CmdlineDVDkey "DVD command line requested key is stored for descrambling.\n"
#endif
#ifndef MSGTR_DVDauthOk
#define MSGTR_DVDauthOk "DVD auth sequence seems to be OK.\n"
#endif
#ifndef MSGTR_DumpSelectedSteramMissing
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: selected stream missing!\n"
#endif
#ifndef MSGTR_CantOpenDumpfile
#define MSGTR_CantOpenDumpfile "Can't open dump file!!!\n"
#endif
#ifndef MSGTR_StreamDumped
#define MSGTR_StreamDumped "stream dumped :)\n"
#endif
#ifndef MSGTR_FPSnotspecified
#define MSGTR_FPSnotspecified "FPS not specified (or invalid) in the header! Use the -fps option!\n"
#endif
#ifndef MSGTR_NoVideoStream
#define MSGTR_NoVideoStream "Sorry, no video stream... it's unplayable yet\n"
#endif
#ifndef MSGTR_TryForceAudioFmt
#define MSGTR_TryForceAudioFmt "Trying to force audio codec driver family"
#endif
#ifndef MSGTR_CantFindAfmtFallback
#define MSGTR_CantFindAfmtFallback "Can't find audio codec for forced driver family, fallback to other drivers.\n"
#endif
#ifndef MSGTR_CantFindAudioCodec
#define MSGTR_CantFindAudioCodec "Can't find codec for audio format"
#endif
#ifndef MSGTR_TryUpgradeCodecsConfOrRTFM
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Try to upgrade it from etc/codecs.conf*** If it's still not OK, then read DOCS/codecs.html!"
#endif
#ifndef MSGTR_CouldntInitAudioCodec
#define MSGTR_CouldntInitAudioCodec "Couldn't initialize audio codec! -> nosound\n"
#endif
#ifndef MSGTR_TryForceVideoFmt
#define MSGTR_TryForceVideoFmt "Trying to force video codec driver family"
#endif
#ifndef MSGTR_CantFindVfmtFallback
#define MSGTR_CantFindVfmtFallback "Can't find video codec for forced driver family, fallback to other drivers.\n"
#endif
#ifndef MSGTR_CantFindVideoCodec
#define MSGTR_CantFindVideoCodec "Can't find codec for video format"
#endif
#ifndef MSGTR_VOincompCodec
#define MSGTR_VOincompCodec "Sorry, selected video_out device is incompatible with this codec.\n"
#endif
#ifndef MSGTR_CouldntInitVideoCodec
#define MSGTR_CouldntInitVideoCodec "FATAL: Couldn't initialize video codec :(\n"
#endif
#ifndef MSGTR_EncodeFileExists
#define MSGTR_EncodeFileExists "File already exists: %s (don't overwrite your favourite AVI!)\n"
#endif
#ifndef MSGTR_CantCreateEncodeFile
#define MSGTR_CantCreateEncodeFile "Cannot create file for encoding\n"
#endif
#ifndef MSGTR_CannotInitVO
#define MSGTR_CannotInitVO "FATAL: Cannot initialize video driver!\n"
#endif
#ifndef MSGTR_CannotInitAO
#define MSGTR_CannotInitAO "couldn't open/init audio device -> NOSOUND\n"
#endif

#ifndef MSGTR_CODEC_INITAL_AV_RESYNC
#define MSGTR_CODEC_INITAL_AV_RESYNC "i_bps==0!!! You may have initial A-V resync\nUse '-' and '+' keys to supress that\n"
#endif
#ifndef MSGTR_CODEC_CANT_INITA
#define MSGTR_CODEC_CANT_INITA "ADecoder init failed :(\n"
#endif
#ifndef MSGTR_CODEC_CANT_INITV
#define MSGTR_CODEC_CANT_INITV "VDecoder init failed :(\n"
#endif
#ifndef MSGTR_CODEC_CANT_PREINITA
#define MSGTR_CODEC_CANT_PREINITA "ADecoder preinit failed :(\n"
#endif
#ifndef MSGTR_CODEC_BAD_AFAMILY
#define MSGTR_CODEC_BAD_AFAMILY "Requested audio codec family [%s] (afm=%s) not available (enable it at compile time!)\n"
#endif
#ifndef MSGTR_CODEC_XP_INT_ERR
#define MSGTR_CODEC_XP_INT_ERR "decaudio: xp-core internal error %i>%i. Be ready to sig11...\n"
#endif
#ifndef MSGTR_CODEC_BUF_OVERFLOW
#define MSGTR_CODEC_BUF_OVERFLOW "AC [%s] causes buffer overflow %i>%i. Be ready to sig11...\n"
#endif
#ifndef MSGTR_CODEC_CANT_LOAD_DLL
#define MSGTR_CODEC_CANT_LOAD_DLL "Can't load library"
#endif
#ifndef MSGTR_CODEC_DLL_HINT
#define MSGTR_CODEC_DLL_HINT "Try obtain this codec at"
#endif
#ifndef MSGTR_CODEC_DLL_OK
#define MSGTR_CODEC_DLL_OK "The library was loaded OK"
#endif
#ifndef MSGTR_CODEC_DLL_SYM_ERR
#define MSGTR_CODEC_DLL_SYM_ERR "Can't resolve symbol"
#endif
//#define MSGTR_NoGui "MPlayer was compiled WITHOUT GUI support!\n"
//#define MSGTR_GuiNeedsX "MPlayer GUI requires X11!\n"
#ifndef MSGTR_NoSound
#define MSGTR_NoSound "Audio: no sound!!!\n"
#endif
#ifndef MSGTR_FPSforced
#define MSGTR_FPSforced "FPS forced to be"
#endif

// open.c, stream.c:
#ifndef MSGTR_CdDevNotfound
#define MSGTR_CdDevNotfound "CD-ROM Device '%s' not found!\n"
#endif
#ifndef MSGTR_ErrTrackSelect
#define MSGTR_ErrTrackSelect "Error selecting VCD track!"
#endif
#ifndef MSGTR_ReadSTDIN
#define MSGTR_ReadSTDIN "Reading from stdin...\n"
#endif
#ifndef MSGTR_UnableOpenURL
#define MSGTR_UnableOpenURL "Unable to open URL"
#endif
#ifndef MSGTR_ConnToServer
#define MSGTR_ConnToServer "Connected to server"
#endif
#ifndef MSGTR_ConnTimeout
#define MSGTR_ConnTimeout "Connection timeout\n"
#endif
#ifndef MSGTR_ConnInterrupt
#define MSGTR_ConnInterrupt "Connection interuppted by user\n"
#endif
#ifndef MSGTR_ConnError
#define MSGTR_ConnError "Connect error: %s\n"
#endif
#ifndef MSGTR_ConnAuthFailed
#define MSGTR_ConnAuthFailed "Authentication failed. Please use the option -user and -passwd or form an URL like: http://username:password@hostname/file"
#endif
#ifndef MSGTR_FileNotFound
#define MSGTR_FileNotFound "File not found: '%s'\n"
#endif

#ifndef MSGTR_CantOpenDVD
#define MSGTR_CantOpenDVD "Couldn't open DVD device"
#endif
#ifndef MSGTR_DVDwait
#define MSGTR_DVDwait "Reading disc structure, please wait"
#endif
#ifndef MSGTR_DVDnumTitles
#define MSGTR_DVDnumTitles "There are %d titles on this DVD"
#endif
#ifndef MSGTR_DVDinvalidTitle
#define MSGTR_DVDinvalidTitle "Invalid DVD title number"
#endif
#ifndef MSGTR_DVDnumChapters
#define MSGTR_DVDnumChapters "Title #%i has %d chapters"
#endif
#ifndef MSGTR_DVDinvalidChapter
#define MSGTR_DVDinvalidChapter "Invalid DVD chapter number"
#endif
#ifndef MSGTR_DVDnumAngles
#define MSGTR_DVDnumAngles "There are %d angles in this DVD title"
#endif
#ifndef MSGTR_DVDinvalidAngle
#define MSGTR_DVDinvalidAngle "Invalid DVD angle number"
#endif
#ifndef MSGTR_DVDnoIFO
#define MSGTR_DVDnoIFO "Can't open the IFO file for DVD title"
#endif
#ifndef MSGTR_DVDnoVOBs
#define MSGTR_DVDnoVOBs "Can't open title VOBS"
#endif
#ifndef MSGTR_DVDopenOk
#define MSGTR_DVDopenOk "DVD successfully opened"
#endif

// demuxer.c, demux_*.c:
#ifndef MSGTR_AudioStreamRedefined
#define MSGTR_AudioStreamRedefined "Warning! Audio stream header redefined"
#endif
#ifndef MSGTR_VideoStreamRedefined
#define MSGTR_VideoStreamRedefined "Warning! video stream header redefined"
#endif
#ifndef MSGTR_SubStreamRedefined
#define MSGTR_SubStreamRedefined "Warning! subtitle stream header redefined"
#endif
#ifndef MSGTR_TooManyAudioInBuffer
#define MSGTR_TooManyAudioInBuffer "Too many audio packets in the buffer"
#endif
#ifndef MSGTR_TooManyVideoInBuffer
#define MSGTR_TooManyVideoInBuffer "Too many video packets in the buffer"
#endif
#ifndef MSGTR_MaybeNI
#define MSGTR_MaybeNI "maybe you play a non-interleaved stream/file or the codec failed"
#endif
#ifndef MSGTR_FormatNotRecognized
#define MSGTR_FormatNotRecognized "============= Sorry, this file format not recognized/supported ==============="
#endif
#ifndef MSGTR_MissingVideoStream
#define MSGTR_MissingVideoStream "No video stream found!\n"
#endif
#ifndef MSGTR_MissingAudioStream
#define MSGTR_MissingAudioStream "No Audio stream found...  ->nosound\n"
#endif
#ifndef MSGTR_MissingVideoStreamBug
#define MSGTR_MissingVideoStreamBug "Missing video stream!? Contact the author, it may be a bug :(\n"
#endif

#ifndef MSGTR_DoesntContainSelectedStream
#define MSGTR_DoesntContainSelectedStream "demux: file doesn't contain the selected audio or video stream\n"
#endif

#ifndef MSGTR_NI_Forced
#define MSGTR_NI_Forced "Forced"
#endif
#ifndef MSGTR_NI_Detected
#define MSGTR_NI_Detected "Detected"
#endif
#ifndef MSGTR_NI_Message
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI file-format!\n"
#endif

#ifndef MSGTR_UsingNINI
#define MSGTR_UsingNINI "Using NON-INTERLEAVED Broken AVI file-format!\n"
#endif
#ifndef MSGTR_CouldntDetFNo
#define MSGTR_CouldntDetFNo "Couldn't determine number of frames (for SOF seek)  \n"
#endif
#ifndef MSGTR_CantSeekRawAVI
#define MSGTR_CantSeekRawAVI "Can't seek in raw .AVI streams! (index required, try with the -idx switch!)  \n"
#endif
#ifndef MSGTR_CantSeekFile
#define MSGTR_CantSeekFile "Can't seek in this file!  \n"
#endif

#ifndef MSGTR_MOVcomprhdr
#define MSGTR_MOVcomprhdr "MOV: Compressed headers not (yet) supported!\n"
#endif
#ifndef MSGTR_MOVvariableFourCC
#define MSGTR_MOVvariableFourCC "MOV: Warning! variable FOURCC detected!?\n"
#endif
#ifndef MSGTR_MOVtooManyTrk
#define MSGTR_MOVtooManyTrk "MOV: Warning! too many tracks!"
#endif
#ifndef MSGTR_MOVnotyetsupp
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV format not yet supported!!!!!!! *******\n"
#endif

// dec_video.c & dec_audio.c:
#ifndef MSGTR_CantOpenCodec
#define MSGTR_CantOpenCodec "could not open codec\n"
#endif
#ifndef MSGTR_CantCloseCodec
#define MSGTR_CantCloseCodec "could not close codec\n"
#endif

#ifndef MSGTR_MissingDLLcodec
#define MSGTR_MissingDLLcodec "ERROR: Couldn't open required DirectShow codec: %s\n"
#endif
#ifndef MSGTR_ACMiniterror
#define MSGTR_ACMiniterror "Could not load/initialize Win32/ACM AUDIO codec (missing DLL file?)\n"
#endif
#ifndef MSGTR_MissingLAVCcodec
#define MSGTR_MissingLAVCcodec "Can't find codec '%s' in libavcodec...\n"
#endif

#ifndef MSGTR_NoDShowSupport
#define MSGTR_NoDShowSupport "MPlayerXP was compiled WITHOUT directshow support!\n"
#endif
#ifndef MSGTR_NoWfvSupport
#define MSGTR_NoWfvSupport "Support for win32 codecs disabled, or unavailable on non-x86 platforms!\n"
#endif
#ifndef MSGTR_NoDivx4Support
#define MSGTR_NoDivx4Support "MPlayerXP was compiled WITHOUT DivX4Linux (libdivxdecore.so) support!\n"
#endif
#ifndef MSGTR_NoLAVCsupport
#define MSGTR_NoLAVCsupport "MPlayerXP was compiled WITHOUT lavc/libavcodec support!\n"
#endif
#ifndef MSGTR_NoACMSupport
#define MSGTR_NoACMSupport "Win32/ACM audio codec disabled, or unavailable on non-x86 CPU -> force nosound :(\n"
#endif
#ifndef MSGTR_NoDShowAudio
#define MSGTR_NoDShowAudio "Compiled without DirectShow support -> force nosound :(\n"
#endif
#ifndef MSGTR_NoOggVorbis
#define MSGTR_NoOggVorbis "OggVorbis audio codec disabled -> force nosound :(\n"
#endif
#ifndef MSGTR_NoXAnimSupport
#define MSGTR_NoXAnimSupport "MPlayerXP was compiled WITHOUT XAnim support!\n"
#endif

#ifndef MSGTR_MpegNoSequHdr
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF while searching for sequence header\n"
#endif
#ifndef MSGTR_CannotReadMpegSequHdr
#define MSGTR_CannotReadMpegSequHdr "FATAL: Cannot read sequence header!\n"
#endif
#ifndef MSGTR_CannotReadMpegSequHdrEx
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Cannot read sequence header extension!\n"
#endif
#ifndef MSGTR_BadMpegSequHdr
#define MSGTR_BadMpegSequHdr "MPEG: Bad sequence header!\n"
#endif
#ifndef MSGTR_BadMpegSequHdrEx
#define MSGTR_BadMpegSequHdrEx "MPEG: Bad sequence header extension!\n"
#endif

#ifndef MSGTR_ShMemAllocFail
#define MSGTR_ShMemAllocFail "Cannot allocate shared memory"
#endif
#ifndef MSGTR_OutOfMemory
#define MSGTR_OutOfMemory "Out of memory"
#endif
#ifndef MSGTR_CantAllocAudioBuf
#define MSGTR_CantAllocAudioBuf "Cannot allocate audio out buffer\n"
#endif
#ifndef MSGTR_NoMemForDecodedImage
#define MSGTR_NoMemForDecodedImage "not enough memory for decoded picture buffer (%ld bytes)\n"
#endif

#ifndef MSGTR_AC3notvalid
#define MSGTR_AC3notvalid "AC3 stream not valid.\n"
#endif
#ifndef MSGTR_AC3only48k
#define MSGTR_AC3only48k "Only 48000 Hz streams supported.\n"
#endif
#ifndef MSGTR_UnknownAudio
#define MSGTR_UnknownAudio "Unknown/missing audio format, using nosound\n"
#endif

// LIRC:
#ifndef MSGTR_SettingUpLIRC
#define MSGTR_SettingUpLIRC "Setting up lirc support..."
#endif
#ifndef MSGTR_LIRCdisabled
#define MSGTR_LIRCdisabled "You won't be able to use your remote control"
#endif
#ifndef MSGTR_LIRCopenfailed
#define MSGTR_LIRCopenfailed "Failed opening lirc support!"
#endif
#ifndef MSGTR_LIRCsocketerr
#define MSGTR_LIRCsocketerr "Something's wrong with the lirc socket"
#endif
#ifndef MSGTR_LIRCcfgerr
#define MSGTR_LIRCcfgerr "Failed to read LIRC config file"
#endif

#endif /*__MPXP_EN_HELP_INCLUDED*/
