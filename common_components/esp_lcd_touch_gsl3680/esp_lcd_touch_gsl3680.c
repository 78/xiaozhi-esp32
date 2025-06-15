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
#include "esp_lcd_touch_gsl3680.h"
#include "gsl_point_id.h"

#define TAG "gsl3680"

/* gsl3680 registers */
#define ESP_LCD_TOUCH_GSL3680_READ_XY_REG     (0x80)

/* gsl3680 support key num */
#define ESP_gsl3680_TOUCH_MAX_BUTTONS         (9)


unsigned int gsl_config_data_id[] =
{
	0xccb69a,  
	0x200,
	0,0,
	0,
	0,0,0,
	0,0,0,0,0,0,0,0x1cc86fd6,


	0x40000d00,0xa,0xe001a,0xe001a,0x3200500,0,0x5100,0x8e00,
	0,0x320014,0,0x14,0,0,0,0,
	0x8,0x4000,0x1000,0x10170002,0x10110000,0,0,0x4040404,
	0x1b6db688,0x64,0xb3000f,0xad0019,0xa60023,0xa0002d,0xb3000f,0xad0019,
	0xa60023,0xa0002d,0xb3000f,0xad0019,0xa60023,0xa0002d,0xb3000f,0xad0019,
	0xa60023,0xa0002d,0x804000,0x90040,0x90001,0,0,0,
	0,0,0,0x14012c,0xa003c,0xa0078,0x400,0x1081,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,

	0,//key_map
	0x3200384,0x64,0x503e8,//0
	0,0,0,//1
	0,0,0,//2
	0,0,0,//3
	0,0,0,//4
	0,0,0,//5
	0,0,0,//6
	0,0,0,//7

	0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,


	0x220,
	0,0,0,0,0,0,0,0,
	0x10203,0x4050607,0x8090a0b,0xc0d0e0f,0x10111213,0x14151617,0x18191a1b,0x1c1d1e1f,
	0x20212223,0x24252627,0x28292a2b,0x2c2d2e2f,0x30313233,0x34353637,0x38393a3b,0x3c3d3e3f,
	0x10203,0x4050607,0x8090a0b,0xc0d0e0f,0x10111213,0x14151617,0x18191a1b,0x1c1d1e1f,
	0x20212223,0x24252627,0x28292a2b,0x2c2d2e2f,0x30313233,0x34353637,0x38393a3b,0x3c3d3e3f,

	0x10203,0x4050607,0x8090a0b,0xc0d0e0f,0x10111213,0x14151617,0x18191a1b,0x1c1d1e1f,
	0x20212223,0x24252627,0x28292a2b,0x2c2d2e2f,0x30313233,0x34353637,0x38393a3b,0x3c3d3e3f,

	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,

	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,

	0x10203,0x4050607,0x8090a0b,0xc0d0e0f,0x10111213,0x14151617,0x18191a1b,0x1c1d1e1f,
	0x20212223,0x24252627,0x28292a2b,0x2c2d2e2f,0x30313233,0x34353637,0x38393a3b,0x3c3d3e3f,

	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,


	0x3,
	0x101,0,0x100,0,
	0x20,0x10,0x8,0x4,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,

	0x4,0,0,0,0,0,0,0,
	0x3800680,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,
};



// static TG_STATE_E tpc_gesture_id = TG_UNKNOWN_STATE;
static XY_DATA_T XY_Coordinate[MAX_FINGER_NUM]={0};
esp_lcd_touch_handle_t esp_lcd_touch_gsl3680;

static uint8_t Finger_num = 0;
static TP_STATE_E tp_event = TP_PEN_NONE;
static uint8_t pre_pen_flag = 0;

static uint16_t x_new = 0;
static uint16_t y_new = 0;
static uint16_t x_start = 0 , y_start = 0;


static esp_err_t esp_lcd_touch_gsl3680_read_data(esp_lcd_touch_handle_t tp);
static bool esp_lcd_touch_gsl3680_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
static esp_err_t esp_lcd_touch_gsl3680_get_button_state(esp_lcd_touch_handle_t tp, uint8_t n, uint8_t *state);
#endif
static esp_err_t esp_lcd_touch_gsl3680_del(esp_lcd_touch_handle_t tp);

/* I2C read/write */
static esp_err_t touch_gsl3680_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);
static esp_err_t touch_gsl3680_i2c_write(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);

/* gsl3680 reset */
static esp_err_t touch_gsl3680_reset(esp_lcd_touch_handle_t tp);
/* Read status and config register */
static esp_err_t touch_gsl3680_read_cfg(esp_lcd_touch_handle_t tp);

/* gsl3680 enter/exit sleep mode */
static esp_err_t esp_lcd_touch_gsl3680_enter_sleep(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_exit_sleep(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_startup_chip(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_read_ram_fw(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_load_fw(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_clear_reg(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_init(esp_lcd_touch_handle_t tp);
static TP_STATE_E _Get_Cal_msg(void);

esp_err_t esp_lcd_touch_new_i2c_gsl3680(esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *out_touch)
{
     esp_err_t ret = ESP_OK;

    assert(io != NULL);
    assert(config != NULL);
    assert(out_touch != NULL);

    /* Prepare main structure */
    esp_lcd_touch_gsl3680 = heap_caps_calloc(1, sizeof(esp_lcd_touch_t), MALLOC_CAP_DEFAULT);
    ESP_GOTO_ON_FALSE(esp_lcd_touch_gsl3680, ESP_ERR_NO_MEM, err, TAG, "no mem for GSL3680 controller");

    /* Communication interface */
    esp_lcd_touch_gsl3680->io = io;

    /* Only supported callbacks are set */
    esp_lcd_touch_gsl3680->read_data = esp_lcd_touch_gsl3680_read_data;
    // esp_lcd_touch_gsl3680->get_xy = esp_lcd_touch_gsl3680_get_xy;
    esp_lcd_touch_gsl3680->get_xy = esp_lcd_touch_gsl3680_get_xy;
#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
    esp_lcd_touch_gsl3680->get_button_state = esp_lcd_touch_gsl3680_get_button_state;
#endif
    esp_lcd_touch_gsl3680->del = esp_lcd_touch_gsl3680_del;
    esp_lcd_touch_gsl3680->enter_sleep = esp_lcd_touch_gsl3680_enter_sleep;
    esp_lcd_touch_gsl3680->exit_sleep = esp_lcd_touch_gsl3680_exit_sleep;

    /* Mutex */
    esp_lcd_touch_gsl3680->data.lock.owner = portMUX_FREE_VAL;

    /* Save config */
    memcpy(&esp_lcd_touch_gsl3680->config, config, sizeof(esp_lcd_touch_config_t));
    esp_lcd_touch_io_gsl3680_config_t *gsl3680_config = (esp_lcd_touch_io_gsl3680_config_t *)esp_lcd_touch_gsl3680->config.driver_data;

    /* Prepare pin for touch controller reset */
    if (esp_lcd_touch_gsl3680->config.rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(esp_lcd_touch_gsl3680->config.rst_gpio_num),
        };
        ret = gpio_config(&rst_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");
    }

    if (gsl3680_config && esp_lcd_touch_gsl3680->config.rst_gpio_num != GPIO_NUM_NC && esp_lcd_touch_gsl3680->config.int_gpio_num != GPIO_NUM_NC) {
        /* Prepare pin for touch controller int */
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .intr_type = GPIO_INTR_DISABLE,
            .pull_down_en = 0,
            .pull_up_en = 1,
            .pin_bit_mask = BIT64(esp_lcd_touch_gsl3680->config.int_gpio_num),
        };
        ret = gpio_config(&int_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");

        ESP_RETURN_ON_ERROR(gpio_set_level(esp_lcd_touch_gsl3680->config.rst_gpio_num, esp_lcd_touch_gsl3680->config.levels.reset), TAG, "GPIO set level error!");
        ESP_RETURN_ON_ERROR(gpio_set_level(esp_lcd_touch_gsl3680->config.int_gpio_num, 0), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Select I2C addr, set output high or low */
        uint32_t gpio_level;
        if (ESP_LCD_TOUCH_IO_I2C_GSL3680_ADDRESS == gsl3680_config->dev_addr) {
            gpio_level = 0;
        } else {
            gpio_level = 0;
            ESP_LOGE(TAG, "Addr (0x%X) is invalid", gsl3680_config->dev_addr);
        }
        ESP_RETURN_ON_ERROR(gpio_set_level(esp_lcd_touch_gsl3680->config.int_gpio_num, gpio_level), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(1));

        ESP_RETURN_ON_ERROR(gpio_set_level(esp_lcd_touch_gsl3680->config.rst_gpio_num, !esp_lcd_touch_gsl3680->config.levels.reset), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(10));

        vTaskDelay(pdMS_TO_TICKS(50));
    } else {
        ESP_LOGW(TAG, "Unable to initialize the I2C address");
        /* Reset controller */
        ret = touch_gsl3680_reset(esp_lcd_touch_gsl3680);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GSL3680 reset failed");
    }

    /* Read status and config info */
    ESP_LOGI(TAG,"init gls3680");
    touch_gsl3680_read_cfg(esp_lcd_touch_gsl3680);
    esp_lcd_touch_gsl3680_init(esp_lcd_touch_gsl3680);
    ret = esp_lcd_touch_gsl3680_read_ram_fw(esp_lcd_touch_gsl3680);
    // touch_gsl3680_read_cfg(esp_lcd_touch_gsl3680);

    /* Prepare pin for touch interrupt */
    if (esp_lcd_touch_gsl3680->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = (esp_lcd_touch_gsl3680->config.levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE),
            .pin_bit_mask = BIT64(esp_lcd_touch_gsl3680->config.int_gpio_num)
        };
        ret = gpio_config(&int_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");

        /* Register interrupt callback */
        if (esp_lcd_touch_gsl3680->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_gsl3680, esp_lcd_touch_gsl3680->config.interrupt_callback);
        }
    }
 
err:
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (0x%x)! Touch controller GSL3680 initialization failed!", ret);
        if (esp_lcd_touch_gsl3680) {
            esp_lcd_touch_gsl3680_del(esp_lcd_touch_gsl3680);
        }
    }

    *out_touch = esp_lcd_touch_gsl3680;

    return ret;

}


static esp_err_t esp_lcd_touch_gsl3680_enter_sleep(esp_lcd_touch_handle_t tp)
{
    // esp_err_t err = touch_gsl3680_i2c_write(tp, ESP_LCD_TOUCH_GSL3680_ENTER_SLEEP, 0x05);
    // ESP_RETURN_ON_ERROR(err, TAG, "Enter Sleep failed!");

    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, 0), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static esp_err_t esp_lcd_touch_gsl3680_exit_sleep(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, 1), TAG, "GPIO set level error!");
    vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

static esp_err_t esp_lcd_touch_gsl3680_read_data(esp_lcd_touch_handle_t tp)
{
    esp_err_t err;
    uint8_t touch_data[24];
    uint8_t touch_cnt = 0;
    uint16_t x_poit, y_poit, x2_poit, y2_poit;

    assert(tp != NULL);

// #ifdef USE_GSL_NOID_VERSION
    struct gsl_touch_info cinfo = {0};
    unsigned int tmp1 = 0;
    uint8_t buf[4] = {0};
// #endif

    memset(XY_Coordinate,0,sizeof(XY_Coordinate));

    err = touch_gsl3680_i2c_read(tp, ESP_LCD_TOUCH_GSL3680_READ_XY_REG, touch_data, 44);
    Finger_num = touch_data[0];
    // ESP_LOGI(TAG,"0x80 = %d",touch_data[0]);

    cinfo.finger_num = Finger_num;	
    for(int j=0;j<Finger_num;j++)
    {
        x_poit = touch_data[(j+1)*4+3] & 0x0f;
        x2_poit = touch_data[(j+1)*4+2];
        cinfo.x[j] = x_poit << 8 | x2_poit;
        y_poit = touch_data[(j+1)*4+1];
        y2_poit = touch_data[(j+1)*4+0];
        cinfo.y[j] = y_poit << 8 | y2_poit;
        cinfo.id[j] = ((touch_data[(j+1)*4+3] & 0xf0) >> 4);
    }
		
	gsl_alg_id_main(&cinfo);
	tmp1=gsl_mask_tiaoping();
	
	if(tmp1>0&&tmp1<0xffffffff)
	{
		uint8 addr = 0xf0;
		buf[0]=0xa;buf[1]=0;buf[2]=0;buf[3]=0;
		touch_gsl3680_i2c_write(tp,addr, buf, 4);
		addr = 0x8;
		buf[0]=(uint8)(tmp1 & 0xff);
		buf[1]=(uint8)((tmp1>>8) & 0xff);
		buf[2]=(uint8)((tmp1>>16) & 0xff);
		buf[3]=(uint8)((tmp1>>24) & 0xff);
		
		touch_gsl3680_i2c_write(tp,addr, buf, 4);
	}
	Finger_num = cinfo.finger_num;	

    for(int j=0;j<Finger_num;j++)
    {
        XY_Coordinate[j].x_position =  cinfo.x[j];
        XY_Coordinate[j].y_position =  cinfo.y[j];
        XY_Coordinate[j].finger_id = cinfo.id[j];
    }
			
    // if(Finger_num >0)
    // int i=0
    // printf("%s: %d[i], %d[x_position], %d[y_position], %d[finger_id], %d[finger_num]\n",
    //       __func__, i, XY_Coordinate[i].x_position, XY_Coordinate[i].y_position, XY_Coordinate[i].finger_id,Finger_num);
    // i=1;
    // printf("%s: %d[i], %d[x_position], %d[y_position], %d[finger_id], %d[finger_num]\n",
    //       __func__, i, XY_Coordinate[i].x_position, XY_Coordinate[i].y_position, XY_Coordinate[i].finger_id,Finger_num);
    
    
    return err;
}

static bool esp_lcd_touch_gsl3680_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
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
static esp_err_t esp_lcd_touch_gsl3680_get_button_state(esp_lcd_touch_handle_t tp, uint8_t n, uint8_t *state)
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

static esp_err_t esp_lcd_touch_gsl3680_del(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);

    /* Reset GPIO pin settings */
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }

    /* Reset GPIO pin settings */
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }

    free(tp);

    return ESP_OK;
}

/*===================================================================================================================================================================================================*/
static esp_err_t esp_lcd_touch_gsl3680_init(esp_lcd_touch_handle_t tp)
{
    ESP_LOGI(TAG,"start init");
    esp_lcd_touch_gsl3680_clear_reg(tp);
    touch_gsl3680_reset(tp);
    esp_lcd_touch_gsl3680_load_fw(tp);
    esp_lcd_touch_gsl3680_startup_chip(tp);
    touch_gsl3680_reset(tp);
    esp_lcd_touch_gsl3680_startup_chip(tp);

    return ESP_OK;
}


static esp_err_t touch_gsl3680_reset(esp_lcd_touch_handle_t tp)
{
    unsigned char write_buf[4];
    uint8_t addr;
    assert(tp != NULL);

    ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, 0), TAG, "GPIO set level error!");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, 1), TAG, "GPIO set level error!");
    vTaskDelay(pdMS_TO_TICKS(20));

    // addr = 0xe0;
    // write_buf[0] = 0x88;
    // touch_gsl3680_i2c_write(tp,addr,write_buf,1);
    // vTaskDelay(pdMS_TO_TICKS(10));
    addr = 0xe4;
    write_buf[0]=0x04;
    touch_gsl3680_i2c_write(tp,addr,write_buf,1);
    vTaskDelay(pdMS_TO_TICKS(10));

    write_buf[0] =0x00;
    write_buf[1] =0x00;
    write_buf[2] =0x00;
    write_buf[3] =0x00;
    touch_gsl3680_i2c_write(tp,0xbc,write_buf,4);

    vTaskDelay(pdMS_TO_TICKS(10));

    return ESP_OK;
}

static esp_err_t touch_gsl3680_read_cfg(esp_lcd_touch_handle_t tp)
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

    ESP_LOGI(TAG,"gsl3680 connect");

    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_read(tp, 0xf0, (uint8_t *)&buf, 4), TAG, "gsl3680 read error!");
    ESP_LOGI(TAG,"gsl3680 read reg 0xf0 before is %x %x %x %x",buf[0],buf[1],buf[2],buf[3]);
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG,"gsl3680 writing 0xf0 0x12");
    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_write(tp,0xf0,write,4),TAG,"gsl3680 read error");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_read(tp, 0xf0, (uint8_t *)&buf, 4), TAG, "gsl3680 read error!");
    ESP_LOGI(TAG,"gsl3680 read reg 0xf0 after is %x %x %x %x",buf[0],buf[1],buf[2],buf[3]);

    if(i2c_buffer_read == i2c_buffer_write)
    {
        ret = ESP_OK;
        ESP_LOGI(TAG,"read cfg success");
    }
    else 
        ret = ESP_FAIL;

    return ret;
}

static esp_err_t esp_lcd_touch_gsl3680_startup_chip(esp_lcd_touch_handle_t tp)
{
    esp_err_t ret = ESP_OK;
    uint8_t write_buf[4];
    uint8_t addr = 0xe0;
    write_buf[0] = 0x00;
    ESP_LOGI(TAG,"enter");
    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_write(tp,addr,write_buf,1),TAG,"gsl3680 read error");
    vTaskDelay(pdMS_TO_TICKS(10));

    gsl_DataInit(gsl_config_data_id);
    return ret;
}

static esp_err_t esp_lcd_touch_gsl3680_read_ram_fw(esp_lcd_touch_handle_t tp)
{
    uint8_t read_buf[4];
    uint8_t addr = 0xb0;
    ESP_LOGI(TAG,"enter read_ram_fw");
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_read(tp, addr, (uint8_t *)&read_buf, 4), TAG, "gsl3680 read error!");
    ESP_LOGI(TAG,"gsl3680 startup_chip failed read 0xb0 = %x,%x,%x,%x ",read_buf[3],read_buf[2],read_buf[1],read_buf[0]);
    if(read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a)
    {
         
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t touch_gsl3680_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len)
{
    assert(tp != NULL);
    assert(data != NULL);


    /* Read data */
    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
  
}

static esp_err_t touch_gsl3680_i2c_write(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data,uint8_t len)
{
    assert(tp != NULL);

    // *INDENT-OFF*
    // /* Write data */
    return esp_lcd_panel_io_tx_param(tp->io, reg, data, len);
    // // *INDENT-ON*
}

static esp_err_t esp_lcd_touch_gsl3680_load_fw(esp_lcd_touch_handle_t tp)
{
    ESP_LOGI(TAG,"start load fw");
    uint16_t addr;
    unsigned char wrbuf[4];
    uint16_t source_line = 0;
    uint16_t source_len = sizeof(GSLX680_FW) / sizeof(struct fw_data);

    for(source_line=0;source_line<source_len;source_line++)
    {
        addr = GSLX680_FW[source_line].offset;
        wrbuf[0] = (uint8_t)(GSLX680_FW[source_line].val & 0x000000ff);
        wrbuf[1] = (uint8_t)((GSLX680_FW[source_line].val & 0x0000ff00) >> 8);
        wrbuf[2] = (uint8_t)((GSLX680_FW[source_line].val & 0x00ff0000) >> 16);
        wrbuf[3] = (uint8_t)((GSLX680_FW[source_line].val & 0xff000000) >> 24);
        if(addr == 0xf0)
            touch_gsl3680_i2c_write(tp,addr,wrbuf,1);
        else
            touch_gsl3680_i2c_write(tp,addr,wrbuf,4);
        
    }
    ESP_LOGI(TAG,"load fw success");
    return ESP_OK;
}

static esp_err_t esp_lcd_touch_gsl3680_clear_reg(esp_lcd_touch_handle_t tp)
{
    uint8_t addr;
    uint8_t wrbuf[4];

    ESP_LOGI(TAG,"clear reg");
    // addr = 0xe0;
    // wrbuf[0] = 0x88;
    // touch_gsl3680_i2c_write(tp,addr,wrbuf,1);
    ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, 0), TAG, "GPIO set level error!");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, 1), TAG, "GPIO set level error!");
    vTaskDelay(pdMS_TO_TICKS(20));
    addr = 0x88;
    wrbuf[0] = 0x01;
    touch_gsl3680_i2c_write(tp,addr,wrbuf,1);
    vTaskDelay(pdMS_TO_TICKS(5));
    addr = 0xe4;
    wrbuf[0] = 0x04;
    touch_gsl3680_i2c_write(tp,addr,wrbuf,1);
    vTaskDelay(pdMS_TO_TICKS(5));
    addr = 0xe0;
    wrbuf[0] = 0x00;
    touch_gsl3680_i2c_write(tp,addr,wrbuf,1);
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
    return esp_lcd_touch_gsl3680_read_ram_fw(esp_lcd_touch_gsl3680);
}