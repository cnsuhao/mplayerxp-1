// Transated by:  Andreas Berntsen  <andreasb@kvarteret.org>
// Updated for 0.60 by: B. Johannessen <bob@well.com>
// UTF-8
#ifdef HELP_MP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Bruk:    mplayerxp [valg] [sti/]filnavn\n"
"\n"
"Valg:\n"
" -vo <drv[:dev]> velg video-ut driver og enhet (se '-vo help' for liste)\n"
" -ao <drv[:dev]> velg lyd-ut driver og enhet (se '-ao help' for liste)\n"
" vcd://<sporno>   spill VCD (video cd) spor fra enhet i stedet for fil\n"
#ifdef CONFIG_DVDREAD
" dvd://<tittelno> spill DVD tittel/spor fra enhet i stedet for fil\n"
#endif
" -ss <timepos>   søk til gitt (sekunder eller hh:mm:ss) posisjon\n"
" -nosound        ikke spill av lyd\n"
" -channels <n>   målnummer for lyd output kanaler\n"
" -fs -vm -zoom   fullskjerm avspillings valg (fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   skaler bildet til <x> * <y> oppløsning [hvis -vo driver støtter det!]\n"
" -sub <fil>      spesifiser hvilken subtitle fil som skal brukes (se også -subfps, -subdelay)\n"
" -vid x -aid y   spesifiser hvilken video (x) og lyd (y) stream som skal spilles av\n"
" -fps x -srate y spesifiser video (x fps) og lyd (y Hz) hastiget\n"
" -pp <quality>   slå på etterbehandlingsfilter (0-4 for DivX, 0-63 for mpeg)\n"
" -nobps          bruk alternativ A-V sync metode for AVI filer (kan være nyttig!)\n"
" -framedrop      slå på bilde-dropping (for trege maskiner)\n"
" -wid <window id> bruk eksisterende vindu for video output (nytting med plugger!)\n"
"\n"
"Tastatur:\n"
" <- eller ->       søk bakover/fremover 10 sekunder\n"
" opp eller ned     søk bakover/fremover 1 minutt\n"
" < or >            søk bakover/fremover i playlisten\n"
" p eller MELLOMROM pause filmen (trykk en tast for å fortsette)\n"
" q eller ESC       stopp avspilling og avslutt programmet\n"
" + eller -         juster lyd-forsinkelse med +/- 0.1 sekund\n"
" o                 gå gjennom OSD modi:  ingen / søkelinje / søkelinje+tidsvisning\n"
" * eller /         øk eller mink volumet (trykk 'm' for å velge master/pcm)\n"
" z or x            juster undertittelens forsinkelse med +/- 0.1 sekund\n"
"\n"
" * * * SE PÅ MANSIDE FOR DETALJER, FLERE (AVANSERTE) VALG OG TASTER! * * *\n"
"\n";
#endif
#endif
