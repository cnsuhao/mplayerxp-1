// Original transation by Firebird <firebird@chez.com>
// Maintained by pl <p_l@tfz.net>
// UTF-8
// ========================= Aide MPlayer ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
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

#define MSGTR_Exiting "Sortie"
#define MSGTR_Exit_frames "Nombre demandé de frames jouées"
#define MSGTR_Exit_quit "Fin"
#define MSGTR_Exit_eof "Fin du fichier"
#define MSGTR_Fatal_error "Erreur fatale"
#define MSGTR_NoHomeDir "Ne peut trouver répertoire home"
#define MSGTR_Playing "Joue"
