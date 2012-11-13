// Original transation by Firebird <firebird@chez.com>
// Maintained by pl <p_l@tfz.net>
// UTF-8
// ========================= Aide MPlayer ===========================

#ifdef HELP_MP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Utilisation:   mplayerxp [options] [répertoire/]fichier\n"
"\n"
"Options:\n"
" -vo <pil[:pér]>  Sélectionne le pilote et le périphérique de sortie vidéo\n"
"                  ('-vo help' pour la liste)\n"
" -ao <pil[:pér]>  Sélectionne le pilote et le périphérique de sortie audio\n"
"                  ('-ao help' pour la liste)\n"
" -play.ss <postemp> Démarre la lecture à partir de la pos. (secondes ou hh:mm:ss)\n"
" -audio.off       Ne jouer aucun son\n"
" -video.fs        Options plein-écran (fs: plein-écran, vm: changement de mode\n"
"                  vidéo, zoom: changement de taille (logiciel)\n"
" -sub.file <fichier>Utilise les sous-titres dans 'fichier'\n"
" -play.list<fich.>Spécifie la liste des fichiers à jouer\n"
" -sync.framedrop  Active le drop d'images (pour ordinateurs lents)\n"
"\n"
"Touches:\n"
" <- ou ->         Saute en avant/arrière de 10 secondes\n"
" haut ou bas      Saute en avant/arrière de 1 minute\n"
" < ou >           Saute en avant/arrière dans la playlist\n"
" p ou ESPACE      Pause (presser n'importe quelle touche pour continuer)\n"
" q ou ESC         Arrête la lecture et quitter le programme\n"
" o                Mode OSD:  aucun / cherchable / cherchable+temps\n"
" * ou /           Augmente/diminue volume ('m' pour sélectionner maître/pcm)\n"
"\n"
" * * * IL Y A D'AUTRES TOUCHES ET OPTIONS DANS LA PAGE MAN ! * * *\n"
"\n";
#endif

// ========================= Messages MPlayer ===========================

// mplayer.c:

#define MSGTR_Exiting "\nSortie... (%s)\n"
#define MSGTR_Exit_frames "Nombre demandé de frames jouées"
#define MSGTR_Exit_quit "Fin"
#define MSGTR_Exit_eof "Fin du fichier"
#define MSGTR_Exit_error "Erreur fatale"
#define MSGTR_IntBySignal "\nMPlayerXP interrompu par le signal %d dans le module: %s \n"
#define MSGTR_NoHomeDir "Ne peut trouver répertoire home\n"
#define MSGTR_GetpathProblem "Problème get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Création du fichier de config: %s\n"
#define MSGTR_InvalidVOdriver "Nom du pilote de sortie vidéo invalide: %s\nUtiliser '-vo help' pour avoir une liste des pilotes disponibles.\n"
#define MSGTR_InvalidAOdriver "Nom du pilote de sortie audio invalide: %s\nUtiliser '-ao help' pour avoir une liste des pilotes disponibles.\n"
#define MSGTR_CopyCodecsConf "(Copiez/liez etc/codecs.conf (dans le source de MPlayerXP) vers ~/.mplayerxp/codecs.conf)\n"
#define MSGTR_CantLoadFont "Ne peut charger la police: %s\n"
#define MSGTR_CantLoadSub "Ne peut charger les sous-titres: %s\n"
#define MSGTR_ErrorDVDkey "Erreur avec la clé du DVD.\n"
#define MSGTR_CmdlineDVDkey "La clé DVD demandée sur la ligne de commande a été sauvegardée pour le décryptage.\n"
#define MSGTR_DVDauthOk "La séquence d'authentification DVD semble OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: le flux sélectionné est manquant\n"
#define MSGTR_CantOpenDumpfile "Ne peut ouvrir un fichier dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS non spécifié (ou invalide) dans l'entête! Utiliser l'option -fps!\n"
#define MSGTR_NoVideoStream "Désolé, aucun flux vidéo... c'est injouable\n"
#define MSGTR_TryForceAudioFmt "Tente de forcer famille de pilotes codec audio de famille '%s' ...\n"
#define MSGTR_CantFindAfmtFallback "Ne peut trouver de codec audio pour famille de pilotes choisie, utilise d'autres.\n"
#define MSGTR_CantFindAudioCodec "Ne peut trouver codec pour format audio"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Tentez de mettre à jour %s à partir de etc/codecs.conf\n*** Si ce n'est toujours pas bon, alors lisez DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Ne peut trouver de codec audio! -> Aucun son\n"
#define MSGTR_TryForceVideoFmt "Tente de forcer famille de pilotes codec vidéo '%s' ...\n"
#define MSGTR_CantFindVfmtFallback "Ne peut trouver de codec vidéo pour famille de pil. choisie, utilise d'autres.\n"
#define MSGTR_CantFindVideoCodec "Ne peut trouver codec pour format vidéo"
#define MSGTR_VOincompCodec "Désolé, le pilote de sortie vidéo choisi n'est pas compatible avec ce codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Ne peut initialiser le codec vidéo :(\n"
#define MSGTR_EncodeFileExists "fichier déjà existant: %s (N'effacez pas vos AVIs préférés!)\n"
#define MSGTR_CantCreateEncodeFile "Ne peut ouvrir fichier pour encodage\n"
#define MSGTR_CannotInitVO "FATAL: Ne peut initialiser le pilote vidéo!\n"
#define MSGTR_CannotInitAO "Ne peut ouvrir/initialiser le périphérique audio -> Aucun son\n"
#define MSGTR_StartPlaying "Démarre la reproduction...\n"

#define MSGTR_Playing "Joue %s\n"
#define MSGTR_NoSound "Audio: Aucun son!!!\n"
#define MSGTR_FPSforced "FPS fixé sur %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Lecteur CD-ROM '%s' non trouvé!\n"
#define MSGTR_ErrTrackSelect "Erreur lors du choix de la piste VCD!"
#define MSGTR_ReadSTDIN "Lecture depuis stdin...\n"
#define MSGTR_UnableOpenURL "Ne peut ouvrir l'URL: %s\n"
#define MSGTR_ConnToServer "Connecté au serveur: %s\n"
#define MSGTR_FileNotFound "Fichier non trouvé: '%s'\n"

#define MSGTR_CantOpenDVD "Ne peut ouvrir le lecteur DVD: %s\n"
#define MSGTR_DVDwait "Lit la structure du disque, attendre svp...\n"
#define MSGTR_DVDnumTitles "Il y a %d titres sur ce DVD.\n"
#define MSGTR_DVDinvalidTitle "Numero de titre DVD invalide: %d\n"
#define MSGTR_DVDinvalidChapter "Numéro de chapitre DVD invalide: %d\n"
#define MSGTR_DVDnumAngles "Il y a %d séquences sur ce titre DVD.\n"
#define MSGTR_DVDinvalidAngle "Numéro de séquence DVD invalide: %d\n"
#define MSGTR_DVDnoIFO "Ne peut ouvrir le fichier IFO pour le titre DVD %d.\n"
#define MSGTR_DVDnoVOBs "Ne peut ouvrir titre VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD ouvert avec succès!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Attention! Entête du flux audio %d redéfini!\n"
#define MSGTR_VideoStreamRedefined "Attention! Entête du flux vidéo %d redéfini!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Trop (%d dans %d octets) de packets audio dans le tampon!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Trop (%d dans %d octets) de packets vidéo dans le tampon!\n"
#define MSGTR_MaybeNI "(Peut-être jouez-vous un flux/fichier non-entrelacé, ou le codec manque...)\n"
#define MSGTR_DetectedFILMfile "Format de fichier FILE détecté!\n"
#define MSGTR_DetectedFLIfile "Format de fichier FLI détecté!\n"
#define MSGTR_DetectedROQfile "Format de fichier RoQ détecté!\n"
#define MSGTR_DetectedREALfile "Format de fichier REAL détecté!\n"
#define MSGTR_DetectedAVIfile "Format de fichier AVI détecté!\n"
#define MSGTR_DetectedASFfile "Format de fichier ASF détecté!\n"
#define MSGTR_DetectedMPEGPESfile "Format de fichier MPEG-PES détecté!\n"
#define MSGTR_DetectedMPEGPSfile "Format de fichier MPEG-PS détecté!\n"
#define MSGTR_DetectedMPEGESfile "Format de fichier MPEG-ES détecté!\n"
#define MSGTR_DetectedQTMOVfile "Format de fichier QuickTime/MOV détecté!\n"
#define MSGTR_MissingMpegVideo "Flux vidéo MPEG manquant!? Contactez l'auteur, ceci pourrait être un bug :(\n"
#define MSGTR_InvalidMPEGES "Flux MPEG-ES invalide??? Contactez l'auteur, ceci pourrait être un bug :(\n"
#define MSGTR_FormatNotRecognized "========== Désolé, ce format de fichier n'est pas reconnu/supporté ===========\n"\
				  "========= Si ce fichier est un flux  AVI, ASF ou MPEG Stream sain, ===========\n"\
				  "===================== alors veuillez contacter l'auteur ! ====================\n"
#define MSGTR_MissingVideoStream "Ne peut trouver de flux vidéo!\n"
#define MSGTR_MissingAudioStream "Ne peut trouver de flux audio...  -> pas de son\n"
#define MSGTR_MissingVideoStreamBug "Flux vidéo manquant!? Contactez l'auteur, ceci pourrait être un bug :(\n"

#define MSGTR_DoesntContainSelectedStream "Demux: le fichier ne contient pas le flux audio ou vidéo sélectionné\n"

#define MSGTR_NI_Forced "Forcé"
#define MSGTR_NI_Detected "Détecté"
#define MSGTR_NI_Message "%s format de fichier AVI NON-ENTRELACÉ!\n"

#define MSGTR_UsingNINI "Utilise fichier de format AVI NON-ENTRELACÉ défectueux!\n"
#define MSGTR_CouldntDetFNo "Ne peut déterminer le nombre de frames (pour recherche SOF)  \n"
#define MSGTR_CantSeekRawAVI "Ne peut chercher dans un flux .AVI brut! (index requis, essayez l'option -idx!)\n"
#define MSGTR_CantSeekFile "Ne peut chercher dans ce fichier!  \n"

#define MSGTR_MOVcomprhdr "MOV: Les entêtes compressées ne sont pas (encore) supportés!\n"
#define MSGTR_MOVvariableFourCC "MOV: Attention! Variable FOURCC détectée!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Attention! Trop de pistes!"
#define MSGTR_MOVnotyetsupp "\n******** Format Quicktime MOV pas encore supporté!!!!!!! *********\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Ne peut ouvrir le codec\n"
#define MSGTR_CantCloseCodec "Ne peut fermer le codec\n"

#define MSGTR_MissingDLLcodec "ERREUR: Ne peut trouver le codec DirectShow requis: %s\n"
#define MSGTR_ACMiniterror "Ne peut charger/initialiser le codec AUDIO Win32/ACM (fichier DLL manquant?)\n"
#define MSGTR_MissingLAVCcodec "Ne peut trouver le codec '%s' de libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayerXP a été compilé SANS support DirectShow!\n"
#define MSGTR_NoWfvSupport "Support des codecs Win32 désactivé, ou non disponible sur plateformes non-x86!\n"
#define MSGTR_NoDivx4Support "MPlayerXP a été compilé SANS le support DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayerXP a été compilé SANS le support ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Codecs audio Win32/ACM désactivés ou non disponibles sur plateformes non-x86 -> force -nosound :(\n"
#define MSGTR_NoDShowAudio "MPlayerXP a été compilé sans support DirectShow -> force -nosound :(\n"
#define MSGTR_NoOggVorbis "Codec audio OggVorbis désactivé -> force -nosound :(\n"
#define MSGTR_NoXAnimSupport "MPlayerXP a été compilé SANS support XAnim!\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: Fin du fichier lors de la recherche d'entête de séquence\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Ne peut lire l'entête de séquence!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Ne peut lire l'extension d'entête de séquence!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Mauvais entête de séquence!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Mauvaise extension d'entête de séquence!\n"

#define MSGTR_ShMemAllocFail "Ne peut allouer de mémoire partagée\n"
#define MSGTR_CantAllocAudioBuf "Ne peut allouer de tampon de sortie audio\n"
#define MSGTR_NoMemForDecodedImage "pas assez de mémoire pour le tampon d'image décodée (%ld octets)\n"

#define MSGTR_AC3notvalid "Flux AC3 non-valide.\n"
#define MSGTR_AC3only48k "Seuls les flux 48000 Hz sont supportés.\n"
#define MSGTR_UnknownAudio "Format audio inconnu/manquant -> pas de son\n"

// LIRC:
#define MSGTR_SettingUpLIRC "définition du support LIRC...\n"
#define MSGTR_LIRCdisabled "Vous ne pourrez pas utiliser votre télécommande\n"
#define MSGTR_LIRCopenfailed "Impossible d'ouvrir le support LIRC!\n"
#define MSGTR_LIRCsocketerr "Quelque chose est défectueux avec le socket LIRC: %s\n"
#define MSGTR_LIRCcfgerr "Impossible de lire le fichier de config LIRC %s !\n"
