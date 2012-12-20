// Transated by:  Andreas Berntsen  <andreasb@kvarteret.org>
// Updated for 0.60 by: B. Johannessen <bob@well.com>
// UTF-8
// ========================= MPlayer hjelp ===========================

#ifdef HELP_MPXP_DEFINE_STATIC
#define MSGTR_BANNER_TEXT 1
static const char* banner_text[]={
"",
"",
"MPlayerXP " VERSION "2002 Nickols_K 2000-2002 Arpad Gereoffy (se DOCS!)",
NULL
};

static const char* help_text[]={
"",
"Bruk:    mplayerxp [valg] [sti/]filnavn",
"",
"Valg:",
" -vo <drv[:dev]> velg video-ut driver og enhet (se '-vo help' for liste)",
" -ao <drv[:dev]> velg lyd-ut driver og enhet (se '-ao help' for liste)",
" -play.ss <timepos>søk til gitt (sekunder eller hh:mm:ss) posisjon",
" -audio.off      ikke spill av lyd",
" -video.fs       fullskjerm avspillings valg (fullscr,vidmode chg,softw.scale)",
" -sub.file <fil> spesifiser hvilken subtitle fil som skal brukes",
" -sync.framedrop slå på bilde-dropping (for trege maskiner)",
"",
"Tastatur:",
" <- eller ->       søk bakover/fremover 10 sekunder",
" opp eller ned     søk bakover/fremover 1 minutt",
" < or >            søk bakover/fremover i playlisten",
" p eller MELLOMROM pause filmen (trykk en tast for å fortsette)",
" q eller ESC       stopp avspilling og avslutt programmet",
" o                 gå gjennom OSD modi:  ingen / søkelinje / søkelinje+tidsvisning",
" * eller /         øk eller mink volumet (trykk 'm' for å velge master/pcm)",
"",
" * * * SE PÅ MANSIDE FOR DETALJER, FLERE (AVANSERTE) VALG OG TASTER! * * *",
NULL
};
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "Avslutter"
#define MSGTR_Exit_frames "Antall forespurte bilder vist"
#define MSGTR_Exit_quit "Avslutt"
#define MSGTR_Exit_eof "Slutt på filen"
#define MSGTR_Fatal_error "Fatal feil"
#define MSGTR_NoHomeDir "Kan ikke finne HOME katalog"
#define MSGTR_Playing "Spiller"
