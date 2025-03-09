#include "hna_16mm65t.h"
#include "driver/usb_serial_jtag.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>
#include <esp_log.h>

// Define the log tag
#define TAG "HNA_16MM65T"

/**
 * @brief Performs the wave animation process.
 *
 * Calculates the elapsed time, resets wave data if necessary, updates the current value of each wave point
 * based on the animation progress, and calls helper functions to update the display.
 * Also calculates the sum of left and right wave values and calls the core wave helper function.
 */
void HNA_16MM65T::waveanimate()
{
    int left_sum = 0, right_sum = 0;
    int64_t current_time = esp_timer_get_time() / 1000;
    int64_t elapsed_time = current_time - wave_start_time;

    if (elapsed_time >= 220)
    {
        wave_start_time = current_time;
        for (size_t i = 0; i < FFT_SIZE; i++)
        {
            waveData[i].last_value = waveData[i].target_value;
            waveData[i].target_value = 0;
            waveData[i].animation_step = 0;
        }
    }
    for (int i = 0; i < FFT_SIZE; i++)
    {
        if (waveData[i].animation_step < wave_total_steps)
        {
            float progress = static_cast<float>(waveData[i].animation_step) / wave_total_steps;
            float factor = 1 - std::exp(-3 * progress);
            waveData[i].current_value = waveData[i].last_value + static_cast<int>((waveData[i].target_value - waveData[i].last_value) * factor);
            wavehelper(i, waveData[i].current_value * 8 / 90);
            waveData[i].animation_step++;
        }
        else
        {
            waveData[i].last_value = waveData[i].target_value;
            wavehelper(i, waveData[i].target_value * 8 / 90);
        }
        if (i < 6)
            left_sum += waveData[i].current_value;
        else
            right_sum += waveData[i].current_value;
    }
    corewavehelper(left_sum * 8 / 90 / 4, right_sum * 8 / 90 / 4);
}

/**
 * @brief Gets a part of the content by combining raw and previous raw data according to a mask.
 *
 * Combines the raw data and the previous raw data based on the given mask,
 * where the bits in the mask determine which parts come from the raw data and which come from the previous raw data.
 *
 * @param raw The current raw data.
 * @param before_raw The previous raw data.
 * @param mask The mask used to determine the combination method.
 * @return The combined data.
 */
uint32_t HNA_16MM65T::contentgetpart(uint32_t raw, uint32_t before_raw, uint32_t mask)
{
    return (raw & mask) | (before_raw & (~mask));
}

/**
 * @brief Performs the content animation process.
 *
 * Checks the elapsed time, updates the content data if the inhibition time has passed.
 * For each content item with a different current and last content,
 * it selects the appropriate animation step based on the animation type and updates the display.
 */
void HNA_16MM65T::contentanimate()
{
    static int64_t start_time = esp_timer_get_time() / 1000;
    int64_t current_time = esp_timer_get_time() / 1000;
    int64_t elapsed_time = current_time - start_time;

    if (elapsed_time >= 30)
        start_time = current_time;
    else
        return;

    if (content_inhibit_time != 0)
    {
        elapsed_time = current_time - content_inhibit_time;
        if (elapsed_time > 0)
        {
            for (size_t i = 0; i < CONTENT_SIZE; i++)
            {
                currentData[i].last_content = currentData[i].current_content;
                currentData[i].animation_type = tempData[i].animation_type;
                currentData[i].current_content = tempData[i].current_content;
            }
            content_inhibit_time = 0;
        }
    }

    for (int i = 0; i < CONTENT_SIZE; i++)
    {
        if (currentData[i].current_content != currentData[i].last_content)
        {
            uint32_t before_raw_code = find_hex_code(currentData[i].last_content);
            uint32_t raw_code = find_hex_code(currentData[i].current_content);
            uint32_t code = raw_code;
            if (currentData[i].animation_type == HNA_CLOCKWISE)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x080000 | 0x800000);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x4C0000 | 0x800000);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x6e0000 | 0x800000);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6000 | 0x800000);
                    break;
                case 4:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6300 | 0x800000);
                    break;
                case 5:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6770 | 0x800000);
                    break;
                case 6:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6ff0 | 0x800000);
                    break;
                case 7:
                    code = contentgetpart(raw_code, before_raw_code, 0x6ffff0 | 0x800000);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_ANTICLOCKWISE)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x004880);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x004ca0);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x004ef0);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x006ff0);
                    break;
                case 4:
                    code = contentgetpart(raw_code, before_raw_code, 0x036ff0);
                    break;
                case 5:
                    code = contentgetpart(raw_code, before_raw_code, 0x676ff0);
                    break;
                case 6:
                    code = contentgetpart(raw_code, before_raw_code, 0xef6ff0);
                    break;
                case 7:
                    code = contentgetpart(raw_code, before_raw_code, 0xffeff0);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_UP2DOWN)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0xe00000);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0xff0000);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0xffe000);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0xffff00);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_DOWN2UP)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x0000f0);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x001ff0);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x00fff0);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x1ffff0);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_LEFT2RT)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x901080);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0xd89880);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0xdcdce0);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0xdefee0);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_RT2LEFT)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x210110);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x632310);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x676770);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x6fef70);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else
                currentData[i].animation_step = -1;

            if (currentData[i].animation_step == -1)
                currentData[i].last_content = currentData[i].current_content;

            charhelper(i, code);
            currentData[i].animation_step++;
        }
    }
}

/**
 * @brief Constructor for the PT6324Writer class.
 *
 * Initializes the PT6324Writer object with the specified GPIO pins and SPI host device.
 *
 * @param din The GPIO pin number for the data input line.
 * @param clk The GPIO pin number for the clock line.
 * @param cs The GPIO pin number for the chip select line.
 * @param spi_num The SPI host device number to use for communication.
 */
HNA_16MM65T::HNA_16MM65T(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num) : PT6324Writer(din, clk, cs, spi_num)
{
    pt6324_init();
    xTaskCreate(
        [](void *arg)
        {
            HNA_16MM65T *vfd = static_cast<HNA_16MM65T *>(arg);
            vfd->symbolhelper(LBAR_RBAR, true);
            while (true)
            {
                vfd->pt6324_refrash(vfd->gram);
                vfd->contentanimate();
                vfd->waveanimate();
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            vTaskDelete(NULL);
        },
        "vfd",
        4096 - 1024,
        this,
        6,
        nullptr);
}

/**
 * @brief Constructor of the HNA_16MM65T class.
 *
 * Initializes the PT6324 device and creates a task to refresh the display and perform animations.
 *
 * @param spi_device The SPI device handle used to communicate with the PT6324.
 */
HNA_16MM65T::HNA_16MM65T(spi_device_handle_t spi_device) : PT6324Writer(spi_device)
{
    if (!spi_device)
    {
        ESP_LOGE(TAG, "VFD spi is null");
        return;
    }
    pt6324_init();
    xTaskCreate(
        [](void *arg)
        {
            HNA_16MM65T *vfd = static_cast<HNA_16MM65T *>(arg);
            vfd->symbolhelper(LBAR_RBAR, true);
            while (true)
            {
                vfd->pt6324_refrash(vfd->gram);
                vfd->contentanimate();
                vfd->waveanimate();
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            vTaskDelete(NULL);
        },
        "vfd",
        4096 - 1024,
        this,
        6,
        nullptr);
}

/**
 * @brief Displays spectrum information.
 *
 * Processes the incoming spectrum data, calculates the average value of each frequency band, and applies gain.
 * Updates the target values and animation steps for subsequent animation effects.
 *
 * @param buf The spectrum data buffer containing the amplitude values of each frequency band.
 * @param size The size of the buffer, i.e., the number of spectrum data points.
 */
void HNA_16MM65T::spectrum_show(float *buf, int size)
{
    wave_start_time = esp_timer_get_time() / 1000;
    if (size < 512)
        return;
    static float fft_gain[FFT_SIZE] = {1.5f * 2, 1.6f * 2, 2.6f * 2, 2.8f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2};
    static uint8_t fft_postion[FFT_SIZE] = {0, 2, 4, 6, 8, 10, 11, 9, 7, 5, 3, 1};
    static float max = 0;
    float fft_buf[FFT_SIZE];
    int sum = 0;
    int elements_per_part = size / 4 / 12;
    for (int i = 0; i < FFT_SIZE; i++)
    {
        int max_val = 0;
        for (int j = 0; j < elements_per_part; j++)
        {
            if (max_val < buf[(i + 3) * elements_per_part + j])
                max_val = buf[(i + 3) * elements_per_part + j];
        }
        sum += max_val;
        fft_buf[i] = max_val;
        if (max < fft_buf[i])
        {
            max = fft_buf[i];
        }
        if (fft_buf[i] < 0)
            fft_buf[i] = -fft_buf[i];
    }
    wavebusy = false;
    for (size_t i = 0; i < FFT_SIZE; i++)
    {
        waveData[i].last_value = waveData[i].target_value;
        waveData[i].target_value = fft_buf[fft_postion[i]] * fft_gain[fft_postion[i]] * 0.25f;
        waveData[i].animation_step = 0;
    }
}

/**
 * @brief Controls the blinking effect related to time and toggles the display state of specific HNA_Symbols.
 *
 * If there is a content inhibition time, the function returns directly. Otherwise, it toggles the time mark state
 * and updates the display of corresponding HNA_Symbols according to the mark state.
 */
void HNA_16MM65T::time_blink()
{
    static bool time_mark = true;
    time_mark = !time_mark;
    if (content_inhibit_time != 0)
    {
        symbolhelper(NUM6_MARK, false);
        symbolhelper(NUM8_MARK, false);
        return;
    }
    symbolhelper(NUM6_MARK, time_mark);
    symbolhelper(NUM8_MARK, time_mark);
}

/**
 * @brief Displays content and sets the content and its animation type at the specified starting position.
 *
 * If there is a content inhibition time, the content is stored in the temporary data. Otherwise,
 * the content is stored in the current data, and the corresponding animation type is set.
 *
 * @param start The starting position index.
 * @param buf The character array to be displayed.
 * @param size The number of characters to be displayed.
 * @param ani The animation type enumeration value.
 */
void HNA_16MM65T::content_show(int start, char *buf, int size, HNA_NumAni ani)
{
    if (content_inhibit_time != 0)
    {
        for (size_t i = 0; i < size && (start + i) < CONTENT_SIZE; i++)
        {
            tempData[start + i].animation_type = ani;
            tempData[start + i].current_content = buf[i];
        }
        return;
    }
    for (size_t i = 0; i < size && (start + i) < CONTENT_SIZE; i++)
    {
        currentData[start + i].animation_type = ani;
        currentData[start + i].current_content = buf[i];
    }
}

/**
 * @brief Displays notification content and sets the content, animation type, and inhibition time at the specified starting position.
 *
 * Sets the content inhibition time, stores the content in the current data, and sets the corresponding animation type.
 *
 * @param start The starting position index.
 * @param buf The character array to be displayed.
 * @param size The number of characters to be displayed.
 * @param ani The animation type enumeration value.
 * @param timeout The content inhibition time (in milliseconds).
 */
void HNA_16MM65T::noti_show(int start, char *buf, int size, HNA_NumAni ani, int timeout)
{
    content_inhibit_time = esp_timer_get_time() / 1000 + timeout;
    for (size_t i = 0; i < size && (start + i) < CONTENT_SIZE; i++)
    {
        currentData[start + i].animation_type = ani;
        currentData[start + i].current_content = buf[i];
    }
}

/**
 * @brief Test function that creates a task to simulate spectrum data display.
 *
 * Creates a task that randomly generates spectrum data and calls the spectrum_show function to display it.
 * It can also be used to test the digital display and dot matrix display.
 */
void HNA_16MM65T::test()
{
    wavebusy = false;
    xTaskCreate(
        [](void *arg)
        {
            HNA_16MM65T *vfd = static_cast<HNA_16MM65T *>(arg);
            int rollcounter = 0;
            HNA_NumAni num_ani = HNA_ANTICLOCKWISE;
            char tempstr[CONTENT_SIZE];
            int64_t start_time = esp_timer_get_time() / 1000;
            while (1)
            {
                int64_t current_time = esp_timer_get_time() / 1000;
                int64_t elapsed_time = current_time - start_time;

                if (elapsed_time >= 5000)
                {
                    num_ani = (HNA_NumAni)((int)(num_ani + 1) % HNA_MAX);
                    start_time = current_time;
                }

                snprintf(tempstr, CONTENT_SIZE, "ABC%dDEF", (rollcounter++) % 100);
                vfd->content_show(0, tempstr, CONTENT_SIZE, num_ani);

                // for (int i = 0; i < FFT_SIZE; i++)
                //     testbuff[i] = rand() % 100;
                // vfd->spectrum_show(testbuff, FFT_SIZE);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            vTaskDelete(NULL);
        },
        "vfd_test",
        4096 - 1024,
        this,
        5,
        nullptr);
}

/**
 * @brief Calibration function that configures the USB SERIAL JTAG and processes received data.
 *
 * Configures the USB SERIAL JTAG driver and allocates a buffer for receiving data.
 * Reads the received data in a loop, parses the data, and updates the display buffer.
 */
void HNA_16MM65T::cali()
{
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .tx_buffer_size = BUF_SIZE,
        .rx_buffer_size = BUF_SIZE,
    };
    wavebusy = false;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    uint8_t *recv_data = (uint8_t *)malloc(BUF_SIZE);
    while (1)
    {
        memset(recv_data, 0, BUF_SIZE);
        int len = usb_serial_jtag_read_bytes(recv_data, BUF_SIZE - 1, 0x20 / portTICK_PERIOD_MS);
        if (len > 0)
        {
            int index = 0, data = 0;
            sscanf((char *)recv_data, "%d:%X", &index, &data);
            printf("Parsed contents: %d and 0x%02X\n", index, data);
            gram[index] = data;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Finds the corresponding hexadecimal code for a given character.
 *
 * Searches for the given character in the character array and returns its corresponding hexadecimal code.
 *
 * @param ch The character to be searched for.
 * @return The hexadecimal code corresponding to the character. Returns 0 if not found.
 */
unsigned int HNA_16MM65T::find_hex_code(char ch)
{
    if (ch >= ' ' && ch <= 'Z')
        return hex_codes[ch - ' '];
    else if (ch >= 'a' && ch <= 'z')
        return hex_codes[ch - 'a' + 'A' - ' '];
    return 0;
}

/**
 * @brief Displays a numeric character (based on a character).
 *
 * Finds the corresponding hexadecimal code based on the given index and character, and updates the display buffer.
 *
 * @param index The index position of the numeric display.
 * @param ch The character to be displayed.
 */
void HNA_16MM65T::charhelper(int index, char ch)
{
    if (index >= 10)
        return;
    uint32_t val = find_hex_code(ch);
    charhelper(index, val);
}

/**
 * @brief Displays a numeric character (based on a hexadecimal code).
 *
 * Updates the display buffer based on the given index and hexadecimal code.
 *
 * @param index The index position of the numeric display.
 * @param code The hexadecimal code to be displayed.
 */
void HNA_16MM65T::charhelper(int index, uint32_t code)
{
    if (index >= 10)
        return;
    gram[NUM_BEGIN + index * 3 + 2] = code >> 16;
    gram[NUM_BEGIN + index * 3 + 1] = code >> 8;
    gram[NUM_BEGIN + index * 3 + 0] = code & 0xff;
}

/**
 * @brief Finds the position of a symbol in the display buffer based on its enumeration value.
 *
 * Receives a symbol enumeration value, looks up the corresponding byte index and bit index in the `symbolPositions` array,
 * and stores them in the variables pointed to by the input pointers.
 *
 * @param flag The symbol enumeration value used to identify the symbol to be found.
 * @param byteIndex A pointer to an integer used to store the byte index of the symbol.
 * @param bitIndex A pointer to an integer used to store the bit index of the symbol.
 */
void HNA_16MM65T::find_enum_code(HNA_Symbols flag, int *byteIndex, int *bitIndex)
{
    *byteIndex = symbolPositions[flag].byteIndex;
    *bitIndex = symbolPositions[flag].bitIndex;
}

/**
 * @brief Controls the display state of a specific symbol.
 *
 * Finds the position of the symbol in the display buffer based on the input symbol enumeration value and display state flag,
 * and sets or clears the corresponding bit to control the display or hiding of the symbol.
 *
 * @param symbol The symbol enumeration value of the symbol to be controlled.
 * @param is_on A boolean value indicating whether the symbol should be displayed (true for display, false for hiding).
 */
void HNA_16MM65T::symbolhelper(HNA_Symbols symbol, bool is_on)
{
    if (symbol >= HNA_SYMBOL_MAX)
        return;

    int byteIndex, bitIndex;
    find_enum_code(symbol, &byteIndex, &bitIndex);

    if (is_on)
        gram[byteIndex] |= bitIndex;
    else
        gram[byteIndex] &= ~bitIndex;
}

/**
 * @brief Updates the display buffer according to different dot matrix states.
 *
 * Performs operations on specific bytes in the display buffer based on the input dot matrix state enumeration value
 * to achieve different dot matrix display effects. Before the operation, specific bits in the relevant bytes are cleared.
 *
 * @param dot The dot matrix state enumeration value indicating the dot matrix style to be displayed.
 */
void HNA_16MM65T::dotshelper(Dots dot)
{
    gram[1] &= ~0xF8;
    gram[2] &= ~0xF;

    switch (dot)
    {
    case DOT_MATRIX_UP:
        gram[1] |= 0x78;
        break;
    case DOT_MATRIX_NEXT:
        gram[1] |= 0xD0;
        gram[2] |= 0xA;
        break;
    case DOT_MATRIX_PAUSE:
        gram[1] |= 0xB2;
        gram[2] |= 0x1;
        break;
    case DOT_MATRIX_FILL:
        gram[1] |= 0xF8;
        gram[2] |= 0x7;
        break;
    }
}

/**
 * @brief Updates the waveform display based on the index and level.
 *
 * Updates the corresponding waveform display in the display buffer based on the input index and level.
 * Checks if the index and level are within the valid range, and then sets or clears the corresponding bits in the display buffer according to the level.
 *
 * @param index The index of the waveform used to determine the starting position in the display buffer.
 * @param level The level of the waveform indicating the height of the waveform, usually in the range of 0 to 8.
 */
void HNA_16MM65T::wavehelper(int index, int level)
{
    static HNA_SymbolPosition wavePositions[] = {
        {33, 0x10},
        {33, 8},
        {33, 4},
        {36, 0x10},
        {36, 8},
        {36, 4},
        {42, 4},
        {42, 8},
        {42, 0x10},
        {45, 4},
        {45, 8},
        {45, 0x10},
    };

    if (index >= 12)
        return;
    if (level > 8)
        level = 8;

    int byteIndex = wavePositions[index].byteIndex, bitIndex = wavePositions[index].bitIndex;

    if (!wavebusy)
        gram[byteIndex + 2] |= 0x80;

    for (size_t i = 0; i < 7; i++)
    {
        if ((i) >= (8 - level) && level > 1)
            gram[byteIndex] |= bitIndex;
        else
            gram[byteIndex] &= ~bitIndex;

        bitIndex <<= 3;
        if (bitIndex > 0xFF)
        {
            bitIndex >>= 8;
            byteIndex++;
        }
    }
}

/**
 * @brief Updates the core waveform display based on the left and right levels.
 *
 * Calculates and updates the display of the core waveform according to the input left and right levels.
 * Applies animation effects and updates the corresponding bits in the display buffer.
 *
 * @param l_level The level of the left waveform, typically in the range of 0 - 100.
 * @param r_level The level of the right waveform, typically in the range of 0 - 100.
 */
void HNA_16MM65T::corewavehelper(int l_level, int r_level)
{
    static int rollcount = 0;
    static int64_t start_time = esp_timer_get_time() / 1000;
    int64_t current_time = esp_timer_get_time() / 1000;

    int64_t elapsed_time = current_time - start_time;

    if (elapsed_time >= 30)
        start_time = current_time;
    else
        return;

    gram[0] &= 0x80;
    gram[COREWAVE_BEGIN] = gram[COREWAVE_BEGIN + 1] = gram[COREWAVE_BEGIN + 2] = 0;
    if (wavebusy)
    {
        uint16_t core_level = (((1 << 3) - 1) << 8) | ((1 << 3) - 1);
        core_level = (core_level << rollcount) | (core_level >> (8 - rollcount));
        rollcount = (rollcount + 1) % 8;
        gram[COREWAVE_BEGIN + 1] = core_level >> 8;
        gram[COREWAVE_BEGIN + 1 + 1] = core_level & 0xFF;
        return;
    }
    if (l_level > 8)
        l_level = 8;
    if (r_level > 8)
        r_level = 8;
    uint16_t core_level = (((1 << l_level) - 1) << 6) | ((1 << r_level) - 1);

    core_level = (core_level << rollcount) | (core_level >> (6 - rollcount));
    rollcount = (rollcount + 1) % 8;

    if (l_level > 1)
        gram[0] |= 0x40;
    if (l_level > 3)
        gram[0] |= 0x20;
    if (l_level > 5)
        gram[0] |= 0x10;

    if (r_level > 1)
        gram[0] |= 0x8;
    if (r_level > 3)
        gram[0] |= 0x4;
    if (r_level > 5)
        gram[0] |= 0x2;

    if (l_level > 3 || r_level > 3)
    {
        gram[COREWAVE_BEGIN + 1] = core_level >> 8;
        gram[COREWAVE_BEGIN + 1 + 1] = core_level & 0x3F;
    }

    gram[COREWAVE_BEGIN + 1 + 1] |= 0x80;

    if (l_level > 2 || r_level > 2)
    {
        gram[COREWAVE_BEGIN + 1 + 1] |= 0x40;
    }

    if (l_level > 4)
    {
        gram[COREWAVE_BEGIN] |= 0x40;
    }
    if (r_level > 4)
    {
        gram[COREWAVE_BEGIN] |= 0x10;
    }
    if (l_level > 5)
    {
        gram[COREWAVE_BEGIN] |= 0x20;
    }
    if (r_level > 5)
    {
        gram[COREWAVE_BEGIN] |= 0x80;
    }
    if (l_level > 6)
    {
        gram[COREWAVE_BEGIN] |= 0x4;
    }
    if (r_level > 6)
    {
        gram[COREWAVE_BEGIN] |= 0x8;
    }
    if (l_level > 7)
    {
        gram[COREWAVE_BEGIN] |= 0x1;
    }
    if (r_level > 7)
    {
        gram[COREWAVE_BEGIN] |= 0x2;
    }
}