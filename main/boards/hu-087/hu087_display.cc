#include "hu087_display.h"

#include <ctime>

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>

#include "application.h"
#include "audio_codec.h"
#include "audio_service.h"
#include "device_state.h"
#include "wifi_manager.h"

#define TAG "Hu087Display"

LV_FONT_DECLARE(font_noto_sans_basic_30_4);
LV_FONT_DECLARE(font_noto_sans_basic_14_1);

namespace {
constexpr float kMouthOpenThreshold = 0.10f;
constexpr float kMouthCloseThreshold = 0.05f;
constexpr int64_t kMinHoldUs = 90000;  // não troca de estado mais rápido que isso
constexpr int64_t kThrottleUs = 60000;  // não processa mais que isso (a task de
                                        // áudio chama a cada 20-30ms)
constexpr int kTickMs = 200;
// 1 minuto parado no relógio (idle) e a tela apaga sozinha (economia de
// bateria) - o tick roda a cada 200ms, 60000/200 = 300 ticks.
constexpr int kTicksAntesDeDormir = 60000 / kTickMs;
}  // namespace

Hu087Display::Hu087Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, bool mirror_x, bool mirror_y)
    : OledDisplay(panel_io, panel, width, height, mirror_x, mirror_y) {}

Hu087Display::~Hu087Display() {
    if (clock_timer_ != nullptr) {
        lv_timer_delete(clock_timer_);
    }
}

void Hu087Display::SetAudioCodecForSync(AudioCodec* codec) {
    audio_codec_ = codec;
    if (audio_codec_ != nullptr) {
        audio_codec_->SetAudioLevelCallback([this](float level) {
            // Executa na task de saída de áudio - não pode fazer nada
            // pesado aqui, só atualizar um número e (de vez em quando)
            // trocar o tamanho de um objeto do LVGL.
            OnAudioLevel(level);
        });
    }
}

void Hu087Display::SetupUI() {
    // Monta a UI padrão primeiro (tela de "conectando", ícones de wifi,
    // etc - continua igual, só fica visível enquanto ainda não tá pronta).
    OledDisplay::SetupUI();

    DisplayLockGuard lock(this);

    /* ---- Relógio (hora + data), tela toda, quando parado ---- */
    clock_container_ = lv_obj_create(container_);
    lv_obj_set_size(clock_container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(clock_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(clock_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(clock_container_, 0, 0);
    lv_obj_set_style_pad_row(clock_container_, 0, 0);
    lv_obj_set_style_border_width(clock_container_, 0, 0);
    lv_obj_set_style_radius(clock_container_, 0, 0);
    lv_obj_set_scrollbar_mode(clock_container_, LV_SCROLLBAR_MODE_OFF);

    time_label_ = lv_label_create(clock_container_);
    lv_obj_set_style_text_font(time_label_, &font_noto_sans_basic_30_4, 0);
    lv_label_set_text(time_label_, "--:--");

    // Data numa fonte bem fina - é só um detalhe secundário embaixo da
    // hora, não precisa chamar tanta atenção quanto ela.
    date_label_ = lv_label_create(clock_container_);
    lv_obj_set_style_text_font(date_label_, &font_noto_sans_basic_14_1, 0);
    lv_label_set_text(date_label_, "--/--");

    lv_obj_add_flag(clock_container_, LV_OBJ_FLAG_HIDDEN);

    /* ---- Carinha (2 olhos ovais + pupila + boquinha) ---- */
    face_container_ = lv_obj_create(container_);
    lv_obj_set_size(face_container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(face_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(face_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(face_container_, 0, 0);
    lv_obj_set_style_pad_row(face_container_, 6, 0);
    lv_obj_set_style_border_width(face_container_, 0, 0);
    lv_obj_set_style_radius(face_container_, 0, 0);
    lv_obj_set_scrollbar_mode(face_container_, LV_SCROLLBAR_MODE_OFF);

    eyes_row_ = lv_obj_create(face_container_);
    lv_obj_set_size(eyes_row_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(eyes_row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(eyes_row_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(eyes_row_, 0, 0);
    lv_obj_set_style_pad_column(eyes_row_, 14, 0);
    lv_obj_set_style_border_width(eyes_row_, 0, 0);
    lv_obj_set_scrollbar_mode(eyes_row_, LV_SCROLLBAR_MODE_OFF);

    // Cada olho: um oval (mais alto que largo) com borda preta, e dentro
    // dele uma pupila preta sólida bem no meio.
    eye_left_ = lv_obj_create(eyes_row_);
    eye_right_ = lv_obj_create(eyes_row_);
    for (lv_obj_t* eye : {eye_left_, eye_right_}) {
        lv_obj_set_size(eye, 22, 28);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(eye, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(eye, 2, 0);
        lv_obj_set_style_border_color(eye, lv_color_black(), 0);
        lv_obj_set_scrollbar_mode(eye, LV_SCROLLBAR_MODE_OFF);
    }

    pupil_left_ = lv_obj_create(eye_left_);
    pupil_right_ = lv_obj_create(eye_right_);
    for (lv_obj_t* pupil : {pupil_left_, pupil_right_}) {
        lv_obj_set_size(pupil, 10, 10);
        lv_obj_set_style_radius(pupil, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(pupil, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(pupil, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(pupil, 0, 0);
        lv_obj_center(pupil);
    }

    // Boquinha: fica fina/fechada em repouso, "abre" quando o volume real
    // do áudio que está tocando passa de um limite (ver OnAudioLevel()).
    mouth_ = lv_obj_create(face_container_);
    lv_obj_set_size(mouth_, 16, 3);
    lv_obj_set_style_radius(mouth_, 2, 0);
    lv_obj_set_style_bg_color(mouth_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(mouth_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mouth_, 0, 0);

    lv_obj_add_flag(face_container_, LV_OBJ_FLAG_HIDDEN);

    /* ---- Tela "Conectando... <rede>" (substitui a tela padrão do xiaozhi) ---- */
    connecting_container_ = lv_obj_create(container_);
    lv_obj_set_size(connecting_container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(connecting_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(connecting_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(connecting_container_, 0, 0);
    lv_obj_set_style_pad_row(connecting_container_, 2, 0);
    lv_obj_set_style_border_width(connecting_container_, 0, 0);
    lv_obj_set_style_radius(connecting_container_, 0, 0);
    lv_obj_set_scrollbar_mode(connecting_container_, LV_SCROLLBAR_MODE_OFF);

    connecting_label_ = lv_label_create(connecting_container_);
    lv_obj_set_style_text_font(connecting_label_, &font_noto_sans_basic_14_1, 0);
    lv_label_set_text(connecting_label_, "Conectando...");

    connecting_ssid_ = lv_label_create(connecting_container_);
    lv_obj_set_style_text_font(connecting_ssid_, &font_noto_sans_basic_14_1, 0);
    lv_label_set_text(connecting_ssid_, "");

    lv_obj_add_flag(connecting_container_, LV_OBJ_FLAG_HIDDEN);

    ticks_ate_dormir_ = kTicksAntesDeDormir;
    clock_timer_ = lv_timer_create(ClockTimerCallback, kTickMs, this);
    Tick();
}

void Hu087Display::MostrarConectando() {
    // Aperto no relógio offline pra falar. Além de marcar a intenção (o
    // Tick() engata a conversa quando conectar), já troca a tela na hora -
    // sem esperar o próximo tick - pra não piscar o relógio antes da tela
    // de conexão.
    pedir_conversa_ = true;
    conectando_visivel_ = true;
    DisplayLockGuard lock(this);
    if (clock_container_ != nullptr) lv_obj_add_flag(clock_container_, LV_OBJ_FLAG_HIDDEN);
    if (connecting_container_ != nullptr) {
        if (connecting_ssid_ != nullptr) lv_label_set_text(connecting_ssid_, "");
        lv_obj_remove_flag(connecting_container_, LV_OBJ_FLAG_HIDDEN);
    }
}

void Hu087Display::ApagarTelaJa() {
    ja_conversou_ = true;  // garante o caminho "desligar pós-conversa" no Tick
    desligar_pedido_ = true;  // não deixa o Tick reacender a tela até dormir
    DisplayLockGuard lock(this);
    SetDisplayPower(false);
    screen_asleep_ = true;
}

void Hu087Display::ClockTimerCallback(lv_timer_t* timer) {
    auto self = static_cast<Hu087Display*>(lv_timer_get_user_data(timer));
    self->Tick();
}

void Hu087Display::Tick() {
    auto& app = Application::GetInstance();
    auto state = app.GetDeviceState();
    bool is_idle = (state == kDeviceStateIdle);
    bool is_conversing = (state == kDeviceStateListening || state == kDeviceStateSpeaking ||
                          state == kDeviceStateNotifying);
    bool rede_subindo = (state == kDeviceStateStarting || state == kDeviceStateConnecting ||
                         state == kDeviceStateActivating || state == kDeviceStateUnknown);

    // Acabou de conversar e voltou pra idle: NÃO mostra o relógio, apaga a
    // tela na hora e dorme assim que o áudio (ex: "tchau") terminar de sair.
    bool desligando_pos_conversa = ja_conversou_ && is_idle;

    // Qual das 3 telas mostrar:
    //  - carinha: conversando
    //  - "Conectando...": (a) boot no interruptor, wifi subindo de verdade;
    //    ou (b) apertei pra falar (pedir_conversa_) - fica até a conversa
    //    começar de fato, sem piscar o relógio no meio.
    //  - relógio: idle, ou modo relógio offline (hora vem do RTC) - mas NÃO
    //    logo depois de uma conversa (aí é pra desligar direto).
    bool mostrar_conectando =
        !is_conversing &&
        (pedir_conversa_ || (rede_subindo && !modo_relogio_offline_ && !is_idle));
    bool mostrar_relogio = !is_conversing && !mostrar_conectando &&
                           !desligando_pos_conversa && (is_idle || modo_relogio_offline_);
    conectando_visivel_ = mostrar_conectando;  // pro botão consultar (EstaConectando)

    DisplayLockGuard lock(this);

    // Tela padrão do xiaozhi (top/status/content, com o "Em espera") só
    // aparece se NENHuma das nossas telas estiver no comando - e nunca no
    // momento de desligar pós-conversa (senão pisca "Em espera").
    bool esconder_padrao = is_conversing || mostrar_conectando || mostrar_relogio ||
                           desligando_pos_conversa;
    if (top_bar_ != nullptr) {
        esconder_padrao ? lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN)
                        : lv_obj_remove_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (status_bar_ != nullptr) {
        esconder_padrao ? lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN)
                        : lv_obj_remove_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (content_ != nullptr) {
        esconder_padrao ? lv_obj_add_flag(content_, LV_OBJ_FLAG_HIDDEN)
                        : lv_obj_remove_flag(content_, LV_OBJ_FLAG_HIDDEN);
    }

    if (clock_container_ != nullptr) {
        mostrar_relogio ? lv_obj_remove_flag(clock_container_, LV_OBJ_FLAG_HIDDEN)
                        : lv_obj_add_flag(clock_container_, LV_OBJ_FLAG_HIDDEN);
    }
    if (face_container_ != nullptr) {
        is_conversing ? lv_obj_remove_flag(face_container_, LV_OBJ_FLAG_HIDDEN)
                      : lv_obj_add_flag(face_container_, LV_OBJ_FLAG_HIDDEN);
    }
    if (connecting_container_ != nullptr) {
        if (mostrar_conectando) {
            if (connecting_ssid_ != nullptr) {
                // Só mostra o nome da rede DEPOIS de conectar de verdade -
                // enquanto tenta, o framework fica pulando de rede salva em
                // rede salva e o nome ficava piscando ("barbearia" -> casa
                // -> ...). Até lá, só o "Conectando..." sozinho.
                auto& wifi = WifiManager::GetInstance();
                lv_label_set_text(connecting_ssid_,
                                  wifi.IsConnected() ? wifi.GetSsid().c_str() : "");
            }
            lv_obj_remove_flag(connecting_container_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(connecting_container_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (state != kDeviceStateSpeaking && mouth_open_) {
        SetMouthOpen(false);
    }

    UpdateBlink(is_conversing);

    // Assim que a conexão (pedida pelo botão) terminar e cair em idle, engata
    // a conversa uma vez.
    if (pedir_conversa_ && is_idle) {
        pedir_conversa_ = false;
        conectando_visivel_ = false;
        desligar_pedido_ = false;
        // Já conectou: sai do "modo relógio offline" pra o fluxo voltar ao
        // normal (a partir daqui é uma sessão online comum, até dormir).
        modo_relogio_offline_ = false;
        app.ToggleChatState();
        ticks_ate_dormir_ = kTicksAntesDeDormir;
        return;
    }

    /* ---------- Economia de bateria: deep sleep ---------- */
    if (is_conversing) {
        // Não reacende a tela se eu apertei pra encerrar (o estado ainda
        // pode não ter virado idle nesse tick).
        if (screen_asleep_ && !desligar_pedido_) {
            SetDisplayPower(true);
            screen_asleep_ = false;
        }
        ja_conversou_ = true;
        ticks_ate_dormir_ = kTicksAntesDeDormir;
        return;
    }

    // Enquanto a tela "Conectando..." estiver no ar (boot no interruptor ou
    // conexão que pedi pra falar), não conta pra dormir - deixa a rede subir.
    if (pedir_conversa_ || conectando_visivel_) {
        return;
    }

    // Só conta pra dormir quando está de fato parado no relógio: idle
    // normal, ou modo offline com o estado ainda preso em "starting" (nunca
    // conectou). Qualquer outro estado (connecting, activating, upgrade,
    // wifi config...) não mexe.
    bool no_relogio = is_idle || (modo_relogio_offline_ && rede_subindo);
    if (!no_relogio) {
        return;
    }

    if (!screen_asleep_) {
        UpdateClockText();
        // Logo depois de uma conversa, desliga tudo quase na hora (o usuário
        // pediu: "ao terminar/se despedir, pode desligar"). Sem conversa,
        // espera 1 minuto parado.
        if (ja_conversou_ && ticks_ate_dormir_ > 3) {
            ticks_ate_dormir_ = 3;
        }
        if (--ticks_ate_dormir_ <= 0) {
            SetDisplayPower(false);
            screen_asleep_ = true;
        }
    }

    // Guarda pra dormir: no fluxo normal usa o CanEnterSleepMode (exige idle
    // + sem áudio). No modo relógio offline o estado fica preso em "starting"
    // (nunca conectou), então aceita dormir desde que o áudio esteja parado.
    bool pode_dormir = app.CanEnterSleepMode() ||
                       (modo_relogio_offline_ && app.GetAudioService().IsIdle());
    if (screen_asleep_ && !deep_sleep_pedido_ && on_enter_deep_sleep_ && pode_dormir) {
        deep_sleep_pedido_ = true;
        on_enter_deep_sleep_();
    }
}

void Hu087Display::WakeScreen() {
    desligar_pedido_ = false;
    if (screen_asleep_) {
        SetDisplayPower(true);
        screen_asleep_ = false;
        UpdateClockText();
    }
    ticks_ate_dormir_ = kTicksAntesDeDormir;
}

void Hu087Display::UpdateBlink(bool face_visible) {
    if (eye_left_ == nullptr || eye_right_ == nullptr) {
        return;
    }
    if (!face_visible) {
        // Não desperdiça tempo piscando escondido - só reresincroniza o
        // contador quando a carinha aparecer de novo.
        eyes_closed_ = false;
        return;
    }

    if (eyes_closed_) {
        // Ficou "fechado" por 1 tick (200ms) - já reabre.
        for (lv_obj_t* eye : {eye_left_, eye_right_}) {
            lv_obj_set_size(eye, 22, 28);
        }
        if (pupil_left_ != nullptr) lv_obj_remove_flag(pupil_left_, LV_OBJ_FLAG_HIDDEN);
        if (pupil_right_ != nullptr) lv_obj_remove_flag(pupil_right_, LV_OBJ_FLAG_HIDDEN);
        eyes_closed_ = false;
        // Próxima piscada daqui uns 2-5s (10 a 25 ticks de 200ms).
        ticks_until_blink_ = 10 + (esp_random() % 16);
        return;
    }

    ticks_until_blink_--;
    if (ticks_until_blink_ <= 0) {
        // "Fecha" o olho - achata bem fininho, esconde a pupila.
        for (lv_obj_t* eye : {eye_left_, eye_right_}) {
            lv_obj_set_size(eye, 22, 3);
        }
        if (pupil_left_ != nullptr) lv_obj_add_flag(pupil_left_, LV_OBJ_FLAG_HIDDEN);
        if (pupil_right_ != nullptr) lv_obj_add_flag(pupil_right_, LV_OBJ_FLAG_HIDDEN);
        eyes_closed_ = true;
    }
}

void Hu087Display::OnAudioLevel(float level) {
    if (level > mouth_level_smoothed_) {
        mouth_level_smoothed_ = mouth_level_smoothed_ * 0.35f + level * 0.65f;
    } else {
        mouth_level_smoothed_ = mouth_level_smoothed_ * 0.8f + level * 0.2f;
    }

    int64_t now = esp_timer_get_time();
    if (now - last_mouth_check_us_ < kThrottleUs) {
        return;
    }
    last_mouth_check_us_ = now;

    bool should_open = mouth_open_ ? (mouth_level_smoothed_ > kMouthCloseThreshold)
                                   : (mouth_level_smoothed_ > kMouthOpenThreshold);
    if (should_open != mouth_open_) {
        SetMouthOpen(should_open);
    }
}

void Hu087Display::SetMouthOpen(bool open) {
    mouth_open_ = open;
    DisplayLockGuard lock(this);
    if (mouth_ == nullptr) {
        return;
    }
    if (open) {
        lv_obj_set_size(mouth_, 12, 10);
    } else {
        lv_obj_set_size(mouth_, 16, 3);
    }
}

void Hu087Display::UpdateClockText() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2020 - 1900)) {
        // Ainda não sincronizou a hora pela internet (SNTP) - não mostra
        // lixo, fica esperando.
        return;
    }

    char time_str[6];
    strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);

    char date_str[6];
    strftime(date_str, sizeof(date_str), "%d/%m", &timeinfo);

    if (time_label_ != nullptr) {
        lv_label_set_text(time_label_, time_str);
    }
    if (date_label_ != nullptr) {
        lv_label_set_text(date_label_, date_str);
    }
}
