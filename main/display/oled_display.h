#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>


class OledDisplay : public LvglDisplay {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

protected:
    // Expostos (em vez de private) pra permitir placas como a hu-087
    // esconderem/mostrarem esse bloco inteiro (ícone de emoção + texto do
    // chat, barra de status) quando querem substituir por algo próprio
    // (ex: um relógio, uma carinha desenhada na mão).
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* top_bar_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* emotion_label_ = nullptr;

public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y);
    ~OledDisplay();

    virtual void SetupUI() override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetTheme(Theme* theme) override;

    // Liga/desliga o painel físico de verdade (comando real de hardware do
    // SSD1306) - bem mais econômico de bateria do que só deixar a tela
    // preta via LVGL, porque desliga o circuito do OLED de vez.
    void SetDisplayPower(bool on) {
        if (panel_ != nullptr) {
            esp_lcd_panel_disp_on_off(panel_, on);
        }
    }

    // OLED não tem luz de fundo (o pixel se ilumina sozinho) - "brilho"
    // aqui é o registrador de contraste do SSD1306 (comando 0x81, 0-255).
    // Usado pra implementar a classe Backlight (ver hu_087_board.cc) e
    // assim ganhar de graça a ferramenta MCP self.screen.set_brightness
    // que já existe pronta pra qualquer placa com "backlight".
    void SetContrast(uint8_t valor) {
        if (panel_io_ != nullptr) {
            esp_lcd_panel_io_tx_param(panel_io_, 0x81, &valor, 1);
        }
    }
};

#endif // OLED_DISPLAY_H
