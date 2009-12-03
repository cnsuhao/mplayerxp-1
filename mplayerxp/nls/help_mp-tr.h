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
" -vo <sürücü>          video çıkış sürücüsünü seçer ('-vo help' ile listeyi görebilirsiniz)\n"
" -ao <sürücü>          ses çıkış sürücüsü seçer ('-ao help' ile listeyi görebilirsiniz)\n"
#ifdef CONFIG_VCD
" vcd://<parçano>    (S)VCD (Süper Video CD) parça numarasını oynatır (sade aygıtı kullan, sisteme takma)\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<başlıkno>   Dosya yerine aygıttan DVD başlığını oynatır.\n"
" -alang/-slang      DVD ses/altyazı dili seçer (2 karakterli ülke kodu ile)\n"
#endif
" -ss <zamankon>     Verilen konumu arar (saniye veya hh:mm:ss ;saat:dakika:saniye olarak)\n"
" -nosound           Sesi çalmaz\n"
" -fs -vm -zoom      Tam ekran çalıştırma seçenekleri (veya -vm, -zoom, detaylar man sayfalarında)\n"
" -x <x> -y <y>      Ekran çözünürlüğünü ayarlar (-vm veya -zoom kullanımı için)\n"
" -sub <dosya>       Kullanılacak altyazı dosyasını seçer ( ayrıca -subfps, -subdelay seçeneklerine bakınız)\n"
" -playlist <dosya>  Çalma listesi dosyasını seçer\n"
" -vid x -aid y      Oynatılacak video (x) ve çalınacak ses (y) yayınını(stream) seçer\n"
" -fps x -srate y    Video (x) biçimini fps olarak ve ses (y) biçimini Hz olarak değiştirir\n"
" -pp <kalite>       postprocessing filtresini etkinleştirir (ayrıntılar için man sayfalarına bakınız)\n"
" -framedrop         kare(frame) atlamayı etkinleştirir (yavaş bilgisayarlar için)\n"
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
