#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>

#include "Application.h"
#include "SystemInfo.h"
#include "SystemReset.h"
#include "lvgl.h"
#include "lv_gui.h"
#include "esp_lcd_touch_cst816s.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ops.h"
#include "esp_io_expander_tca9554.h"
#include "lvgl_helpers.h"
#include "esp_freertos_hooks.h"
#include "avi_player.h"
#include "lv_demos.h"
#include "usbh_modem_board.h"
#include "esp_netif.h"
#include "file_manager.h"
#define TAG "main"

#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A (ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000)
#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554 (ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)

#define I2C_SCL_IO (GPIO_NUM_18)
#define I2C_SDA_IO (GPIO_NUM_17)
esp_err_t tfcard_ret = ESP_FAIL;
static void lv_tick_task(void *arg)
{
    (void)arg;
    lv_tick_inc(10);
}
SemaphoreHandle_t xGuiSemaphore;

static void gui_task(void *arg)
{
    xGuiSemaphore = xSemaphoreCreateMutex();
    lv_init(); // lvgl内核初始化

    lvgl_driver_init(); // lvgl显示接口初始化

    /* Example for 1) */
    static lv_disp_draw_buf_t draw_buf;

    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

    // lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DLV_HOR_RES_MAX * DLV_VER_RES_MAX); /*Initialize the display buffer*/

    static lv_disp_drv_t disp_drv;         /*A variable to hold the drivers. Must be static or global.*/
    lv_disp_drv_init(&disp_drv);           /*Basic initialization*/
    disp_drv.draw_buf = &draw_buf;         /*Set an initialized buffer*/
    disp_drv.flush_cb = disp_driver_flush; /*Set a flush callback to draw to the display*/
    disp_drv.hor_res = 280;                /*Set the horizontal resolution in pixels*/
    disp_drv.ver_res = 240;                /*Set the vertical resolution in pixels*/
    lv_disp_drv_register(&disp_drv);       /*Register the driver and save the created display objects*/
                                           /*触摸屏输入接口配置*/
    touch_driver_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);

    // esp_register_freertos_tick_hook(lv_tick_task);

    /* 创建一个定时器中断来进入 lv_tick_inc 给lvgl运行提供心跳 这里是10ms一次 主要是动画运行要用到 */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000));

    lv_main_page();
    if (tfcard_ret == ESP_OK)
        avi_player_load();

    // lv_demo_widgets();
    // lv_demo_music();
    // lv_demo_benchmark();
    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {

            lv_timer_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }
}

static void on_modem_event(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == MODEM_BOARD_EVENT)
    {
        if (event_id == MODEM_EVENT_SIMCARD_DISCONN)
        {
            ESP_LOGW(TAG, "Modem Board Event: SIM Card disconnected");
        }
        else if (event_id == MODEM_EVENT_SIMCARD_CONN)
        {
            ESP_LOGI(TAG, "Modem Board Event: SIM Card Connected");
        }
        else if (event_id == MODEM_EVENT_DTE_DISCONN)
        {
            ESP_LOGW(TAG, "Modem Board Event: USB disconnected");
        }
        else if (event_id == MODEM_EVENT_DTE_CONN)
        {
            ESP_LOGI(TAG, "Modem Board Event: USB connected");
        }
        else if (event_id == MODEM_EVENT_DTE_RESTART)
        {
            ESP_LOGW(TAG, "Modem Board Event: Hardware restart");
        }
        else if (event_id == MODEM_EVENT_DTE_RESTART_DONE)
        {
            ESP_LOGI(TAG, "Modem Board Event: Hardware restart done");
        }
        else if (event_id == MODEM_EVENT_NET_CONN)
        {
            ESP_LOGI(TAG, "Modem Board Event: Network connected");
        }
        else if (event_id == MODEM_EVENT_NET_DISCONN)
        {
            ESP_LOGW(TAG, "Modem Board Event: Network disconnected");
        }
        else if (event_id == MODEM_EVENT_WIFI_STA_CONN)
        {
            ESP_LOGI(TAG, "Modem Board Event: Station connected");
        }
        else if (event_id == MODEM_EVENT_WIFI_STA_DISCONN)
        {
            ESP_LOGW(TAG, "Modem Board Event: All stations disconnected");
        }
    }
}

extern "C" void app_main(void)
{
#ifdef CONFIG_AUDIO_CODEC_ES8311_ES7210
    // Make GPIO15 HIGH to enable the 4G module
    gpio_config_t ml307_enable_config = {
        .pin_bit_mask = (1ULL << 15),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&ml307_enable_config);
    gpio_set_level(GPIO_NUM_15, 1);
#endif

    // Check if the reset button is pressed
    SystemReset system_reset;
    system_reset.CheckButtons();

    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    /* Initialize I2C peripheral */
    const i2c_config_t es_i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 400000,
        }};
    (i2c_param_config(I2C_NUM_0, &es_i2c_cfg));
    (i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
    static esp_io_expander_handle_t io_expander = NULL; // IO expander tca9554 handle
    if ((esp_io_expander_new_i2c_tca9554(I2C_NUM_0, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554, &io_expander) != ESP_OK) &&
        (esp_io_expander_new_i2c_tca9554(I2C_NUM_0, BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A, &io_expander) != ESP_OK))
    {
        ESP_LOGE(TAG, "Failed to initialize IO expander");
    }
    else
    {
        ESP_LOGI(TAG, "Initialize IO expander OK");

        (esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT));
        (esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_2, false));
        vTaskDelay(200);
        (esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_2, true));
    }
    tfcard_ret = fm_sdcard_init();

    //     /* Waiting for modem powerup */
    //     ESP_LOGI(TAG, "====================================");
    //     ESP_LOGI(TAG, "     ESP 4G Cat.1 Wi-Fi Router");
    //     ESP_LOGI(TAG, "====================================");

    //     /* Initialize modem board. Dial-up internet */
    //     modem_config_t modem_config = MODEM_DEFAULT_CONFIG();
    //     /* Modem init flag, used to control init process */
    // #ifndef CONFIG_EXAMPLE_ENTER_PPP_DURING_INIT
    //     /* if Not enter ppp, modem will enter command mode after init */
    //     modem_config.flags |= MODEM_FLAGS_INIT_NOT_ENTER_PPP;
    //     /* if Not waiting for modem ready, just return after modem init */
    //     modem_config.flags |= MODEM_FLAGS_INIT_NOT_BLOCK;
    // #endif
    //     modem_config.handler = on_modem_event;
    //     modem_board_init(&modem_config);

    // Otherwise, launch the application

    xTaskCreatePinnedToCore(&gui_task, "gui task", 1024 * 5, NULL, 5, NULL, 0);
    vTaskDelay(1000);
    // label_ask_set_text("可以唤醒我啦");
    Application::GetInstance().Start();
    if (tfcard_ret == ESP_OK)
    {
        while (1)
        {
            switch (biaoqing)
            {
            case 0:
                play_change(FACE_STATIC);
                break;

            case 1:
                play_change(FACE_HAPPY);

                break;
            case 2:
                play_change(FACE_ANGRY);

                break;
            case 3:
                play_change(FACE_BAD);

                break;
            case 4:
                play_change(FACE_FEAR);

                break;
            case 5:
                play_change(FACE_NOGOOD);

                break;
            default:
                break;
            }
            biaoqing = 0;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
    // Dump CPU usage every 10 second
    while (true)
    {

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
    }
}
