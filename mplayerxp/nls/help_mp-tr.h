// MPlayerXP Turkish Translation
// Synced with help_mp-en.h r26067
// Translated by: Tuncer Altay, tunceraltay (at) yahoo.com and Kadir T. İri, centurium (at) gmx.net
// Additions by Mehmet KÖSE <mehmetkse@gmail.com> 
// ~/Turkey/Ankara/Ankara University/Computer Engineering Department
// UTF-8
#ifdef HELP_MP_DEFINE_STATIC
#ifndef MSGTR_BANNER_TEXT
static char* banner_text=
"\n\n"
"MPlayerXP " VERSION "(C) 2002 Nickols_K 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Kullanım: mplayerxp [seçenekler] [adres|yol/]dosya adı\n"
"\n"
"Genel seçenekler: (Tüm seçenekler için man sayfalarına bakınız)\n"
" -vo <sürücü>       video çıkış sürücüsünü seçer ('-vo help' ile listeyi görebilirsiniz)\n"
" -ao <sürücü>       ses çıkış sürücüsü seçer ('-ao help' ile listeyi görebilirsiniz)\n"
" -play.ss <zamankon> Verilen konumu arar (saniye veya hh:mm:ss ;saat:dakika:saniye olarak)\n"
" -audio.off         Sesi çalmaz\n"
" -video.fs          Tam ekran çalıştırma seçenekleri (veya -video.vm, -video.zoom, detaylar man sayfalarında)\n"
" -sub.file <dosya>  Kullanılacak altyazı dosyasını seçer\n"
" -play.list <dosya> Çalma listesi dosyasını seçer\n"
" -sync.framedrop    kare(frame) atlamayı etkinleştirir (yavaş bilgisayarlar için)\n"
"\n"
"Başlıca Tuşlar: (tüm liste man sayfasındadır, ayrıca input.conf  dosyasını kontrol ediniz)\n"
" <-  veya  ->       geri sar/ileri sar (10 saniye )\n"
" yukarı veya aşağı  geri sar/ileri sar  (1 dakika)\n"
" pgup veya pgdown   geri sar/ileri sar (10 dakika)\n"
" < veya >           çalma listesinde önceki/sonraki \n"
" p veya BOŞLUK      duraklat (devam etmek için herhangi bir tuşa basınız)\n"
" q veya ESC         durdur ve uygulamadan çık\n"
" + veya -           ses gecikmesini +/- 0.1 saniye olarak ayarla\n"
" o                  OSD modunu değiştir:  yok / oynatma imi / oynatma imi + zamanlayıcı\n"
" * veya /           sesi yükselt veya alçalt\n"
" z veya x           altyazı gecikmesini +/- 0.1 saniye olarak ayarla\n"
" r veya t           altyazı konumunu yukarı/aşağı ayarla, -vf seçeneğine de bakınız\n"
"\n"
" * * AYRINTILAR, DAHA FAZLA (GELİŞMİŞ) SEÇENEKLER VE TUŞLAR İÇİN MAN SAYFALARINA BAKINIZ * *\n"
"\n";
#endif
#endif
