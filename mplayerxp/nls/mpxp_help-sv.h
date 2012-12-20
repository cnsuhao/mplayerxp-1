// Translated by:  Carl Fürstenberg <azatoth AT gmail DOT com>
// Helped by: Jan Knutar <jknutar AT nic DOT fi>
// UTF-8
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
"Användning:   mplayerxp [argument] [url|sökväg/]filnamn",
"",
"Grundläggande argument: (komplett lista återfinns i `man mplayerxp`)",
" -vo <drv[:enhet]>   välj video-ut drivrutin & enhet ('-vo help' för lista)",
" -ao <drv[:enhet]>   välj audio-ut drivrutin & enhet ('-ao help' för lista)",
" -play.ss <tidpos>   sök till given position (sekunder eller hh:mm:ss)",
" -audio.off          spela inte upp ljud",
" -video.fs           fullskärmsuppspelning (eller -video.vm, -video.zoom, detaljer på manualsidan)",
" -sub.file <fil>     specifiera textningsfil att använda",
" -play.list <fil>    specifiera spellistefil",
" -sync.framedrop     aktivera reducering av antalet bildrutor (för långsamma maskiner)",
"",
"Grundläggande navigering: (komplett lista återfinns på manualsidan, läs även input.conf)",
" <-  eller  ->       sök bakåt/framåt 10 sekunder",
" upp eller ner       sök bakåt/framåt 1 minut",
" pgup eller pgdown   sök bakåt/framåt 10 minuter",
" < eller >           stega bakåt/framåt i spellistan",
" p eller SPACE       pausa filmen (tryck på valfri tagent för att fortsätta)",
" q eller ESC         stanna spelningen och avsluta programmet",
" o                   växla OSD läge:  ingen / lägesindikator / lägesindikator + tidtagare",
" * eller /           öka eller sänk PCM-volym",
"",
" * * * LÄS MANUALEN FÖR FLER DETALJER, MER AVANCERADE ARGUMENT OCH KOMMANDON * * *",
NULL
};
#endif
#endif
