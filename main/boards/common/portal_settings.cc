#include "portal_settings.h"

#include "ota.h"
#include "settings.h"

#include <nvs.h>

namespace {
using namespace wifi_config;

constexpr Translation kOtaTranslations[] = {
    {"ar-SA", "ota_url.label", "عنوان OTA مخصص"},
    {"ar-SA", "ota_url.invalid", "أدخل عنوان HTTP(S) بطول 255 بايت كحد أقصى، أو اترك الحقل فارغًا."},
    {"bg-BG", "ota_url.label", "Персонализиран OTA URL"},
    {"bg-BG", "ota_url.invalid", "Въведете HTTP(S) адрес до 255 байта или оставете празно."},
    {"ca-ES", "ota_url.label", "URL OTA personalitzat"},
    {"ca-ES", "ota_url.invalid",
     "Introdueix un URL HTTP(S) de fins a 255 bytes o deixa el camp buit."},
    {"cs-CZ", "ota_url.label", "Vlastní OTA URL"},
    {"cs-CZ", "ota_url.invalid",
     "Zadejte HTTP(S) adresu do 255 bajtů, nebo ponechte pole prázdné."},
    {"da-DK", "ota_url.label", "Brugerdefineret OTA URL"},
    {"da-DK", "ota_url.invalid",
     "Indtast en HTTP(S)-adresse på højst 255 byte, eller lad feltet stå tomt."},
    {"de-DE", "ota_url.label", "Benutzerdefinierte OTA-URL"},
    {"de-DE", "ota_url.invalid",
     "Eine HTTP(S)-URL mit maximal 255 Byte eingeben oder das Feld leer lassen."},
    {"el-GR", "ota_url.label", "Προσαρμοσμένη διεύθυνση OTA"},
    {"el-GR", "ota_url.invalid", "Εισαγάγετε URL HTTP(S) έως 255 byte ή αφήστε το πεδίο κενό."},
    {"es-ES", "ota_url.label", "URL OTA personalizada"},
    {"es-ES", "ota_url.invalid",
     "Introduce una URL HTTP(S) de hasta 255 bytes o deja el campo vacío."},
    {"fa-IR", "ota_url.label", "آدرس OTA سفارشی"},
    {"fa-IR", "ota_url.invalid",
     "یک نشانی HTTP(S) با حداکثر 255 بایت وارد کنید یا فیلد را خالی بگذارید."},
    {"fi-FI", "ota_url.label", "Mukautettu OTA-osoite"},
    {"fi-FI", "ota_url.invalid",
     "Anna enintään 255 tavun HTTP(S)-osoite tai jätä kenttä tyhjäksi."},
    {"fil-PH", "ota_url.label", "Custom OTA URL"},
    {"fil-PH", "ota_url.invalid",
     "Maglagay ng HTTP(S) URL na hanggang 255 byte o iwanang blangko."},
    {"fr-FR", "ota_url.label", "URL OTA personnalisée"},
    {"fr-FR", "ota_url.invalid",
     "Saisissez une URL HTTP(S) de 255 octets maximum ou laissez le champ vide."},
    {"he-IL", "ota_url.label", "כתובת OTA מותאמת אישית"},
    {"he-IL", "ota_url.invalid",
     "יש להזין כתובת HTTP(S) באורך של עד 255 בתים, או להשאיר את השדה ריק."},
    {"hi-IN", "ota_url.label", "कस्टम OTA URL"},
    {"hi-IN", "ota_url.invalid", "अधिकतम 255 बाइट का HTTP(S) पता दर्ज करें या फ़ील्ड खाली छोड़ दें।"},
    {"hr-HR", "ota_url.label", "Prilagođeni OTA URL"},
    {"hr-HR", "ota_url.invalid",
     "Unesite HTTP(S) adresu do 255 bajtova ili ostavite polje prazno."},
    {"hu-HU", "ota_url.label", "Egyéni OTA URL"},
    {"hu-HU", "ota_url.invalid",
     "Adjon meg egy legfeljebb 255 bájtos HTTP(S)-címet, vagy hagyja üresen a mezőt."},
    {"id-ID", "ota_url.label", "URL OTA Kustom"},
    {"id-ID", "ota_url.invalid", "Masukkan URL HTTP(S) maksimal 255 byte atau biarkan kosong."},
    {"it-IT", "ota_url.label", "URL OTA personalizzato"},
    {"it-IT", "ota_url.invalid",
     "Inserisci un URL HTTP(S) di massimo 255 byte o lascia il campo vuoto."},
    {"ja-JP", "ota_url.label", "カスタムOTA URL"},
    {"ja-JP", "ota_url.invalid", "255 バイト以内の HTTP(S) URL を入力するか、空欄にしてください。"},
    {"ko-KR", "ota_url.label", "사용자 정의 OTA URL"},
    {"ko-KR", "ota_url.invalid", "최대 255바이트의 HTTP(S) URL을 입력하거나 비워 두세요."},
    {"ms-MY", "ota_url.label", "URL OTA Tersuai"},
    {"ms-MY", "ota_url.invalid", "Masukkan URL HTTP(S) sehingga 255 bait atau biarkan kosong."},
    {"nb-NO", "ota_url.label", "Egendefinert OTA-URL"},
    {"nb-NO", "ota_url.invalid",
     "Skriv inn en HTTP(S)-adresse på maks. 255 byte, eller la feltet stå tomt."},
    {"nl-NL", "ota_url.label", "Aangepaste OTA-URL"},
    {"nl-NL", "ota_url.invalid",
     "Voer een HTTP(S)-URL van maximaal 255 bytes in of laat het veld leeg."},
    {"pl-PL", "ota_url.label", "Niestandardowy adres URL OTA"},
    {"pl-PL", "ota_url.invalid",
     "Wpisz adres HTTP(S) o długości do 255 bajtów lub pozostaw pole puste."},
    {"pt-PT", "ota_url.label", "URL OTA personalizado"},
    {"pt-PT", "ota_url.invalid", "Introduza um URL HTTP(S) até 255 bytes ou deixe o campo vazio."},
    {"ro-RO", "ota_url.label", "URL OTA personalizat"},
    {"ro-RO", "ota_url.invalid",
     "Introduceți un URL HTTP(S) de cel mult 255 de octeți sau lăsați câmpul gol."},
    {"ru-RU", "ota_url.label", "Пользовательский URL OTA"},
    {"ru-RU", "ota_url.invalid",
     "Введите HTTP(S) URL длиной до 255 байт или оставьте поле пустым."},
    {"sk-SK", "ota_url.label", "Vlastná URL OTA"},
    {"sk-SK", "ota_url.invalid",
     "Zadajte HTTP(S) adresu s najviac 255 bajtmi alebo nechajte pole prázdne."},
    {"sl-SI", "ota_url.label", "Prilagojen URL OTA"},
    {"sl-SI", "ota_url.invalid",
     "Vnesite naslov HTTP(S) z največ 255 bajti ali pustite polje prazno."},
    {"sr-RS", "ota_url.label", "Прилагођени OTA URL"},
    {"sr-RS", "ota_url.invalid", "Унесите HTTP(S) адресу до 255 бајтова или оставите поље празно."},
    {"sv-SE", "ota_url.label", "Anpassad OTA-URL"},
    {"sv-SE", "ota_url.invalid",
     "Ange en HTTP(S)-adress på högst 255 byte eller lämna fältet tomt."},
    {"th-TH", "ota_url.label", "URL OTA กำหนดเอง"},
    {"th-TH", "ota_url.invalid", "ป้อน URL HTTP(S) ไม่เกิน 255 ไบต์ หรือเว้นว่างไว้"},
    {"tr-TR", "ota_url.label", "Özel OTA URL"},
    {"tr-TR", "ota_url.invalid",
     "En fazla 255 baytlık bir HTTP(S) adresi girin veya alanı boş bırakın."},
    {"uk-UA", "ota_url.label", "Користувацька URL-адреса OTA"},
    {"uk-UA", "ota_url.invalid",
     "Введіть HTTP(S) URL завдовжки до 255 байтів або залиште поле порожнім."},
    {"vi-VN", "ota_url.label", "Địa chỉ OTA tùy chỉnh"},
    {"vi-VN", "ota_url.invalid", "Nhập URL HTTP(S) tối đa 255 byte hoặc để trống."},
    {"zh-CN", "ota_url.help", "留空使用默认 OTA 服务器。"},
    {"zh-CN", "ota_url.label", "自定义 OTA 地址"},
    {"zh-CN", "ota_url.invalid", "请输入 HTTP(S) 地址（最多 255 字节），或留空。"},
    {"zh-TW", "ota_url.label", "自定義 OTA 位址"},
    {"zh-TW", "ota_url.invalid", "請輸入 HTTP(S) 位址（最多 255 位元組），或留空。"},
};
constexpr Translation kSleepTranslations[] = {
    {"ar-SA", "title", "وضع السكون"},
    {"ar-SA", "enabled.label", "وضع السكون"},
    {"ar-SA", "dim.label", "تعتيم الشاشة بعد"},
    {"ar-SA", "off.label", "إيقاف التشغيل بعد"},
    {"ar-SA", "dim.unit", "دقيقة"},
    {"ar-SA", "off.unit", "دقيقة"},
    {"ar-SA", "off.order", "يجب أن يكون وقت إيقاف التشغيل بعد تعتيم الشاشة."},
    {"bg-BG", "title", "Режим на заспиване"},
    {"bg-BG", "enabled.label", "Режим на заспиване"},
    {"bg-BG", "dim.label", "Затъмняване след"},
    {"bg-BG", "off.label", "Изключване след"},
    {"bg-BG", "dim.unit", "мин"},
    {"bg-BG", "off.unit", "мин"},
    {"bg-BG", "off.order", "Времето за изключване трябва да е след времето за затъмняване."},
    {"ca-ES", "title", "Mode de repòs"},
    {"ca-ES", "enabled.label", "Mode de repòs"},
    {"ca-ES", "dim.label", "Atenua la pantalla al cap de"},
    {"ca-ES", "off.label", "Apaga el dispositiu al cap de"},
    {"ca-ES", "dim.unit", "min"},
    {"ca-ES", "off.unit", "min"},
    {"ca-ES", "off.order", "L'apagada ha de ser posterior a l'atenuació de la pantalla."},
    {"cs-CZ", "title", "Režim spánku"},
    {"cs-CZ", "enabled.label", "Režim spánku"},
    {"cs-CZ", "dim.label", "Ztlumit displej po"},
    {"cs-CZ", "off.label", "Vypnout zařízení po"},
    {"cs-CZ", "dim.unit", "min"},
    {"cs-CZ", "off.unit", "min"},
    {"cs-CZ", "off.order", "Vypnutí musí nastat později než ztlumení displeje."},
    {"da-DK", "title", "Dvaletilstand"},
    {"da-DK", "enabled.label", "Dvaletilstand"},
    {"da-DK", "dim.label", "Dæmp skærmen efter"},
    {"da-DK", "off.label", "Sluk enheden efter"},
    {"da-DK", "dim.unit", "min"},
    {"da-DK", "off.unit", "min"},
    {"da-DK", "off.order", "Enheden skal slukke senere, end skærmen dæmpes."},
    {"de-DE", "title", "Ruhemodus"},
    {"de-DE", "enabled.label", "Ruhemodus"},
    {"de-DE", "dim.label", "Display dimmen nach"},
    {"de-DE", "off.label", "Gerät ausschalten nach"},
    {"de-DE", "dim.unit", "Min."},
    {"de-DE", "off.unit", "Min."},
    {"de-DE", "off.order", "Das Ausschalten muss später als das Dimmen erfolgen."},
    {"el-GR", "title", "Λειτουργία ύπνου"},
    {"el-GR", "enabled.label", "Λειτουργία ύπνου"},
    {"el-GR", "dim.label", "Μείωση φωτεινότητας μετά από"},
    {"el-GR", "off.label", "Απενεργοποίηση μετά από"},
    {"el-GR", "dim.unit", "λεπτά"},
    {"el-GR", "off.unit", "λεπτά"},
    {"el-GR", "off.order", "Η απενεργοποίηση πρέπει να γίνεται μετά τη μείωση φωτεινότητας."},
    {"es-ES", "title", "Modo de suspensión"},
    {"es-ES", "enabled.label", "Modo de suspensión"},
    {"es-ES", "dim.label", "Atenuar pantalla tras"},
    {"es-ES", "off.label", "Apagar dispositivo tras"},
    {"es-ES", "dim.unit", "min"},
    {"es-ES", "off.unit", "min"},
    {"es-ES", "off.order", "El apagado debe ser posterior a la atenuación de la pantalla."},
    {"fa-IR", "title", "حالت خواب"},
    {"fa-IR", "enabled.label", "حالت خواب"},
    {"fa-IR", "dim.label", "کم‌نور شدن صفحه پس از"},
    {"fa-IR", "off.label", "خاموش شدن دستگاه پس از"},
    {"fa-IR", "dim.unit", "دقیقه"},
    {"fa-IR", "off.unit", "دقیقه"},
    {"fa-IR", "off.order",
     "زمان خاموش شدن باید بیشتر از زمان کم‌نور شدن صفحه "
     "باشد."},
    {"fi-FI", "title", "Lepotila"},
    {"fi-FI", "enabled.label", "Lepotila"},
    {"fi-FI", "dim.label", "Himmennä näyttö viiveellä"},
    {"fi-FI", "off.label", "Sammuta laite viiveellä"},
    {"fi-FI", "dim.unit", "min"},
    {"fi-FI", "off.unit", "min"},
    {"fi-FI", "off.order", "Sammutusviiveen on oltava näytön himmennysviivettä pidempi."},
    {"fil-PH", "title", "Mode ng pagtulog"},
    {"fil-PH", "enabled.label", "Mode ng pagtulog"},
    {"fil-PH", "dim.label", "Padilimin ang screen pagkalipas ng"},
    {"fil-PH", "off.label", "Patayin ang device pagkalipas ng"},
    {"fil-PH", "dim.unit", "min"},
    {"fil-PH", "off.unit", "min"},
    {"fil-PH", "off.order", "Dapat mas huli ang pagpatay kaysa sa pagdilim ng screen."},
    {"fr-FR", "title", "Mode veille"},
    {"fr-FR", "enabled.label", "Mode veille"},
    {"fr-FR", "dim.label", "Atténuer l'écran après"},
    {"fr-FR", "off.label", "Éteindre l'appareil après"},
    {"fr-FR", "dim.unit", "min"},
    {"fr-FR", "off.unit", "min"},
    {"fr-FR", "off.order", "L'extinction doit avoir lieu après l'atténuation de l'écran."},
    {"he-IL", "title", "מצב שינה"},
    {"he-IL", "enabled.label", "מצב שינה"},
    {"he-IL", "dim.label", "עמעום המסך לאחר"},
    {"he-IL", "off.label", "כיבוי המכשיר לאחר"},
    {"he-IL", "dim.unit", "דק׳"},
    {"he-IL", "off.unit", "דק׳"},
    {"he-IL", "off.order", "הכיבוי חייב להתרחש לאחר עמעום המסך."},
    {"hi-IN", "title", "स्लीप मोड"},
    {"hi-IN", "enabled.label", "स्लीप मोड"},
    {"hi-IN", "dim.label", "स्क्रीन मंद होने का समय"},
    {"hi-IN", "off.label", "डिवाइस बंद होने का समय"},
    {"hi-IN", "dim.unit", "मिनट"},
    {"hi-IN", "off.unit", "मिनट"},
    {"hi-IN", "off.order", "बंद होने का समय स्क्रीन मंद होने के समय से अधिक होना चाहिए।"},
    {"hr-HR", "title", "Način mirovanja"},
    {"hr-HR", "enabled.label", "Način mirovanja"},
    {"hr-HR", "dim.label", "Zatamni zaslon nakon"},
    {"hr-HR", "off.label", "Isključi uređaj nakon"},
    {"hr-HR", "dim.unit", "min"},
    {"hr-HR", "off.unit", "min"},
    {"hr-HR", "off.order", "Isključivanje mora biti nakon zatamnjenja zaslona."},
    {"hu-HU", "title", "Alvó mód"},
    {"hu-HU", "enabled.label", "Alvó mód"},
    {"hu-HU", "dim.label", "Kijelző halványítása ennyi idő után"},
    {"hu-HU", "off.label", "Kikapcsolás ennyi idő után"},
    {"hu-HU", "dim.unit", "perc"},
    {"hu-HU", "off.unit", "perc"},
    {"hu-HU", "off.order", "A kikapcsolásnak a kijelző halványítása után kell történnie."},
    {"id-ID", "title", "Mode tidur"},
    {"id-ID", "enabled.label", "Mode tidur"},
    {"id-ID", "dim.label", "Redupkan layar setelah"},
    {"id-ID", "off.label", "Matikan perangkat setelah"},
    {"id-ID", "dim.unit", "menit"},
    {"id-ID", "off.unit", "menit"},
    {"id-ID", "off.order", "Waktu mati harus lebih lama daripada waktu peredupan layar."},
    {"it-IT", "title", "Modalità sospensione"},
    {"it-IT", "enabled.label", "Modalità sospensione"},
    {"it-IT", "dim.label", "Attenua schermo dopo"},
    {"it-IT", "off.label", "Spegni dispositivo dopo"},
    {"it-IT", "dim.unit", "min"},
    {"it-IT", "off.unit", "min"},
    {"it-IT", "off.order", "Lo spegnimento deve avvenire dopo l'attenuazione dello schermo."},
    {"ja-JP", "title", "スリープモード"},
    {"ja-JP", "enabled.label", "スリープモード"},
    {"ja-JP", "dim.label", "画面減光まで"},
    {"ja-JP", "off.label", "電源オフまで"},
    {"ja-JP", "dim.unit", "分"},
    {"ja-JP", "off.unit", "分"},
    {"ja-JP", "off.order", "電源オフの時間は画面減光の時間より長くしてください。"},
    {"ko-KR", "title", "절전 모드"},
    {"ko-KR", "enabled.label", "절전 모드"},
    {"ko-KR", "dim.label", "화면 어두워짐까지"},
    {"ko-KR", "off.label", "전원 꺼짐까지"},
    {"ko-KR", "dim.unit", "분"},
    {"ko-KR", "off.unit", "분"},
    {"ko-KR", "off.order", "전원이 꺼지는 시간은 화면이 어두워지는 시간보다 길어야 합니다."},
    {"ms-MY", "title", "Mod tidur"},
    {"ms-MY", "enabled.label", "Mod tidur"},
    {"ms-MY", "dim.label", "Malapkan skrin selepas"},
    {"ms-MY", "off.label", "Matikan peranti selepas"},
    {"ms-MY", "dim.unit", "minit"},
    {"ms-MY", "off.unit", "minit"},
    {"ms-MY", "off.order",
     "Masa peranti dimatikan mesti lebih lama daripada masa skrin dimalapkan."},
    {"nb-NO", "title", "Hvilemodus"},
    {"nb-NO", "enabled.label", "Hvilemodus"},
    {"nb-NO", "dim.label", "Demp skjermen etter"},
    {"nb-NO", "off.label", "Slå av enheten etter"},
    {"nb-NO", "dim.unit", "min"},
    {"nb-NO", "off.unit", "min"},
    {"nb-NO", "off.order", "Enheten må slås av senere enn skjermen dempes."},
    {"nl-NL", "title", "Slaapstand"},
    {"nl-NL", "enabled.label", "Slaapstand"},
    {"nl-NL", "dim.label", "Scherm dimmen na"},
    {"nl-NL", "off.label", "Apparaat uitschakelen na"},
    {"nl-NL", "dim.unit", "min"},
    {"nl-NL", "off.unit", "min"},
    {"nl-NL", "off.order", "Uitschakelen moet later plaatsvinden dan het dimmen van het scherm."},
    {"pl-PL", "title", "Tryb uśpienia"},
    {"pl-PL", "enabled.label", "Tryb uśpienia"},
    {"pl-PL", "dim.label", "Przyciemnij ekran po"},
    {"pl-PL", "off.label", "Wyłącz urządzenie po"},
    {"pl-PL", "dim.unit", "min"},
    {"pl-PL", "off.unit", "min"},
    {"pl-PL", "off.order", "Wyłączenie musi nastąpić później niż przyciemnienie ekranu."},
    {"pt-PT", "title", "Modo de suspensão"},
    {"pt-PT", "enabled.label", "Modo de suspensão"},
    {"pt-PT", "dim.label", "Reduzir brilho após"},
    {"pt-PT", "off.label", "Desligar dispositivo após"},
    {"pt-PT", "dim.unit", "min"},
    {"pt-PT", "off.unit", "min"},
    {"pt-PT", "off.order", "O dispositivo deve desligar-se depois de o brilho ser reduzido."},
    {"ro-RO", "title", "Mod repaus"},
    {"ro-RO", "enabled.label", "Mod repaus"},
    {"ro-RO", "dim.label", "Reduce luminozitatea după"},
    {"ro-RO", "off.label", "Oprește dispozitivul după"},
    {"ro-RO", "dim.unit", "min"},
    {"ro-RO", "off.unit", "min"},
    {"ro-RO", "off.order", "Oprirea trebuie să aibă loc după reducerea luminozității."},
    {"ru-RU", "title", "Режим сна"},
    {"ru-RU", "enabled.label", "Режим сна"},
    {"ru-RU", "dim.label", "Затемнение через"},
    {"ru-RU", "off.label", "Выключение через"},
    {"ru-RU", "dim.unit", "мин"},
    {"ru-RU", "off.unit", "мин"},
    {"ru-RU", "off.order", "Время выключения должно быть больше времени затемнения."},
    {"sk-SK", "title", "Režim spánku"},
    {"sk-SK", "enabled.label", "Režim spánku"},
    {"sk-SK", "dim.label", "Stlmiť displej po"},
    {"sk-SK", "off.label", "Vypnúť zariadenie po"},
    {"sk-SK", "dim.unit", "min"},
    {"sk-SK", "off.unit", "min"},
    {"sk-SK", "off.order", "Vypnutie musí nastať neskôr než stlmenie displeja."},
    {"sl-SI", "title", "Način spanja"},
    {"sl-SI", "enabled.label", "Način spanja"},
    {"sl-SI", "dim.label", "Zatemni zaslon po"},
    {"sl-SI", "off.label", "Izklopi napravo po"},
    {"sl-SI", "dim.unit", "min"},
    {"sl-SI", "off.unit", "min"},
    {"sl-SI", "off.order", "Izklop mora biti poznejši od zatemnitve zaslona."},
    {"sr-RS", "title", "Режим спавања"},
    {"sr-RS", "enabled.label", "Режим спавања"},
    {"sr-RS", "dim.label", "Затамни екран након"},
    {"sr-RS", "off.label", "Искључи уређај након"},
    {"sr-RS", "dim.unit", "мин"},
    {"sr-RS", "off.unit", "мин"},
    {"sr-RS", "off.order", "Искључивање мора бити касније од затамњења екрана."},
    {"sv-SE", "title", "Viloläge"},
    {"sv-SE", "enabled.label", "Viloläge"},
    {"sv-SE", "dim.label", "Dimma skärmen efter"},
    {"sv-SE", "off.label", "Stäng av enheten efter"},
    {"sv-SE", "dim.unit", "min"},
    {"sv-SE", "off.unit", "min"},
    {"sv-SE", "off.order", "Enheten måste stängas av senare än skärmen dimmas."},
    {"th-TH", "title", "โหมดพักเครื่อง"},
    {"th-TH", "enabled.label", "โหมดพักเครื่อง"},
    {"th-TH", "dim.label", "หรี่หน้าจอหลังจาก"},
    {"th-TH", "off.label", "ปิดเครื่องหลังจาก"},
    {"th-TH", "dim.unit", "นาที"},
    {"th-TH", "off.unit", "นาที"},
    {"th-TH", "off.order", "เวลาปิดเครื่องต้องมากกว่าเวลาหรี่หน้าจอ"},
    {"tr-TR", "title", "Uyku modu"},
    {"tr-TR", "enabled.label", "Uyku modu"},
    {"tr-TR", "dim.label", "Ekranı karartma süresi"},
    {"tr-TR", "off.label", "Kapanma süresi"},
    {"tr-TR", "dim.unit", "dk"},
    {"tr-TR", "off.unit", "dk"},
    {"tr-TR", "off.order", "Kapanma süresi ekranı karartma süresinden uzun olmalıdır."},
    {"uk-UA", "title", "Режим сну"},
    {"uk-UA", "enabled.label", "Режим сну"},
    {"uk-UA", "dim.label", "Затемнення через"},
    {"uk-UA", "off.label", "Вимкнення через"},
    {"uk-UA", "dim.unit", "хв"},
    {"uk-UA", "off.unit", "хв"},
    {"uk-UA", "off.order", "Час вимкнення має бути більшим за час затемнення."},
    {"vi-VN", "title", "Chế độ ngủ"},
    {"vi-VN", "enabled.label", "Chế độ ngủ"},
    {"vi-VN", "dim.label", "Làm mờ màn hình sau"},
    {"vi-VN", "off.label", "Tắt thiết bị sau"},
    {"vi-VN", "dim.unit", "phút"},
    {"vi-VN", "off.unit", "phút"},
    {"vi-VN", "off.order", "Thời gian tắt phải lớn hơn thời gian làm mờ màn hình."},
    {"zh-CN", "help", "留空沿用板型默认值。填 0 可关闭对应计时。"},
    {"zh-CN", "title", "睡眠模式"},
    {"zh-CN", "enabled.label", "睡眠模式"},
    {"zh-CN", "dim.label", "屏幕变暗时间"},
    {"zh-CN", "off.label", "关机时间"},
    {"zh-CN", "dim.unit", "分钟"},
    {"zh-CN", "off.unit", "分钟"},
    {"zh-CN", "off.order", "关机时间必须大于屏幕变暗时间。"},
    {"zh-TW", "title", "睡眠模式"},
    {"zh-TW", "enabled.label", "睡眠模式"},
    {"zh-TW", "dim.label", "螢幕變暗時間"},
    {"zh-TW", "off.label", "關機時間"},
    {"zh-TW", "dim.unit", "分鐘"},
    {"zh-TW", "off.unit", "分鐘"},
    {"zh-TW", "off.order", "關機時間必須大於螢幕變暗時間。"},
};

SaveResult StorageError(esp_err_t error) {
    if (error == ESP_OK) {
        return {};
    }
    return {"", "Could not save settings.", "save_failed"};
}

SaveResult SaveOta(const Values& values) {
    const auto entry = values.find("ota_url");
    if (entry == values.end()) {
        return {};
    }
    const auto& url = std::get<std::string>(entry->second);
    if (!Ota::IsValidUrl(url)) {
        return {"ota_url", "Enter a valid HTTP(S) URL, or leave this blank.", "ota_url.invalid"};
    }
    nvs_handle_t nvs;
    auto result = nvs_open("wifi", NVS_READWRITE, &nvs);
    if (result == ESP_OK) {
        result = nvs_set_str(nvs, "ota_url", url.c_str());
        if (result == ESP_OK) {
            result = nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    return StorageError(result);
}

Value ReadMinutes(Settings& settings, const char* key) {
    const auto value = settings.GetInt(key, -1);
    if (value >= 0 && value <= 1440) {
        return value;
    }
    return nullptr;
}

Values LoadSleep() {
    Settings settings("wifi", false);
    return {{"enabled", settings.GetBool("sleep_mode", true)},
            {"dim", ReadMinutes(settings, "sleep_dim_min")},
            {"off", ReadMinutes(settings, "sleep_off_min")}};
}

esp_err_t WriteMinutes(nvs_handle_t nvs, const char* key, const Value& value) {
    if (const auto minutes = std::get_if<int32_t>(&value)) {
        return nvs_set_i32(nvs, key, *minutes);
    }
    auto err = nvs_erase_key(nvs, key);
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}

SaveResult SaveSleep(const Values& submitted) {
    auto values = LoadSleep();
    for (const auto& [key, value] : submitted) {
        values[key] = value;
    }
    const auto dim = std::get_if<int32_t>(&values.at("dim"));
    const auto off = std::get_if<int32_t>(&values.at("off"));
    const bool enabled = std::get<bool>(values.at("enabled"));
    if (enabled && dim && off && *dim > 0 && *off > 0 && *off <= *dim) {
        return {"off", "Power off must be later than screen dimming.", "off.order"};
    }

    nvs_handle_t nvs;
    auto result = nvs_open("wifi", NVS_READWRITE, &nvs);
    if (result == ESP_OK) {
        if (auto it = submitted.find("enabled"); it != submitted.end()) {
            result = nvs_set_u8(nvs, "sleep_mode", enabled);
        }
        if (enabled && result == ESP_OK && submitted.contains("dim")) {
            result = WriteMinutes(nvs, "sleep_dim_min", values.at("dim"));
        }
        if (enabled && result == ESP_OK && submitted.contains("off")) {
            result = WriteMinutes(nvs, "sleep_off_min", values.at("off"));
        }
        if (result == ESP_OK) {
            result = nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    return StorageError(result);
}
}  // namespace

std::vector<wifi_config::Group> MakeXiaozhiPortalSettings() {
    using namespace wifi_config;
    return {
        {.id = "ota",
         .title = "OTA",
         .fields = {{.id = "ota_url", .label = "Custom OTA URL",
                     .help = "Leave blank to use the default OTA server.",
                     .max_length = Ota::kMaxUrlLength}},
         .translations = kOtaTranslations,
         .load = [] {
             Settings settings("wifi", false);
             auto url = settings.GetString("ota_url");
             if (!Ota::IsValidUrl(url)) {
                 url.clear();
             }
             return Values{{"ota_url", std::move(url)}};
         },
         .save = SaveOta},
        {.id = "sleep",
         .title = "Sleep mode",
         .help = "Leave a timer blank to use the board default. Enter 0 to disable that timer.",
         .fields = {
             {.id = "enabled", .type = FieldType::Boolean, .label = "Sleep mode"},
             {.id = "dim", .type = FieldType::Integer, .label = "Screen dim after", .unit = "min",
              .min = 0, .max = 1440, .enabled_by = "enabled"},
             {.id = "off", .type = FieldType::Integer, .label = "Power off after", .unit = "min",
              .min = 0, .max = 1440, .enabled_by = "enabled"},
         },
         .translations = kSleepTranslations,
         .load = LoadSleep,
         .save = SaveSleep},
    };
}
