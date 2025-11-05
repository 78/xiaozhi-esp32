# Guia da placa de desenvolvimento personalizado

Este guia descreve como personalizar um novo programa de inicialização da placa de desenvolvimento para o projeto do robô de bate-papo por voz Xiaozhi AI. Xiaozhi AI suporta mais de 70 placas de desenvolvimento da série ESP32, e o código de inicialização de cada placa de desenvolvimento é colocado no diretório correspondente.

## NOTA IMPORTANTE

> **Aviso**: Para placas de desenvolvimento personalizadas, quando a configuração IO for diferente da placa de desenvolvimento original, não substitua diretamente a configuração da placa de desenvolvimento original para compilar o firmware. Um novo tipo de placa de desenvolvimento deve ser criado ou diferenciado por nomes diferentes e definições de macro sdkconfig por meio da configuração de builds no arquivo config.json. Use `python scripts/release.py [nome do diretório da placa de desenvolvimento]` para compilar e empacotar o firmware.
>
> Se você substituir diretamente a configuração original, seu firmware personalizado poderá ser substituído pelo firmware padrão da placa de desenvolvimento original durante futuras atualizações OTA, fazendo com que seu dispositivo não funcione corretamente. Cada placa de desenvolvimento possui uma identificação exclusiva e um canal de atualização de firmware correspondente. É muito importante manter a singularidade da identificação da placa de desenvolvimento.

## Estrutura de diretório

A estrutura de diretórios de cada placa de desenvolvimento geralmente contém os seguintes arquivos:

- `xxx_board.cc` - O código principal de inicialização em nível de placa, que implementa inicialização e funções relacionadas à placa
- `config.h` - arquivo de configuração em nível de placa, que define o mapeamento de pinos de hardware e outros itens de configuração
- `config.json` - Configuração de compilação, especificando o chip de destino e opções especiais de compilação
- `README.md` - documentação relacionada à placa de desenvolvimento

## Etapas do quadro de desenvolvimento personalizado

### 1. Crie um novo diretório da placa de desenvolvimento

Primeiro, crie um novo diretório no diretório `boards/`. O método de nomenclatura deve estar no formato `[nome da marca]-[tipo de placa de desenvolvimento]`, por exemplo `m5stack-tab5`:

```bash
mkdir main/boards/my-custom-board
```

### 2. Criar arquivo de configuração

#### config.h

Defina todas as configurações de hardware em `config.h`, incluindo:

- Taxa de amostragem de áudio e configuração de pinos I2S
- Endereço do chip do codec de áudio e configuração do pino I2C
- Configuração de botões e pinos de LED
- Exibir parâmetros e configuração de pinos

Exemplo de referência (de lichuang-c3-dev):

```c
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

//Configuração de áudio
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_10
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_12
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_8
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_7
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_11

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_13
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_0
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_1
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

//Configuração do botão
#define BOOT_BUTTON_GPIO        GPIO_NUM_9

//Configuração de exibição
#define DISPLAY_SPI_SCK_PIN     GPIO_NUM_3
#define DISPLAY_SPI_MOSI_PIN    GPIO_NUM_5
#define DISPLAY_DC_PIN          GPIO_NUM_6
#define DISPLAY_SPI_CS_PIN      GPIO_NUM_4

#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_2
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#endif // _BOARD_CONFIG_H_
```

#### config.json

Defina a configuração de compilação em `config.json`. Este arquivo é usado para compilação automática do script `scripts/release.py`:

```json
{
    "target": "esp32s3", // Modelo de chip alvo: esp32, esp32s3, esp32c3, esp32c6, esp32p4, etc.
    "builds": [
        {
            "name": "my-custom-board", // Nome da placa de desenvolvimento, usada para gerar pacotes de firmware
            "sdkconfig_append": [
                //Configuração especial do tamanho do Flash
                "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                //Configuração especial da tabela de partições
                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""
            ]
        }
    ]
}
```

**Descrição do item de configuração:**
- `target`: modelo do chip alvo, deve corresponder ao hardware
- `name`: O nome do pacote de firmware compilado, é recomendado que seja consistente com o nome do diretório
- `sdkconfig_append`: array adicional de itens de configuração sdkconfig, que será anexado à configuração padrão

**Configuração sdkconfig_append comumente usada:**
```json
//Tamanho do Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y"   // 4MB Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y"   // 8MB Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y"  // 16MB Flash

//Tabela de partição
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/4m.csv\"" // tabela de partições de 4 MB
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\"" // tabela de partições de 8 MB
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/16m.csv\"" // tabela de partição de 16 MB

//configuração de idioma
"CONFIG_LANGUAGE_EN_US=y" // Inglês
"CONFIG_LANGUAGE_ZH_CN=y" // Chinês simplificado

//Configuração da palavra de ativação
"CONFIG_USE_DEVICE_AEC=y" // Habilita AEC do lado do dispositivo
"CONFIG_WAKE_WORD_DISABLED=y" // Desativa palavras de ativação
```

### 3. Escreva o código de inicialização no nível da placa

Crie um arquivo `my_custom_board.cc` para implementar toda a lógica de inicialização da placa de desenvolvimento.

Uma definição básica de classe de placa de desenvolvimento contém as seguintes partes:

1. **Definição de classe**: Herdado de `WifiBoard` ou `Ml307Board`
2. **Função de inicialização**: incluindo inicialização de I2C, display, botões, IoT e outros componentes
3. **Reescrita de função virtual**: como `GetAudioCodec()`, `GetDisplay()`, `GetBacklight()`, etc.
4. **Registrar placa de desenvolvimento**: Use a macro `DECLARE_BOARD` para registrar a placa de desenvolvimento

```cpp
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#define TAG "MyCustomBoard"

class MyCustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_;

    //Inicialização I2C
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    // inicialização SPI (para exibição)
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    //Inicialização do botão
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    //Inicialização do display (tome ST7789 como exemplo)
    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        
        //Cria objeto de exibição
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // Inicialização das ferramentas MCP
    void InitializeTools() {
        // Consulte a documentação do MCP
    }

public:
    //Construtor
    MyCustomBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    //Obtém o codec de áudio
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, 
            I2C_NUM_0, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    // Obtém a tela de exibição
    virtual Display* GetDisplay() override {
        return display_;
    }
    
    // Obtém o controle da luz de fundo
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

//Registra o quadro de desenvolvimento
DECLARE_BOARD(MyCustomBoard);
```

### 4. Adicionar configuração do sistema de compilação

#### Adicione opções de placa de desenvolvimento em Kconfig.projbuild

Abra o arquivo `main/Kconfig.projbuild` e adicione um novo item de configuração da placa de desenvolvimento na seção `choice BOARD_TYPE`:

```kconfig
choice BOARD_TYPE
    prompt "Board Type"
    default BOARD_TYPE_BREAD_COMPACT_WIFI
    help
        Tipo de placa. Tipo de placa de desenvolvimento
    
    # ...outras opções de placa...
    
    config BOARD_TYPE_MY_CUSTOM_BOARD
        bool "Meu quadro personalizado (meu quadro de desenvolvimento personalizado)"
        depende de IDF_TARGET_ESP32S3 # Modifique de acordo com seu chip alvo
endchoice
```

**Observação:**
- `BOARD_TYPE_MY_CUSTOM_BOARD` é o nome do item de configuração. Ele precisa estar em letras maiúsculas e separado por sublinhados.
- `depends on` especifica o tipo de chip alvo (como `IDF_TARGET_ESP32S3`, `IDF_TARGET_ESP32C3`, etc.)
- O texto da descrição pode estar em chinês e inglês

#### Adicione a configuração da placa de desenvolvimento em CMakeLists.txt

Abra o arquivo `main/CMakeLists.txt` e adicione uma nova configuração na seção de julgamento do tipo de placa de desenvolvimento:

```cmake
# Adicione a configuração da sua placa de desenvolvimento na cadeia elseif
elseif(CONFIG_BOARD_TYPE_MY_CUSTOM_BOARD)
    set(BOARD_TYPE "my-custom-board") # Igual ao nome do diretório
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4) #Escolha a fonte apropriada de acordo com o tamanho da tela
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64) # Opcional, se a exibição de emoticons for necessária
endif()
```

**Instruções de configuração de fontes e emoticons:**

Escolha o tamanho de fonte apropriado com base na resolução da tela:
- Tela pequena (128x64 OLED): `font_puhui_basic_14_1` / `font_awesome_14_1`
- Telas pequenas e médias (240x240): `font_puhui_basic_16_4` / `font_awesome_16_4`
- Tela média (240x320): `font_puhui_basic_20_4` / `font_awesome_20_4`
- Tela grande (480x320+): `font_puhui_basic_30_4` / `font_awesome_30_4`

Opções de coleta de emoticons:
- `twemoji_32` - emoticon de 32x32 pixels (tela pequena)
- `twemoji_64` - emoji de 64x64 pixels (tela grande)

### 5. Configuração e compilação

#### Método 1: configuração manual usando idf.py

1. **Defina o chip alvo** (ao configurar ou substituir o chip pela primeira vez):
   ```bash
   # Para ESP32-S3
   idf.py set-target esp32s3
   
   # Para ESP32-C3
   idf.py set-target esp32c3
   
   #Para ESP32
   idf.py set-target esp32
   ```

2. **Limpar configuração antiga**:
   ```bash
   idf.py fullclean
   ```

3. **Entre no menu de configuração**:
   ```bash
   idf.py menuconfig
   ```
   
   Navegue até: `Xiaozhi Assistant` -> `Board Type` no menu e selecione sua placa de desenvolvimento personalizada.

4. **Compilar e gravar**:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

#### Método 2: Use o script release.py (recomendado)

Se você tiver um arquivo `config.json` no diretório da placa de desenvolvimento, você pode usar este script para concluir automaticamente a configuração e compilação:

```bash
python scripts/release.py my-custom-board
```

Este script automaticamente:
- Leia a configuração `target` em `config.json` e defina o chip alvo
- Aplicar opções de compilação em `sdkconfig_append`
- Compilar e empacotar firmware

### 6. Crie README.md

As características, requisitos de hardware, etapas de compilação e gravação da placa de desenvolvimento estão descritas em README.md:


## Componentes comuns da placa de desenvolvimento

### 1. Exibição

O projeto oferece suporte a uma variedade de drivers de vídeo, incluindo:
- ST7789 (SPI)
- ILI9341 (SPI)
- SH8601 (QSPI)
- espere...

### 2. Codec de áudio

Os codecs suportados incluem:
- ES8311 (comumente usado)
- ES7210 (conjunto de microfones)
- AW88298 (amplificador de potência)
- espere...

### 3. Gerenciamento de energia

Algumas placas de desenvolvimento usam chips de gerenciamento de energia:
- AXP2101
- Outros PMICs disponíveis

### 4. Controle do dispositivo MCP

Várias ferramentas MCP podem ser adicionadas para permitir o uso da IA:
- Alto-falante (controle de alto-falante)
- Tela (ajuste de brilho da tela)
- Bateria (leitura do nível da bateria)
- Luz (controle de luz)
- espere...

## Relacionamento de herança de classe da placa de desenvolvimento

- `Board` - classe básica de nível de conselho
  - `WifiBoard` - placa de desenvolvimento conectada por Wi-Fi
  - `Ml307Board` - Placa de desenvolvimento usando módulo 4G
  - `DualNetworkBoard` - Placa de desenvolvimento que suporta comutação de rede Wi-Fi e 4G

## Habilidades de desenvolvimento

1. **Consulte placas de desenvolvimento semelhantes**: Se sua nova placa de desenvolvimento for semelhante à placa de desenvolvimento existente, você pode consultar a implementação existente
2. **Depuração passo a passo**: primeiro implemente funções básicas (como exibição) e, em seguida, adicione funções mais complexas (como áudio)
3. **Mapeamento de pinos**: certifique-se de que todos os mapeamentos de pinos estejam configurados corretamente em config.h
4. **Verifique a compatibilidade de hardware**: Confirme a compatibilidade de todos os chips e drivers

## Possíveis problemas

1. **A exibição está anormal**: Verifique a configuração SPI, configurações de espelhamento e configurações de inversão de cores
2. **Sem saída de áudio**: Verifique a configuração I2S, o pino de habilitação do PA e o endereço do codec
3. **Não é possível conectar-se à rede**: verifique as credenciais de Wi-Fi e a configuração da rede
4. **Não é possível se comunicar com o servidor**: Verifique a configuração do MQTT ou WebSocket

## Referências

- Documentação ESP-IDF: https://docs.espressif.com/projects/esp-idf/
- Documentação LVGL: https://docs.lvgl.io/
- Documentação ESP-SR: https://github.com/espressif/esp-sr