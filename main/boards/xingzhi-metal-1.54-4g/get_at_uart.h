#include "board.h"
#include "dual_network_board.h"
#include "ml307_board.h"
#include "at_modem.h"
#include "at_uart.h" 
#include <esp_log.h>
#include <memory> 

static const char *TAG = "AtUartAccessor";

// 获取AtUart指针
AtUart* GetAtUartFromMl307() {
    // 步骤1：将Board单例转为DualNetworkBoard*
    DualNetworkBoard* dual_board = dynamic_cast<DualNetworkBoard*>(&Board::GetInstance());
    if (!dual_board) {
        ESP_LOGE(TAG, "Board is not DualNetworkBoard");
        return nullptr;
    }

    // 步骤2：确认当前网络是ML307
    if (dual_board->GetNetworkType() != NetworkType::ML307) {
        ESP_LOGE(TAG, "Current network is not ML307");
        return nullptr;
    }

    // 步骤3：获取Ml307Board实例
    Board& current_board = dual_board->GetCurrentBoard();
    Ml307Board* ml307_board = dynamic_cast<Ml307Board*>(&current_board);
    if (!ml307_board) {
        ESP_LOGE(TAG, "Current board is not Ml307Board");
        return nullptr;
    }

    // 步骤4：获取AtModem*
    AtModem* modem = static_cast<AtModem*>(ml307_board->GetNetwork());
    if (!modem) {
        ESP_LOGE(TAG, "Ml307 modem is not initialized");
        return nullptr;
    }

    // 步骤5：shared_ptr<AtUart>获取裸指针
    std::shared_ptr<AtUart> at_uart_ptr = modem->GetAtUart();
    if (!at_uart_ptr) {
        ESP_LOGE(TAG, "AtUart shared_ptr is null");
        return nullptr;
    }
    AtUart* at_uart = at_uart_ptr.get();

    return at_uart;
}