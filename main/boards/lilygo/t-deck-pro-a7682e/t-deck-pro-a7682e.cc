#include "a7682e_audio_codec.h"
#include "a7682e_epd_display.h"
#include "a7682e_modem.h"
#include "a7682e_protocol_handler.h"
#include "config.h"
#include "display/display.h"
#include "wifi_board.h"

class TDeckProA7682eBoard : public WifiBoard {
public:
    TDeckProA7682eBoard() = default;

    AudioCodec* GetAudioCodec() override {
        static A7682eAudioCodec codec(modem_);
        return &codec;
    }

    BoardProtocolHandler* GetProtocolHandler() override {
        static A7682eProtocolHandler handler(modem_);
        return &handler;
    }

    Display* GetDisplay() override {
        static A7682eEpdDisplay display;
        return &display;
    }

private:
    A7682eModem modem_;
};

DECLARE_BOARD(TDeckProA7682eBoard);
