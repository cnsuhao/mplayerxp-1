// Translated by:  Carl Fürstenberg <azatoth AT gmail DOT com>
// Helped by: Jan Knutar <jknutar AT nic DOT fi>
// UTF-8
#ifdef HELP_MPXP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Användning:   mplayerxp [argument] [url|sökväg/]filnamn\n"
"\n"
"Grundläggande argument: (komplett lista återfinns i `man mplayerxp`)\n"
" -vo <drv[:enhet]>   välj video-ut drivrutin & enhet ('-vo help' för lista)\n"
" -ao <drv[:enhet]>   välj audio-ut drivrutin & enhet ('-ao help' för lista)\n"
" -play.ss <tidpos>   sök till given position (sekunder eller hh:mm:ss)\n"
" -audio.off          spela inte upp ljud\n"
" -video.fs           fullskärmsuppspelning (eller -video.vm, -video.zoom, detaljer på manualsidan)\n"
" -sub.file <fil>     specifiera textningsfil att använda\n"
" -play.list <fil>    specifiera spellistefil\n"
" -sync.framedrop     aktivera reducering av antalet bildrutor (för långsamma maskiner)\n"
"\n"
"Grundläggande navigering: (komplett lista återfinns på manualsidan, läs även input.conf)\n"
" <-  eller  ->       sök bakåt/framåt 10 sekunder\n"
" upp eller ner       sök bakåt/framåt 1 minut\n"
" pgup eller pgdown   sök bakåt/framåt 10 minuter\n"
" < eller >           stega bakåt/framåt i spellistan\n"
" p eller SPACE       pausa filmen (tryck på valfri tagent för att fortsätta)\n"
" q eller ESC         stanna spelningen och avsluta programmet\n"
" o                   växla OSD läge:  ingen / lägesindikator / lägesindikator + tidtagare\n"
" * eller /           öka eller sänk PCM-volym\n"
"\n"
" * * * LÄS MANUALEN FÖR FLER DETALJER, MER AVANCERADE ARGUMENT OCH KOMMANDON * * *\n"
"\n";
#endif
#endif
