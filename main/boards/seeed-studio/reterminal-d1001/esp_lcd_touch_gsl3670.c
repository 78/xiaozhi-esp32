#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gsl3670.h"

#define TAG "gsl3670"

/* gsl3670 registers */
#define ESP_LCD_TOUCH_GSL3670_READ_XY_REG     (0x80)

/* gsl3670 support key num */
#define ESP_gsl3670_TOUCH_MAX_BUTTONS         (9)


static XY_DATA_T XY_Coordinate[MAX_FINGER_NUM]={0};
esp_lcd_touch_handle_t esp_lcd_touch_gsl3670;

static uint8_t Finger_num = 0;
static TP_STATE_E tp_event = TP_PEN_NONE;
static uint8_t pre_pen_flag = 0;

static uint16_t x_new = 0;
static uint16_t y_new = 0;
static uint16_t x_start = 0 , y_start = 0;

static void gsl3670_set_reset(esp_lcd_touch_handle_t tp, bool asserted)
{
    const gsl3670_driver_config_t *cfg = (const gsl3670_driver_config_t *)tp->config.driver_data;
    if (cfg != NULL && cfg->set_reset != NULL) {
        cfg->set_reset(cfg->ctx, asserted);
    }
}

static esp_err_t esp_lcd_touch_gsl3670_read_data(esp_lcd_touch_handle_t tp);
static bool esp_lcd_touch_gsl3670_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
static esp_err_t esp_lcd_touch_gsl3670_get_button_state(esp_lcd_touch_handle_t tp, uint8_t n, uint8_t *state);
#endif
static esp_err_t esp_lcd_touch_gsl3670_del(esp_lcd_touch_handle_t tp);

/* I2C read/write */
static esp_err_t touch_gsl3670_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);
static esp_err_t touch_gsl3670_i2c_write(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);

/* gsl3670 reset */
static esp_err_t touch_gsl3670_reset(esp_lcd_touch_handle_t tp);
/* Read status and config register */
static esp_err_t touch_gsl3670_read_cfg(esp_lcd_touch_handle_t tp);

/* gsl3670 enter/exit sleep mode */
static esp_err_t esp_lcd_touch_gsl3670_enter_sleep(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3670_exit_sleep(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3670_startup_chip(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3670_read_ram_fw(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3670_load_fw(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3670_clear_reg(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3670_init(esp_lcd_touch_handle_t tp);
static TP_STATE_E _Get_Cal_msg(void);

esp_err_t esp_lcd_touch_new_i2c_gsl3670(esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *out_touch)
{
     esp_err_t ret = ESP_OK;

    assert(io != NULL);
    assert(config != NULL);
    assert(out_touch != NULL);

    /* Prepare main structure */
    esp_lcd_touch_gsl3670 = heap_caps_calloc(1, sizeof(esp_lcd_touch_t), MALLOC_CAP_DEFAULT);
    ESP_GOTO_ON_FALSE(esp_lcd_touch_gsl3670, ESP_ERR_NO_MEM, err, TAG, "no mem for GSL3670 controller");

    /* Communication interface */
    esp_lcd_touch_gsl3670->io = io;

    /* Only supported callbacks are set */
    esp_lcd_touch_gsl3670->read_data = esp_lcd_touch_gsl3670_read_data;
    esp_lcd_touch_gsl3670->get_xy = esp_lcd_touch_gsl3670_get_xy;
#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
    esp_lcd_touch_gsl3670->get_button_state = esp_lcd_touch_gsl3670_get_button_state;
#endif
    esp_lcd_touch_gsl3670->del = esp_lcd_touch_gsl3670_del;
    esp_lcd_touch_gsl3670->enter_sleep = esp_lcd_touch_gsl3670_enter_sleep;
    esp_lcd_touch_gsl3670->exit_sleep = esp_lcd_touch_gsl3670_exit_sleep;

    /* Mutex */
    esp_lcd_touch_gsl3670->data.lock.owner = portMUX_FREE_VAL;

    /* Save config */
    memcpy(&esp_lcd_touch_gsl3670->config, config, sizeof(esp_lcd_touch_config_t));
    const gsl3670_driver_config_t *gsl3670_config = (const gsl3670_driver_config_t *)esp_lcd_touch_gsl3670->config.driver_data;

    if(gsl3670_config) {
        gsl3670_set_reset(esp_lcd_touch_gsl3670, true);
        vTaskDelay(pdMS_TO_TICKS(10));
        gsl3670_set_reset(esp_lcd_touch_gsl3670, false);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* Read status and config info */
    ESP_LOGI(TAG,"init gls3670");
    touch_gsl3670_read_cfg(esp_lcd_touch_gsl3670);
    esp_lcd_touch_gsl3670_init(esp_lcd_touch_gsl3670);
    ret = esp_lcd_touch_gsl3670_read_ram_fw(esp_lcd_touch_gsl3670);
    // touch_gsl3670_read_cfg(esp_lcd_touch_gsl3670);

    /* Prepare pin for touch interrupt */
    if (esp_lcd_touch_gsl3670->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = (esp_lcd_touch_gsl3670->config.levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE),
            .pin_bit_mask = BIT64(esp_lcd_touch_gsl3670->config.int_gpio_num)
        };
        ret = gpio_config(&int_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");

        /* Register interrupt callback */
        if (esp_lcd_touch_gsl3670->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_gsl3670, esp_lcd_touch_gsl3670->config.interrupt_callback);
        }
    }
 
err:
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (0x%x)! Touch controller GSL3670 initialization failed!", ret);
        if (esp_lcd_touch_gsl3670) {
            esp_lcd_touch_gsl3670_del(esp_lcd_touch_gsl3670);
        }
    }

    *out_touch = esp_lcd_touch_gsl3670;

    return ret;

}

static esp_err_t esp_lcd_touch_gsl3670_enter_sleep(esp_lcd_touch_handle_t tp)
{
    // TODO
    return ESP_OK;
}

static esp_err_t esp_lcd_touch_gsl3670_exit_sleep(esp_lcd_touch_handle_t tp)
{
    // TODO
    return ESP_OK;
}

static esp_err_t esp_lcd_touch_gsl3670_read_data(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);
    uint8_t touch_data[44];
    memset(XY_Coordinate, 0, sizeof(XY_Coordinate));

    // Filter state tracking: GSL3670 hardware contact IDs are 1..10 (or 0..15)
    static uint16_t s_last_x[16] = {0};
    static uint16_t s_last_y[16] = {0};
    static bool s_active[16] = {false};

    esp_err_t err = touch_gsl3670_i2c_read(tp, ESP_LCD_TOUCH_GSL3670_READ_XY_REG, touch_data, sizeof(touch_data));
    if (err != ESP_OK) {
        Finger_num = 0;
        memset(s_active, 0, sizeof(s_active));
        static int err_count = 0;
        if (++err_count % 50 == 0) {
            ESP_LOGW(TAG, "touch_gsl3670_i2c_read err=%s", esp_err_to_name(err));
        }
        return err;
    }

    Finger_num = (touch_data[0] > MAX_FINGER_NUM) ? MAX_FINGER_NUM : touch_data[0];

    if (Finger_num == 0) {
        memset(s_active, 0, sizeof(s_active));
        return ESP_OK;
    }

    // Mark current active contact IDs to purge state for contacts that were lifted
    bool current_seen[16] = {false};

    // Hardware electrode matrix configuration for GSL3670 on reTerminal D1001:
    // - drv (driving lines): 26 lines -> full-scale range = 26 * 64 = 1664 (maps to long edge / Y)
    // - sen (sensing lines): 14 lines -> full-scale range = 14 * 64 = 896  (maps to short edge / X)
    const uint32_t x_scale = tp->config.x_max ? tp->config.x_max : 800;
    const uint32_t y_scale = tp->config.y_max ? tp->config.y_max : 1280;

    for (int j = 0; j < Finger_num; j++) {
        // Crucial: mask high 4 bits on both coordinate bytes; upper nibbles
        // contain hardware contact tracking and status flags rather than ADC value.
        uint32_t raw_drv = ((touch_data[(j + 1) * 4 + 3] & 0x0f) << 8) | touch_data[(j + 1) * 4 + 2];
        uint32_t raw_sen = ((touch_data[(j + 1) * 4 + 1] & 0x0f) << 8) | touch_data[(j + 1) * 4 + 0];
        uint8_t id = (touch_data[(j + 1) * 4 + 3] >> 4) & 0x0f;

        // Apply Silead calibration matrix scaling:
        // match_x = 4113, match_y = 4119 (linear trim)
        int32_t sen_cal = ((int32_t)raw_sen - 2) * 4119 + 2048;
        sen_cal = (sen_cal > 0) ? (sen_cal / 4096) : 0;

        int32_t drv_cal = ((int32_t)raw_drv * 4113 + 2048) / 4096;
        if (drv_cal < 0) drv_cal = 0;

        uint32_t target_x = ((uint32_t)sen_cal * x_scale + 448) / 896;
        uint32_t target_y = ((uint32_t)drv_cal * y_scale + 832) / 1664;

        // Bound to valid pixel index [0, scale - 1]
        if (x_scale > 0 && target_x >= x_scale) target_x = x_scale - 1;
        if (y_scale > 0 && target_y >= y_scale) target_y = y_scale - 1;

        // Low-pass jitter filter: tracked per hardware contact id
        uint8_t track_id = id & 0x0f;
        current_seen[track_id] = true;

        uint16_t final_x;
        uint16_t final_y;
        if (!s_active[track_id]) {
            final_x = (uint16_t)target_x;
            final_y = (uint16_t)target_y;
            s_active[track_id] = true;
        } else {
            final_x = (uint16_t)((s_last_x[track_id] * 1 + target_x * 3 + 2) / 4);
            final_y = (uint16_t)((s_last_y[track_id] * 1 + target_y * 3 + 2) / 4);
        }
        s_last_x[track_id] = final_x;
        s_last_y[track_id] = final_y;

        XY_Coordinate[j].x_position = final_x;
        XY_Coordinate[j].y_position = final_y;
        XY_Coordinate[j].finger_id = id;

        ESP_LOGD(TAG, "TOUCH [%d] id=%u: raw(sen=%lu, drv=%lu) -> cal(%ld, %ld) -> mapped(x=%u, y=%u)",
                 j, (unsigned int)id, (unsigned long)raw_sen, (unsigned long)raw_drv,
                 (long)sen_cal, (long)drv_cal, (unsigned int)final_x, (unsigned int)final_y);
    }

    // Reset filter memory for contacts no longer present
    for (int k = 0; k < 16; k++) {
        if (!current_seen[k]) {
            s_active[k] = false;
        }
    }

    return ESP_OK;
}

static bool esp_lcd_touch_gsl3670_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    assert(tp != NULL);
    assert(x != NULL);
    assert(y != NULL);
    assert(point_num != NULL);
    assert(max_point_num > 0);

    portENTER_CRITICAL(&tp->data.lock);

    if(max_point_num > Finger_num)
        *point_num = Finger_num;
    else
        *point_num = max_point_num;
    for(int i=0;i<*point_num;i++)
    {
        x[i] = XY_Coordinate[i].x_position;
        y[i] = XY_Coordinate[i].y_position;
        // strength[i] = XY_Coordinate[i].finger_id;
    }
    

    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
static esp_err_t esp_lcd_touch_gsl3670_get_button_state(esp_lcd_touch_handle_t tp, uint8_t n, uint8_t *state)
{
    esp_err_t err = ESP_OK;
    assert(tp != NULL);
    assert(state != NULL);

    *state = 0;

    portENTER_CRITICAL(&tp->data.lock);

    if (n > tp->data.buttons) {
        err = ESP_ERR_INVALID_ARG;
    } else {
        *state = tp->data.button[n].status;
    }

    portEXIT_CRITICAL(&tp->data.lock);

    return err;
}
#endif

static esp_err_t esp_lcd_touch_gsl3670_del(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);

    /* Reset GPIO pin settings */
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }

    free(tp);

    return ESP_OK;
}

static esp_err_t esp_lcd_touch_gsl3670_init(esp_lcd_touch_handle_t tp)
{
    ESP_LOGI(TAG,"start init");
    esp_lcd_touch_gsl3670_clear_reg(tp);
    touch_gsl3670_reset(tp);
    esp_lcd_touch_gsl3670_load_fw(tp);
    esp_lcd_touch_gsl3670_startup_chip(tp);
    touch_gsl3670_reset(tp);
    esp_lcd_touch_gsl3670_startup_chip(tp);

    return ESP_OK;
}


static esp_err_t touch_gsl3670_reset(esp_lcd_touch_handle_t tp)
{
    unsigned char write_buf[4];
    uint8_t addr;
    assert(tp != NULL);

    gsl3670_set_reset(tp, true);
    vTaskDelay(pdMS_TO_TICKS(20));
    gsl3670_set_reset(tp, false);
    vTaskDelay(pdMS_TO_TICKS(20));

    // addr = 0xe0;
    // write_buf[0] = 0x88;
    // touch_gsl3670_i2c_write(tp,addr,write_buf,1);
    // vTaskDelay(pdMS_TO_TICKS(10));
    addr = 0xe4;
    write_buf[0]=0x04;
    touch_gsl3670_i2c_write(tp,addr,write_buf,1);
    vTaskDelay(pdMS_TO_TICKS(10));

    write_buf[0] =0x00;
    write_buf[1] =0x00;
    write_buf[2] =0x00;
    write_buf[3] =0x00;
    touch_gsl3670_i2c_write(tp,0xbc,write_buf,4);

    vTaskDelay(pdMS_TO_TICKS(10));

    return ESP_OK;
}

static esp_err_t touch_gsl3670_read_cfg(esp_lcd_touch_handle_t tp)
{
    uint8_t buf[4];
    uint8_t write[4];
    uint8_t i2c_buffer_read = 0;
    uint8_t i2c_buffer_write = 0x12;
    esp_err_t ret = ESP_OK;

    write[0] = 0x12;
    write[1] = 0x34;
    write[2] = 0x56;
    assert(tp != NULL);

    ESP_LOGI(TAG,"gsl3670 connect");

    ESP_RETURN_ON_ERROR(touch_gsl3670_i2c_read(tp, 0xf0, (uint8_t *)&buf, 4), TAG, "gsl3670 read error!");
    ESP_LOGI(TAG,"gsl3670 read reg 0xf0 before is %x %x %x %x",buf[0],buf[1],buf[2],buf[3]);
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG,"gsl3670 writing 0xf0 0x12");
    ESP_RETURN_ON_ERROR(touch_gsl3670_i2c_write(tp,0xf0,write,4),TAG,"gsl3670 read error");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(touch_gsl3670_i2c_read(tp, 0xf0, (uint8_t *)&buf, 4), TAG, "gsl3670 read error!");
    ESP_LOGI(TAG,"gsl3670 read reg 0xf0 after is %x %x %x %x",buf[0],buf[1],buf[2],buf[3]);

    if(i2c_buffer_read == i2c_buffer_write)
    {
        ret = ESP_OK;
        ESP_LOGI(TAG,"read cfg success");
    }
    else 
        ret = ESP_FAIL;

    return ret;
}

static esp_err_t esp_lcd_touch_gsl3670_startup_chip(esp_lcd_touch_handle_t tp)
{
    esp_err_t ret = ESP_OK;
    uint8_t write_buf[4];
    uint8_t addr = 0xe0;
    write_buf[0] = 0x00;
    ESP_LOGI(TAG,"enter");
    ESP_RETURN_ON_ERROR(touch_gsl3670_i2c_write(tp,addr,write_buf,1),TAG,"gsl3670 read error");
    vTaskDelay(pdMS_TO_TICKS(10));

    // Hardware boot ready; no external tracking table needed
    return ret;
}

static esp_err_t esp_lcd_touch_gsl3670_read_ram_fw(esp_lcd_touch_handle_t tp)
{
    uint8_t read_buf[4];
    uint8_t addr = 0xb0;
    ESP_LOGI(TAG,"enter read_ram_fw");
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_RETURN_ON_ERROR(touch_gsl3670_i2c_read(tp, addr, (uint8_t *)&read_buf, 4), TAG, "gsl3670 read error!");
    ESP_LOGI(TAG,"gsl3670 startup_chip read 0xb0 = %x,%x,%x,%x (expect 5a,5a,5a,5a)",read_buf[3],read_buf[2],read_buf[1],read_buf[0]);
    if(read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t touch_gsl3670_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len)
{
    assert(tp != NULL);
    assert(data != NULL);
    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
  
}

static esp_err_t touch_gsl3670_i2c_write(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data,uint8_t len)
{
    assert(tp != NULL);
    return esp_lcd_panel_io_tx_param(tp->io, reg, data, len);
}

static esp_err_t esp_lcd_touch_gsl3670_load_fw(esp_lcd_touch_handle_t tp)
{
    ESP_LOGI(TAG,"start load fw");
    uint16_t addr;
    unsigned char wrbuf[4];
    uint16_t source_line = 0;
    uint16_t source_len = sizeof(GSLX670_FW) / sizeof(struct fw_data);

    for(source_line=0;source_line<source_len;source_line++)
    {
        addr = GSLX670_FW[source_line].offset;
        wrbuf[0] = (uint8_t)(GSLX670_FW[source_line].val & 0x000000ff);
        wrbuf[1] = (uint8_t)((GSLX670_FW[source_line].val & 0x0000ff00) >> 8);
        wrbuf[2] = (uint8_t)((GSLX670_FW[source_line].val & 0x00ff0000) >> 16);
        wrbuf[3] = (uint8_t)((GSLX670_FW[source_line].val & 0xff000000) >> 24);
        /* Stop at the first failed write: a half written firmware image
         * would otherwise be handed to LVGL as a working touch panel. */
        ESP_RETURN_ON_ERROR(touch_gsl3670_i2c_write(tp, addr, wrbuf, (addr == 0xf0) ? 1 : 4), TAG,
                            "gsl3670 firmware download failed at line %u", source_line);
    }
    ESP_LOGI(TAG,"load fw success");
    return ESP_OK;
}

static esp_err_t esp_lcd_touch_gsl3670_clear_reg(esp_lcd_touch_handle_t tp)
{
    uint8_t addr;
    uint8_t wrbuf[4];

    ESP_LOGI(TAG,"clear reg");
    // addr = 0xe0;
    // wrbuf[0] = 0x88;
    // touch_gsl3670_i2c_write(tp,addr,wrbuf,1);
    gsl3670_set_reset(tp, true);
    vTaskDelay(pdMS_TO_TICKS(20));
    gsl3670_set_reset(tp, false);
    vTaskDelay(pdMS_TO_TICKS(20));
    
    addr = 0x88;
    wrbuf[0] = 0x01;
    touch_gsl3670_i2c_write(tp,addr,wrbuf,1);
    vTaskDelay(pdMS_TO_TICKS(5));
    addr = 0xe4;
    wrbuf[0] = 0x04;
    touch_gsl3670_i2c_write(tp,addr,wrbuf,1);
    vTaskDelay(pdMS_TO_TICKS(5));
    addr = 0xe0;
    wrbuf[0] = 0x00;
    touch_gsl3670_i2c_write(tp,addr,wrbuf,1);
    vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

static TP_STATE_E _Get_Cal_msg(void)
{
    uint8 pen_flag = 0;
	uint16 x_poit, y_poit, x2_poit, y2_poit;
	int32 x_delta = 0 , y_delta = 0;

	pen_flag = Finger_num;
	x_poit = XY_Coordinate[0].x_position;
	y_poit = XY_Coordinate[0].y_position;
	x2_poit = XY_Coordinate[1].x_position;
	y2_poit = XY_Coordinate[1].y_position;

	if(pen_flag==0)
	{
		if(tp_event == TP_PEN_MOVE)//the last event=move
		{
			x_new = x_poit;
			y_new = y_poit;
		}
		else//the last event=down
		{
			x_new = x_start;
			y_new = y_start;
		}

		tp_event = TP_PEN_UP;
	}
	else if(pen_flag==2)
	{
		tp_event = TP_PEN_DOWN;
		x_start = x_poit;
		y_start = y_poit;
		x_new = x_poit;
		y_new = y_poit;
	}
	else if(pre_pen_flag!=1)//pen_flag=1,pre_pen_flag==0 or 2
	{
		tp_event = TP_PEN_DOWN;
		x_start = x_poit;
		y_start = y_poit;
		x_new = x_poit;
		y_new = y_poit;
	 }
	else// if((pen_flag==1)&&(pre_pen_flag==1))
	{
		x_delta = x_poit - x_start;
		y_delta = y_poit - y_start;
		if((x_delta>20)||(x_delta<-20)||(y_delta>25)||(y_delta<-25))
		{
			tp_event = TP_PEN_MOVE;
		}

		if(tp_event == TP_PEN_MOVE)
		{
			x_new = x_poit;
			y_new = y_poit;
		}
		else
		{
			x_new = x_start;
			y_new = y_start;
		}

	 }

	pre_pen_flag = pen_flag;
	return tp_event;
}

esp_err_t esp_while_read()
{
    return esp_lcd_touch_gsl3670_read_ram_fw(esp_lcd_touch_gsl3670);
}
