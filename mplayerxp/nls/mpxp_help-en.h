#ifndef __MPXP_EN_HELP_INCLUDED
#define __MPXP_EN_HELP_INCLUDED 1
/* MASTER FILE. Use this file as base for translation!

 Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
 and send a notify message to mplayer-dev-eng maillist.

 ========================= MPlayerXP help =========================== */

#ifdef HELP_MPXP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static const char* banner_text=
"\n\n"
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static const char help_text[]=
"Usage:   mplayerxp [options] [path/]filename\n"
"\n"
"Options:\n"
" -vo <drv[:dev]> select video output driver & device (see '-vo help' for list)\n"
" -ao <drv[:dev]> select audio output driver & device (see '-ao help' for list)\n"
" -play.ss <timepos> seek to given (seconds or hh:mm:ss) position\n"
" -audio.off      don't play sound\n"
" -video.fs       fullscreen playing options (fullscr,vidmode chg,softw.scale)\n"
" -sub.file <file> specify subtitle file to use\n"
" -play.list<file> specify playlist file\n"
" -sync.framedrop  enable frame-dropping (for slow machines)\n"
"\n"
"Keys:\n"
" <-  or  ->      seek backward/forward 10 seconds\n"
" up or down      seek backward/forward  1 minute\n"
" < or >          seek backward/forward in playlist\n"
" p or SPACE      pause movie (press any key to continue)\n"
" q or ESC        stop playing and quit program\n"
" o               cycle OSD mode:  none / seekbar / seekbar+timer\n"
" * or /          increase or decrease volume (press 'm' to select master/pcm)\n"
"\n"
" * * * SEE MANPAGE FOR DETAILS, FURTHER (ADVANCED) OPTIONS AND KEYS ! * * *\n"
"\n";
#endif
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#ifndef MSGTR_SystemTooSlow
#define MSGTR_SystemTooSlow "\n\n"\
"         ************************************************\n"\
"         **** Your system is too SLOW to play this!  ****\n"\
"         ************************************************\n"\
"!!! Possible reasons, problems, workaround:\n"\
"- Most common: broken/buggy AUDIO driver. Workaround: -ao sdl or -ao alsa\n"\
"- Slow VIDEO card. Try different -vo driver (for list: -vo help)\n"\
"   -vo (*)vidix is recommended (if available) or try with -framedrop !\n"\
"- Slow cpu. Don't try to playback big dvd/divx on slow cpu! try -hardframedrop\n"\
"- Broken file. Try various combinations of these: -nobps  -ni  -mc 0  -forceidx\n"\
"*******************************************************************************\n"\
" Also try study these keys:\n"\
" -video.bm (currently may work with some vidix drivers only) [speedup upto 30%]\n"\
" '/bin/sh hdparm -u1 -d1 -a8 /dev/hdX' [25%]\n"\
" Try disable OSD by pressing 'O' key twice [5-15%]\n"\
"Also try to decrease the number of buffers for decoding ahead: '-core.da_buffs'\n"\
"*******************************************************************************\n"\
"On SMP -lavc.slices=0 may help too\n"
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
#ifndef MSGTR_Exit_error
#define MSGTR_Exit_error "Fatal error"
#endif
#ifndef MSGTR_IntBySignal
#define MSGTR_IntBySignal "\nMPlayerXP interrupted by signal %d in module: %s \n"
#endif
#ifndef MSGTR_NoHomeDir
#define MSGTR_NoHomeDir "Can't find HOME dir\n"
#endif
#ifndef MSGTR_GetpathProblem
#define MSGTR_GetpathProblem "get_path(\"config\") problem\n"
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
#ifndef MSGTR_CoreDumped
#define MSGTR_CoreDumped "core dumped :)\n"
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
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Try to upgrade %s from etc/codecs.conf\n*** If it's still not OK, then read DOCS/codecs.html!\n"
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
#ifndef MSGTR_StartPlaying
#define MSGTR_StartPlaying "Start playing...\n"
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
#define MSGTR_CODEC_CANT_LOAD_DLL "codec_ld: Can't load library: '%s' due: %s\n"
#endif
#ifndef MSGTR_CODEC_DLL_HINT
#define MSGTR_CODEC_DLL_HINT "codec_ld: Try obtain this codec at: %s\n"
#endif
#ifndef MSGTR_CODEC_DLL_OK
#define MSGTR_CODEC_DLL_OK "codec_ld: The library: '%s' was loaded OK\n"
#endif
#ifndef MSGTR_CODEC_DLL_SYM_ERR
#define MSGTR_CODEC_DLL_SYM_ERR "Can't resolve symbol: %s\n"
#endif
//#define MSGTR_NoGui "MPlayer was compiled WITHOUT GUI support!\n"
//#define MSGTR_GuiNeedsX "MPlayer GUI requires X11!\n"
#ifndef MSGTR_Playing
#define MSGTR_Playing "Playing"
#endif
#ifndef MSGTR_NoSound
#define MSGTR_NoSound "Audio: no sound!!!\n"
#endif
#ifndef MSGTR_FPSforced
#define MSGTR_FPSforced "FPS forced to be %5.3f  (ftime: %5.3f)\n"
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
#define MSGTR_UnableOpenURL "Unable to open URL: %s\n"
#endif
#ifndef MSGTR_ConnToServer
#define MSGTR_ConnToServer "Connected to server: %s\n"
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
#define MSGTR_ConnAuthFailed "Authentication failed\n"\
			    "Please use the option -user and -passwd to provide your username/password for a list of URLs,\n"\
			    "or form an URL like: http://username:password@hostname/file\n"
#endif
#ifndef MSGTR_FileNotFound
#define MSGTR_FileNotFound "File not found: '%s'\n"
#endif

#ifndef MSGTR_CantOpenDVD
#define MSGTR_CantOpenDVD "Couldn't open DVD device: %s\n"
#endif
#ifndef MSGTR_DVDwait
#define MSGTR_DVDwait "Reading disc structure, please wait...\n"
#endif
#ifndef MSGTR_DVDnumTitles
#define MSGTR_DVDnumTitles "There are %d titles on this DVD.\n"
#endif
#ifndef MSGTR_DVDinvalidTitle
#define MSGTR_DVDinvalidTitle "Invalid DVD title number: %d\n"
#endif
#ifndef MSGTR_DVDnumChapters
#define MSGTR_DVDnumChapters "Title #%i has %d chapters.\n"
#endif
#ifndef MSGTR_DVDinvalidChapter
#define MSGTR_DVDinvalidChapter "Invalid DVD chapter number: %d\n"
#endif
#ifndef MSGTR_DVDnumAngles
#define MSGTR_DVDnumAngles "There are %d angles in this DVD title.\n"
#endif
#ifndef MSGTR_DVDinvalidAngle
#define MSGTR_DVDinvalidAngle "Invalid DVD angle number: %d\n"
#endif
#ifndef MSGTR_DVDnoIFO
#define MSGTR_DVDnoIFO "Can't open the IFO file for DVD title %d.\n"
#endif
#ifndef MSGTR_DVDnoVOBs
#define MSGTR_DVDnoVOBs "Can't open title VOBS (VTS_%02d_1.VOB).\n"
#endif
#ifndef MSGTR_DVDopenOk
#define MSGTR_DVDopenOk "DVD successfully opened!\n"
#endif

// demuxer.c, demux_*.c:
#ifndef MSGTR_AudioStreamRedefined
#define MSGTR_AudioStreamRedefined "Warning! Audio stream header %d redefined!\n"
#endif
#ifndef MSGTR_VideoStreamRedefined
#define MSGTR_VideoStreamRedefined "Warning! video stream header %d redefined!\n"
#endif
#ifndef MSGTR_SubStreamRedefined
#define MSGTR_SubStreamRedefined "Warning! subtitle stream id: %d redefined!\n"
#endif
#ifndef MSGTR_TooManyAudioInBuffer
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Too many (%d in %d bytes) audio packets in the buffer!\n"
#endif
#ifndef MSGTR_TooManyVideoInBuffer
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Too many (%d in %d bytes) video packets in the buffer!\n"
#endif
#ifndef MSGTR_MaybeNI
#define MSGTR_MaybeNI "(maybe you play a non-interleaved stream/file or the codec failed)\n"
#endif
#ifndef MSGTR_DetectedFILMfile
#define MSGTR_DetectedFILMfile "Detected FILM file format!\n"
#endif
#ifndef MSGTR_DetectedFLIfile
#define MSGTR_DetectedFLIfile "Detected FLI file format!\n"
#endif
#ifndef MSGTR_DetectedROQfile
#define MSGTR_DetectedROQfile "Detected RoQ file format!\n"
#endif
#ifndef MSGTR_DetectedREALfile
#define MSGTR_DetectedREALfile "Detected REAL file format!\n"
#endif
#ifndef MSGTR_DetectedAVIfile
#define MSGTR_DetectedAVIfile "Detected AVI file format!\n"
#endif
#ifndef MSGTR_DetectedASFfile
#define MSGTR_DetectedASFfile "Detected ASF file format!\n"
#endif
#ifndef MSGTR_DetectedMPEGPESfile
#define MSGTR_DetectedMPEGPESfile "Detected MPEG-PES file format!\n"
#endif
#ifndef MSGTR_DetectedMPEGPSfile
#define MSGTR_DetectedMPEGPSfile "Detected MPEG-PS file format!\n"
#endif
#ifndef MSGTR_DetectedMPEGESfile
#define MSGTR_DetectedMPEGESfile "Detected MPEG-ES file format!\n"
#endif
#ifndef MSGTR_DetectedQTMOVfile
#define MSGTR_DetectedQTMOVfile "Detected QuickTime/MOV file format!\n"
#endif
#ifndef MSGTR_MissingMpegVideo
#define MSGTR_MissingMpegVideo "Missing MPEG video stream!? contact the author, it may be a bug :(\n"
#endif
#ifndef MSGTR_InvalidMPEGES
#define MSGTR_InvalidMPEGES "Invalid MPEG-ES stream??? contact the author, it may be a bug :(\n"
#endif
#ifndef MSGTR_FormatNotRecognized
#define MSGTR_FormatNotRecognized "============= Sorry, this file format not recognized/supported ===============\n"\
				  "=== If this file is an AVI, ASF or MPEG stream, please contact the author! ===\n"
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
#define MSGTR_ShMemAllocFail "Cannot allocate shared memory\n"
#endif
#ifndef MSGTR_OutOfMemory
#define MSGTR_OutOfMemory "Out of memory\n"
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
#define MSGTR_SettingUpLIRC "Setting up lirc support...\n"
#endif
#ifndef MSGTR_LIRCdisabled
#define MSGTR_LIRCdisabled "You won't be able to use your remote control\n"
#endif
#ifndef MSGTR_LIRCopenfailed
#define MSGTR_LIRCopenfailed "Failed opening lirc support!\n"
#endif
#ifndef MSGTR_LIRCsocketerr
#define MSGTR_LIRCsocketerr "Something's wrong with the lirc socket: %s\n"
#endif
#ifndef MSGTR_LIRCcfgerr
#define MSGTR_LIRCcfgerr "Failed to read LIRC config file %s !\n"
#endif

#endif /*__MPXP_EN_HELP_INCLUDED*/
