#ifndef HU087_DISPLAY_H
#define HU087_DISPLAY_H

#include <functional>
#include <string>

#include "display/oled_display.h"

class AudioCodec;

// Tela customizada da hu-087. Três estados de tela, alternados sozinhos
// conforme o estado do dispositivo:
//  - Ligando/conectando (ainda não pronta): a tela padrão do xiaozhi
//    (status/ícones de wifi) - sem mexer em nada aqui.
//  - Parada (idle): hora + data bem grandes, tela toda.
//  - Conversando (ouvindo/falando): carinha desenhada na mão (2 olhos
//    ovais com pupila + boquinha que abre/fecha de acordo com o volume
//    real do áudio que está saindo, via SetAudioCodecForSync()).
class Hu087Display : public OledDisplay {
public:
    Hu087Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                 int height, bool mirror_x, bool mirror_y);
    ~Hu087Display();

    virtual void SetupUI() override;

    // Liga o gancho de nível de áudio do codec na boquinha - chame depois
    // de criar o AudioCodec da placa.
    void SetAudioCodecForSync(AudioCodec* codec);

    // Economia de bateria: a tela (o painel físico de verdade, não só o
    // conteúdo) desliga sozinha depois de 1 minuto parada no relógio.
    // Nunca desliga durante conversa. O botão físico usa isso pra decidir
    // se só acorda a tela ou se já acorda a Sofia pra escutar.
    bool IsScreenAsleep() const { return screen_asleep_; }
    void WakeScreen();

    // Depois de 1 minuto parado no relógio, além de apagar a tela, o
    // relógio entra em deep sleep de verdade (rádio, CPU, tudo desligado -
    // ~10µA). Quem faz o desligamento em si é a placa (precisa mexer no
    // pino do amplificador e configurar o botão como fonte de despertar),
    // então ela registra aqui o que fazer. Só é chamado quando o
    // Application confirma que dá pra dormir (idle, sem áudio no ar).
    void OnEnterDeepSleep(std::function<void()> callback) { on_enter_deep_sleep_ = callback; }

    // Modo "relógio offline": acordou do deep sleep pelo botão, o wifi está
    // desligado. Mostra o relógio (hora vinda do RTC) na hora, sem tela de
    // conexão. O board liga isso no boot quando o wake foi por botão.
    void SetModoRelogioOffline(bool on) { modo_relogio_offline_ = on; }

    // Nome da rede wifi salva, pra mostrar embaixo do "Conectando...".
    void SetSsid(const std::string& ssid) { ssid_ = ssid; }

    // Força a tela "Conectando... <rede>" e marca que, assim que a conexão
    // terminar (estado idle), é pra já engatar a conversa. Chamado pelo
    // board quando aperto o botão no relógio offline pra falar com o xiaozhi.
    void MostrarConectando();

    // A tela "Conectando..." está na frente agora? (o board usa pra decidir
    // que um toque nela = entrar no modo de configurar outro wifi.)
    bool EstaConectando() const { return conectando_visivel_; }

    // Apaga o painel NA HORA (usado quando aperto pra encerrar a conversa -
    // pra não piscar tela nenhuma antes de dormir). O deep sleep em si vem
    // logo depois pelo Tick(), quando o áudio termina de sair.
    void ApagarTelaJa();

private:
    lv_obj_t* clock_container_ = nullptr;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;

    lv_obj_t* connecting_container_ = nullptr;
    lv_obj_t* connecting_label_ = nullptr;
    lv_obj_t* connecting_ssid_ = nullptr;

    lv_obj_t* face_container_ = nullptr;
    lv_obj_t* eyes_row_ = nullptr;
    lv_obj_t* eye_left_ = nullptr;
    lv_obj_t* eye_right_ = nullptr;
    lv_obj_t* pupil_left_ = nullptr;
    lv_obj_t* pupil_right_ = nullptr;
    lv_obj_t* mouth_ = nullptr;

    lv_timer_t* clock_timer_ = nullptr;
    bool mouth_open_ = false;

    // Piscada periódica dos olhos, só enquanto a carinha está visível.
    int ticks_until_blink_ = 15;
    bool eyes_closed_ = false;
    void UpdateBlink(bool face_visible);

    AudioCodec* audio_codec_ = nullptr;
    float mouth_level_smoothed_ = 0.0f;
    int64_t last_mouth_check_us_ = 0;

    bool screen_asleep_ = false;
    int ticks_ate_dormir_ = 0;
    bool deep_sleep_pedido_ = false;
    std::function<void()> on_enter_deep_sleep_;

    bool modo_relogio_offline_ = false;
    bool conectando_visivel_ = false;
    bool pedir_conversa_ = false;   // subiu wifi pra falar; engata ao ficar idle
    bool ja_conversou_ = false;     // teve conversa nessa "sessão acordada"
    bool desligar_pedido_ = false;  // apertei pra encerrar: tela fica apagada até dormir
    std::string ssid_;

    static void ClockTimerCallback(lv_timer_t* timer);
    void Tick();
    void UpdateClockText();
    void OnAudioLevel(float level);
    void SetMouthOpen(bool open);
};

#endif  // HU087_DISPLAY_H
