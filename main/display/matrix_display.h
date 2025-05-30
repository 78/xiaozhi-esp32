#ifndef MATRIX_DISPLAY_H
#define MATRIX_DISPLAY_H

#include "display.h"

/* LED PINS */
#define UART_LED_TXD GPIO_NUM_17
#define UART_LED_RXD GPIO_NUM_18
#define UART_LED_RTS UART_PIN_NO_CHANGE
#define UART_LED_CTS UART_PIN_NO_CHANGE

#define LED_UART_PORT_NUM      UART_NUM_1
#define LED_UART_BAUD_RATE     (19200)
#define BUF_SIZE                (1024)

class MatrixDisplay : public NoDisplay {
private:
    uint8_t calculate_checksum(uint8_t *data, int len);
    void InitializeLedUart();
    void SendUartMessage(uint16_t anim_index);
public:
    MatrixDisplay();
    ~MatrixDisplay();
    virtual void SetEmotion(const char* emotion) override;
};

#endif // MATRIX_DISPLAY_H
