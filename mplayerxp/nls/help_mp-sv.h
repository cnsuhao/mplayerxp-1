// Translated by:  Carl Fürstenberg <azatoth AT gmail DOT com>
// Helped by: Jan Knutar <jknutar AT nic DOT fi>
// UTF-8
#ifdef HELP_MP_DEFINE_STATIC
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
#ifdef CONFIG_VCD
" vcd://<spårnr>      spela (S)VCD (Super Video CD) spår (rå enhet, ingen montering)\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<titlenr>     spela DVD titel från enhet istället för ifrån en enkel fil\n"
" -alang/-slang       välj DVD audio/textningsspråk (m.h.a. ett 2-teckens landskod)\n"
#endif
" -ss <tidpos>        sök till given position (sekunder eller hh:mm:ss)\n"
" -nosound            spela inte upp ljud\n"
" -fs                 fullskärmsuppspelning (eller -vm, -zoom, detaljer på manualsidan)\n"
" -x <x> -y <y>       sätt skärmupplösning (för användning med -vm eller -zoom)\n"
" -sub <fil>          specifiera textningsfil att använda (se också -subfps, -subdelay)\n"
" -playlist <fil>     specifiera spellistefil\n"
" -vid x -aid y       välj video (x) och audio (y) ström att spela\n"
" -fps x -srate y     ändra video (x fps) och audio (y Hz) frekvens\n"
" -pp <kvalité>       aktivera postredigeringsfilter (detaljer på manualsidan)\n"
" -framedrop          aktivera reducering av antalet bildrutor (för långsamma maskiner)\n" 
"\n"
"Grundläggande navigering: (komplett lista återfinns på manualsidan, läs även input.conf)\n"
" <-  eller  ->       sök bakåt/framåt 10 sekunder\n"
" upp eller ner       sök bakåt/framåt 1 minut\n"
" pgup eller pgdown   sök bakåt/framåt 10 minuter\n"
" < eller >           stega bakåt/framåt i spellistan\n"
" p eller SPACE       pausa filmen (tryck på valfri tagent för att fortsätta)\n"
" q eller ESC         stanna spelningen och avsluta programmet\n"
" + eller -           ställ in audiofördröjning med ± 0.1 sekund\n"
" o                   växla OSD läge:  ingen / lägesindikator / lägesindikator + tidtagare\n"
" * eller /           öka eller sänk PCM-volym\n"
" z eller x           ställ in textningsfördröjning med ± 0.1 sekund\n"
" r or t              ställ in textningsposition upp/ner, se också '-vf expand'\n"
"\n"
" * * * LÄS MANUALEN FÖR FLER DETALJER, MER AVANCERADE ARGUMENT OCH KOMMANDON * * *\n"
"\n";
#endif
#endif
