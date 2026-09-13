#include <driver/i2c_master.h>
#include <driver/rtc_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_sleep.h>
#include <esp_timer.h>

#include "application.h"
#include "assets/lang_config.h"
#include "boards/common/backlight.h"
#include "boards/common/wifi_board.h"
#include "button.h"
#include "ssid_manager.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "hu087_display.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "system_reset.h"

#define TAG "Hu087Board"

// O pino do amplificador (AUDIO_I2S_SPK_GPIO_CTLR) fica ligado o tempo
// todo por padrão, mesmo sem tocar nenhum som - consome bateria à toa
// (o amp tem uma correntinha de repouso mesmo "quieto"). Essa classe liga
// o pino só quando o áudio de saída é realmente ligado (fala começando) e
// desliga quando termina - aproveita o mesmo gancho (EnableOutput) que já
// existe pra ligar/desligar o canal I2S.
class Hu087AudioCodec : public NoAudioCodecSimplex {
public:
    using NoAudioCodecSimplex::NoAudioCodecSimplex;

    void EnableOutput(bool enable) override {
        NoAudioCodecSimplex::EnableOutput(enable);
        gpio_set_level(AUDIO_I2S_SPK_GPIO_CTLR, enable ? 1 : 0);
    }
};

// OLED não tem luz de fundo de verdade (o pixel se ilumina sozinho), mas
// o SSD1306 tem um registrador de "contraste" que dá o mesmo efeito de
// deixar mais forte/fraco. Encaixando isso na classe Backlight (a mesma
// que placas com LCD/luz de fundo usam), a Sofia ganha de graça a
// ferramenta MCP self.screen.set_brightness já pronta (ver mcp_server.cc -
// ela só registra essa ferramenta se GetBacklight() não for nulo).
class Ssd1306ContrastBacklight : public Backlight {
public:
    explicit Ssd1306ContrastBacklight(Hu087Display* display) : display_(display) {}

    void SetBrightnessImpl(uint8_t brightness) override {
        // brightness chega de 0 a 100 (%) - o registrador do SSD1306 é de
        // 0 a 255.
        uint8_t contraste = static_cast<uint8_t>((static_cast<int>(brightness) * 255) / 100);
        display_->SetContrast(contraste);
    }

private:
    Hu087Display* display_;
};

class Hu087Board : public WifiBoard {
    private:
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Backlight* backlight_ = nullptr;
    Button touch_button_;
    bool time_sync_started_ = false;
    // Este boot foi um wake do nosso deep sleep (botão), e quando começou.
    bool woke_from_deep_sleep_ = false;
    int64_t boot_time_us_ = 0;
    // No modo "relógio offline" (acordou do botão, wifi desligado): o próximo
    // clique deixa de ser "abrir a IA" e passa a ser "agora sim liga o wifi
    // e conecta no xiaozhi". pending_talk_ = já mandaram falar, esperando a
    // rede subir pra entrar em conversa.
    bool modo_relogio_offline_ = false;
    bool pending_talk_ = false;

    // Pega hora/data certas da internet (SNTP) assim que conecta no wifi -
    // sem isso o relógio não tem como saber que horas são de verdade.
    // Fuso fixo Brasil (UTC-3, sem horário de verão - já foi abolido).
    void InitializeTimeSync() {
        if (time_sync_started_) {
            return;
        }
        time_sync_started_ = true;

        setenv("TZ", "<-03>3", 1);
        tzset();

        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        esp_netif_sntp_init(&config);
    }

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags =
                {
                    .dc_low_on_data = 0,
                    .disable_control_phase = 0,
                },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(
            esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(
            esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        auto hu087_display = new Hu087Display(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                              DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        hu087_display->SetAudioCodecForSync(GetAudioCodec());
        backlight_ = new Ssd1306ContrastBacklight(hu087_display);
        // Quando o display decidir que é hora de dormir (1 min parado, tela
        // já apagada, Application confirmou que dá), roda o desligamento no
        // task principal - não no meio do tick do LVGL.
        hu087_display->OnEnterDeepSleep([this]() {
            Application::GetInstance().Schedule([this]() { EnterDeepSleep(); });
        });
        hu087_display->SetModoRelogioOffline(modo_relogio_offline_);
        {
            auto& lista = SsidManager::GetInstance().GetSsidList();
            if (!lista.empty()) {
                hu087_display->SetSsid(lista.front().ssid);
            }
        }
        display_ = hu087_display;
    }

    // Chamado quando o relógio fica 1 minuto parado (tela já apagada): desliga
    // tudo de verdade. Deep sleep do ESP32-S3 = ~10µA (rádio, CPU, RAM, tudo
    // off). O único jeito de "não consumir nada" com a tela apagada e ainda
    // acordar 100% das vezes no botão - porque acordar aqui é o chip
    // reiniciando limpo (impossível travar tentando reconectar).
    void EnterDeepSleep() {
        ESP_LOGI(TAG, "Entrando em deep sleep - economia maxima");

        // 1) Alto-falante 100% desligado. Corta a saída de áudio e TRAVA o
        //    pino do amplificador (GPIO17) em LOW pro sono inteiro. GPIO17 é
        //    RTC-capaz, então usa rtc_gpio_hold (trava de verdade no deep
        //    sleep - o gpio_hold comum às vezes não segura pino RTC). Sem
        //    isso o amp flutua e fica puxando corrente parado (mA).
        auto codec = GetAudioCodec();
        if (codec != nullptr) {
            codec->EnableOutput(false);
        }
        gpio_set_level(AUDIO_I2S_SPK_GPIO_CTLR, 0);
        rtc_gpio_hold_en(AUDIO_I2S_SPK_GPIO_CTLR);
        gpio_deep_sleep_hold_en();

        // 2) Painel OLED já foi apagado pelo display; nada a fazer aqui.

        // 3) Botão (GPIO18, ativo em LOW) como fonte de despertar. ext1 usa
        //    o controlador RTC e funciona mesmo com todo o resto desligado
        //    (~10µA). PRECISA ligar o pull-up do RTC: no deep sleep o
        //    pull-up interno do driver do botão some e o pino ficaria
        //    solto/baixo - aí o "acorda em LOW" dispara na hora e ele nunca
        //    dorme de verdade (religa sozinho). Com o pull-up do RTC o pino
        //    fica ALTO parado, e só vai a LOW quando o botão é apertado.
        rtc_gpio_init(TOUCH_BUTTON_GPIO);
        rtc_gpio_set_direction(TOUCH_BUTTON_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pulldown_dis(TOUCH_BUTTON_GPIO);
        rtc_gpio_pullup_en(TOUCH_BUTTON_GPIO);
        // Mantém o domínio RTC ligado no sono - senão o pull-up acima morre
        // junto e o pino volta a flutuar (o custo disso é uns poucos µA, não
        // muda a conta de "dura semanas").
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        rtc_gpio_hold_en(TOUCH_BUTTON_GPIO);  // congela o pull-up no sono

        esp_sleep_enable_ext1_wakeup(1ULL << TOUCH_BUTTON_GPIO, ESP_EXT1_WAKEUP_ANY_LOW);

        vTaskDelay(pdMS_TO_TICKS(150));
        esp_deep_sleep_start();  // não retorna - o próximo "boot" é o wake
    }

    void initializeAmpCtrl() {
        // (a trava de deep sleep do pino já foi liberada no início do
        // construtor)
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << AUDIO_I2S_SPK_GPIO_CTLR),  // Select GPIO 2
            .mode = GPIO_MODE_OUTPUT,                           // Set as output
            .pull_up_en = GPIO_PULLUP_ENABLE,                   // Disable pull-up
            .pull_down_en = GPIO_PULLDOWN_DISABLE,              // Disable pull-down
            .intr_type = GPIO_INTR_DISABLE  // Disable interrupts
        };
        gpio_config(&io_conf);
        // Começa desligado - o Hu087AudioCodec liga sozinho na hora que
        // tiver áudio de verdade pra tocar (ver EnableOutput acima).
        gpio_set_level(AUDIO_I2S_SPK_GPIO_CTLR, 0);
    };

    void InitializeButtons() {
        touch_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            auto hu087_display = dynamic_cast<Hu087Display*>(display_);

            // Engole o toque nos primeiros 1,5s de um wake por botão - dedo
            // que demorou a soltar não pode disparar nada.
            if (woke_from_deep_sleep_ &&
                esp_timer_get_time() - boot_time_us_ < 1500000) {
                return;
            }

            // Tela "Conectando..." na frente (subindo wifi - no boot do
            // interruptor ou porque pedi pra falar): um toque aqui entra no
            // modo de configurar outra rede wifi.
            if (hu087_display != nullptr && hu087_display->EstaConectando()) {
                EnterWifiConfigMode();
                return;
            }

            // Boot no interruptor ainda em "Starting" sem a tela de
            // conectando montada ainda: também vai pro modo de config.
            if (app.GetDeviceState() == kDeviceStateStarting && !modo_relogio_offline_) {
                EnterWifiConfigMode();
                return;
            }

            // Tela apagou sozinha por um instante antes de dormir: só acorda
            // ela de volta.
            if (hu087_display != nullptr && hu087_display->IsScreenAsleep()) {
                hu087_display->WakeScreen();
                return;
            }

            // Relógio offline (acordou do botão, wifi desligado): o toque
            // sobe o wifi e engata a conversa quando estiver pronto.
            if (modo_relogio_offline_ && !pending_talk_) {
                LigarWifiEConversar();
                return;
            }

            // Apertou ouvindo (ou num aviso): é o gesto de "desligar" -
            // encerra a chamada de vez. Apaga o painel NA HORA (sem piscar
            // "Em espera" nem relógio); o Tick desliga tudo quando o áudio
            // acabar de sair.
            auto st = app.GetDeviceState();
            if (hu087_display != nullptr &&
                (st == kDeviceStateListening || st == kDeviceStateNotifying)) {
                hu087_display->ApagarTelaJa();
                app.ToggleChatState();
                return;
            }

            // Apertou enquanto ela está falando: só interrompe a fala (o
            // xiaozhi já volta a ouvir sozinho em seguida, sem precisar de
            // outro toque) - não mexe na tela, continua mostrando o rosto.
            if (st == kDeviceStateSpeaking) {
                app.ToggleChatState();
                return;
            }

            // Já online: liga/desliga a conversa normalmente.
            app.ToggleChatState();
        });

        touch_button_.OnLongPress([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }  // Need to implement logic to lower volume
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME +
                                           std::to_string(volume));
        });
    }

 public:
    Hu087Board() : touch_button_(TOUCH_BUTTON_GPIO) {
        boot_time_us_ = esp_timer_get_time();
        woke_from_deep_sleep_ =
            (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);
        modo_relogio_offline_ = woke_from_deep_sleep_;

        // Libera qualquer trava de pino que sobrou do deep sleep ANTES de
        // configurar botão/amp de novo.
        rtc_gpio_hold_dis(AUDIO_I2S_SPK_GPIO_CTLR);
        rtc_gpio_hold_dis(TOUCH_BUTTON_GPIO);
        gpio_deep_sleep_hold_dis();

        // Fuso do Brasil (UTC-3) fixado já no boot - assim o relógio mostra
        // a hora certa vinda do RTC mesmo quando acorda do deep sleep sem
        // nunca ligar o wifi. (Antes isso só era feito quando conectava.)
        setenv("TZ", "<-03>3", 1);
        tzset();

        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        initializeAmpCtrl();  // Could control the amp ctrl pin throught voice
                              // detection i guess

        SetNetworkEventCallback([this](NetworkEvent event, const std::string& data) {
            if (event == NetworkEvent::Connected) {
                InitializeTimeSync();
            }
        });
    }

    // Quando acorda do deep sleep pelo botão, NÃO sobe o wifi: mostra o
    // relógio na hora (a hora vem do RTC, que continua contando dormindo).
    // O wifi só sobe se a pessoa apertar de novo pra falar com o xiaozhi
    // (aí pending_talk_ = true e deixa o fluxo normal seguir).
    void StartNetwork() override {
        if (modo_relogio_offline_ && !pending_talk_) {
            ESP_LOGI(TAG, "Wake do botao: relogio offline, wifi fica desligado");
            return;
        }
        WifiBoard::StartNetwork();
    }

    // Chamado pelo botão quando está no relógio offline: agora sim liga o
    // wifi e engata a conversa assim que estiver pronto.
    void LigarWifiEConversar() {
        if (pending_talk_) {
            return;
        }
        ESP_LOGI(TAG, "Botao no relogio offline: subindo wifi pra falar");
        pending_talk_ = true;
        auto hu087_display = dynamic_cast<Hu087Display*>(display_);
        if (hu087_display != nullptr) {
            hu087_display->MostrarConectando();
        }
        WifiBoard::StartNetwork();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Hu087AudioCodec audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_RIGHT, AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_RIGHT);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }
    virtual Backlight* GetBacklight() override { return backlight_; }
};

DECLARE_BOARD(Hu087Board);
