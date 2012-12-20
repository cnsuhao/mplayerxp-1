// Original transation by Firebird <firebird@chez.com>
// Maintained by pl <p_l@tfz.net>
// UTF-8
// ========================= Aide MPlayer ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Utilisation:   mplayerxp [options] [répertoire/]fichier",
"",
"Options:",
" -vo <pil[:pér]>  Sélectionne le pilote et le périphérique de sortie vidéo",
"                  ('-vo help' pour la liste)",
" -ao <pil[:pér]>  Sélectionne le pilote et le périphérique de sortie audio",
"                  ('-ao help' pour la liste)",
" -play.ss <postemp> Démarre la lecture à partir de la pos. (secondes ou hh:mm:ss)",
" -audio.off       Ne jouer aucun son",
" -video.fs        Options plein-écran (fs: plein-écran, vm: changement de mode",
"                  vidéo, zoom: changement de taille (logiciel)",
" -sub.file <fichier>Utilise les sous-titres dans 'fichier'",
" -play.list<fich.>Spécifie la liste des fichiers à jouer",
" -sync.framedrop  Active le drop d'images (pour ordinateurs lents)",
"",
"Touches:",
" <- ou ->         Saute en avant/arrière de 10 secondes",
" haut ou bas      Saute en avant/arrière de 1 minute",
" < ou >           Saute en avant/arrière dans la playlist",
" p ou ESPACE      Pause (presser n'importe quelle touche pour continuer)",
" q ou ESC         Arrête la lecture et quitter le programme",
" o                Mode OSD:  aucun / cherchable / cherchable+temps",
" * ou /           Augmente/diminue volume ('m' pour sélectionner maître/pcm)",
"",
" * * * IL Y A D'AUTRES TOUCHES ET OPTIONS DANS LA PAGE MAN ! * * *",
NULL
};
#endif

// ========================= Messages MPlayer ===========================

// mplayer.c:

#define MSGTR_Exiting "Sortie"
#define MSGTR_Exit_frames "Nombre demandé de frames jouées"
#define MSGTR_Exit_quit "Fin"
#define MSGTR_Exit_eof "Fin du fichier"
#define MSGTR_Fatal_error "Erreur fatale"
#define MSGTR_NoHomeDir "Ne peut trouver répertoire home"
#define MSGTR_Playing "Joue"
