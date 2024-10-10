/**
 * @file ADCRAW.c
 */

#include "adcraw.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stddef.h>

#if CONFIG_LV_TOUCH_CONTROLLER_ADCRAW

#define TAG "ADCRAW"
#define CALIBRATIONINSET 1 // range 0 <= CALIBRATIONINSET <= 40
#define SAMPLE_CALIBRATION_POINTS 4
// use this scale factor to avoid working in floating point numbers
#define TOUCHSCREEN_RESISTIVE_CALIBRATION_SCALE_FACTOR 8
#define SCALE_FACTOR (1 << TOUCHSCREEN_RESISTIVE_CALIBRATION_SCALE_FACTOR)
#define CAL_X_INSET (((GetMaxX() + 1) * (CALIBRATIONINSET >> 1)) / 100)
#define CAL_Y_INSET (((GetMaxY() + 1) * (CALIBRATIONINSET >> 1)) / 100)
#define NUMSAMPLES 8

static void ad_touch_handler(void *arg);

static const esp_timer_create_args_t periodic_timer_args = {
	.callback = &ad_touch_handler,
};
static esp_timer_handle_t periodic_timer;

// Current ADC values for X and Y channels
int16_t adcX, adcY = 0;
int16_t temp_x, temp_y, temp_z1, temp_z2;

// coefficient values
int _trA, _trB, _trC, _trD;

int16_t xRawTouch[SAMPLE_CALIBRATION_POINTS];
int16_t yRawTouch[SAMPLE_CALIBRATION_POINTS];
TOUCH_STATES state;

const gpio_num_t yu = TOUCHSCREEN_RESISTIVE_PIN_YU;
const gpio_num_t xl = TOUCHSCREEN_RESISTIVE_PIN_XL;
const gpio_num_t yd = TOUCHSCREEN_RESISTIVE_PIN_YD;
const gpio_num_t xr = TOUCHSCREEN_RESISTIVE_PIN_XR;

static const int gpio_to_adc[] = {
	GPIO_TO_ADC_ELEMENT(TOUCHSCREEN_RESISTIVE_PIN_YD),
	GPIO_TO_ADC_ELEMENT(TOUCHSCREEN_RESISTIVE_PIN_XR)
};

static void TouchCalculateCalPoints(void)
{
	int32_t trA, trB, trC, trD;     // variables for the coefficients
	int32_t trAhold, trBhold, trChold, trDhold;
	int32_t test1, test2;           // temp variables (must be signed type)

	int16_t xPoint[SAMPLE_CALIBRATION_POINTS], yPoint[SAMPLE_CALIBRATION_POINTS];

	yPoint[0] = yPoint[1] = CAL_Y_INSET;
	yPoint[2] = yPoint[3] = (GetMaxY() - CAL_Y_INSET);
	xPoint[0] = xPoint[3] = CAL_X_INSET;
	xPoint[1] = xPoint[2] = (GetMaxX() - CAL_X_INSET);

	// calculate points transfer function
	// based on two simultaneous equations solve for the constants

	// use sample points 1 and 4
	// Dy1 = aTy1 + b; Dy4 = aTy4 + b
	// Dx1 = cTx1 + d; Dy4 = aTy4 + b

	test1 = (int32_t)yPoint[0] - (int32_t)yPoint[3];
	test2 = (int32_t)yRawTouch[0] - (int32_t)yRawTouch[3];

	trA = ((int32_t)((int32_t)test1 * SCALE_FACTOR) / test2);
	trB = ((int32_t)((int32_t)yPoint[0] * SCALE_FACTOR) - (trA * (int32_t)yRawTouch[0]));

	test1 = (int32_t)xPoint[0] - (int32_t)xPoint[2];
	test2 = (int32_t)xRawTouch[0] - (int32_t)xRawTouch[2];

	trC = ((int32_t)((int32_t)test1 * SCALE_FACTOR) / test2);
	trD = ((int32_t)((int32_t)xPoint[0] * SCALE_FACTOR) - (trC * (int32_t)xRawTouch[0]));

	trAhold = trA;
	trBhold = trB;
	trChold = trC;
	trDhold = trD;

	// use sample points 2 and 3
	// Dy2 = aTy2 + b; Dy3 = aTy3 + b
	// Dx2 = cTx2 + d; Dy3 = aTy3 + b

	test1 = (int32_t)yPoint[1] - (int32_t)yPoint[2];
	test2 = (int32_t)yRawTouch[1] - (int32_t)yRawTouch[2];

	trA = ((int32_t)(test1 * SCALE_FACTOR) / test2);
	trB = ((int32_t)((int32_t)yPoint[1] * SCALE_FACTOR) - (trA * (int32_t)yRawTouch[1]));

	test1 = (int32_t)xPoint[1] - (int32_t)xPoint[3];
	test2 = (int32_t)xRawTouch[1] - (int32_t)xRawTouch[3];

	trC = ((int32_t)((int32_t)test1 * SCALE_FACTOR) / test2);
	trD = ((int32_t)((int32_t)xPoint[1] * SCALE_FACTOR) - (trC * (int32_t)xRawTouch[1]));

	// get the average and use it
	_trA = (trA + trAhold) >> 1;
	_trB = (trB + trBhold) >> 1;
	_trC = (trC + trChold) >> 1;
	_trD = (trD + trDhold) >> 1;
}

void adcraw_init(void)
{
	state = IDLE;      // set the state of the state machine to start the sampling

	gpio_set_drive_capability(yu, GPIO_DRIVE_CAP_3);
	gpio_set_drive_capability(yd, GPIO_DRIVE_CAP_3);
	gpio_set_drive_capability(xl, GPIO_DRIVE_CAP_3);
	gpio_set_drive_capability(xr, GPIO_DRIVE_CAP_3);
	
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 5 * 1000));        //5ms (expressed as microseconds)

	/*Load calibration data*/
	xRawTouch[0] = TOUCHCAL_ULX;
	yRawTouch[0] = TOUCHCAL_ULY;
	xRawTouch[1] = TOUCHCAL_URX;
	yRawTouch[1] = TOUCHCAL_URY;
	xRawTouch[3] = TOUCHCAL_LLX;
	yRawTouch[3] = TOUCHCAL_LLY;
	xRawTouch[2] = TOUCHCAL_LRX;
	yRawTouch[2] = TOUCHCAL_LRY;

	TouchCalculateCalPoints();
}

static void setup_axis(gpio_num_t plus, gpio_num_t minus, gpio_num_t measure, gpio_num_t ignore)
{
	// Set GPIOs:
	// - Float "ignore" and "measure"
	gpio_pad_select_gpio(ignore);
	gpio_set_direction(ignore, GPIO_MODE_DISABLE);
	gpio_set_pull_mode(ignore, GPIO_FLOATING);
	gpio_pad_select_gpio(measure);
	gpio_set_direction(measure, GPIO_MODE_DISABLE);
	gpio_set_pull_mode(measure, GPIO_FLOATING);
	// - Set "plus" to 1, "minus" to 0
	gpio_config(&(gpio_config_t) {
		.mode = GPIO_MODE_OUTPUT,
		.pin_bit_mask = (1ULL << plus) | (1ULL << minus)
	});
	gpio_set_level(plus, 1);
	gpio_set_level(minus, 0);
}

static void setup_adc(gpio_num_t measure)
{
	// Init ADC
	adc1_channel_t channel = gpio_to_adc[measure];
	adc_gpio_init(ADC_UNIT_1, channel);
	adc1_config_width(ADC_WIDTH_BIT_10);
	adc1_config_channel_atten(channel, ADC_ATTEN_DB_11);
}
    
static void insert_sort(int16_t array[], uint8_t size) {
	uint8_t j;
	int16_t save;

	for (int i = 1; i < size; i++) {
		save = array[i];
		for (j = i; j >= 1 && save < array[j - 1]; j--)
			array[j] = array[j - 1];
		array[j] = save;
	}
}

static void ad_touch_handler(void *arg)
{
	(void) arg;
	uint8_t i;
	int16_t samples[NUMSAMPLES];

	switch (state) {
	case IDLE:
		adcX = 0;
		adcY = 0;

	case SET_X :
		setup_axis(yd, yu, xr, xl);
		setup_adc(xr);
		state = READ_X;
		break;

	case READ_X:
		for (i = 0; i < NUMSAMPLES; i++)
			samples[i] = adc1_get_raw(gpio_to_adc[xr]);
		insert_sort(samples, NUMSAMPLES);
		temp_x = samples[NUMSAMPLES / 2];
		
	case SET_Y :
		setup_axis(xl, xr, yd, yu);
		setup_adc(yd);
		state = READ_Y;
		break;

	case READ_Y:
		for (i = 0; i < NUMSAMPLES; i++)
			samples[i] = adc1_get_raw(gpio_to_adc[yd]);
		insert_sort(samples, NUMSAMPLES);
		temp_y = samples[NUMSAMPLES / 2];
		
	case SET_Z1 :
		setup_axis(yu, xl, yd, xr);
		setup_adc(yd);
		state = READ_Z1;
		break;

	case READ_Z1:
		temp_z1 = adc1_get_raw(gpio_to_adc[yd]);

	case SET_Z2 :
		setup_axis(yu, xl, xr, yd);
		setup_adc(yd);
		state = READ_Z2;
		break;

	case READ_Z2:
		temp_z2 = adc1_get_raw(gpio_to_adc[xr]);
		
		if (temp_z1 < TOUCHSCREEN_RESISTIVE_PRESS_THRESHOLD) {
#if CONFIG_LV_TOUCH_XY_SWAP
			adcX = temp_y; 
			adcY = temp_x; 
#else
			adcX = temp_x; 
			adcY = temp_y; 
#endif
		}
		else {
			adcX = -1; 
			adcY = -1; 
		}
		state = SET_X;
		//printf("x: %d   y: %d   z: %d\n", adcX, adcY, temp_z1 - temp_z2);
		break;
	}

	return;
}

static int16_t TouchGetRawX(void)
{
	int16_t x = adcX;

#if CONFIG_LV_TOUCH_INVERT_X
	x = 1023 - x;
#endif
	return x;
}

static int16_t TouchGetX(void)
{
	int16_t result = TouchGetRawX();

	if (result > 0) {
		result = (int16_t)((((int32_t)_trC * result) + _trD) >> TOUCHSCREEN_RESISTIVE_CALIBRATION_SCALE_FACTOR);
	}
	printf("x: %d\n", result);
	return (result);
}

static int16_t TouchGetRawY(void)
{
	int16_t y = adcY;

#if CONFIG_LV_TOUCH_INVERT_Y
	y = 1023 - y;
#endif
	return y;
}

static int16_t TouchGetY(void)
{
	int16_t result = TouchGetRawY();

	if (result > 0) {
		result = (int16_t)((((int32_t)_trA * result) + (int32_t)_trB) >> TOUCHSCREEN_RESISTIVE_CALIBRATION_SCALE_FACTOR);
	}
	printf("y: %d\n", result);
	return (result);
}

/**
 * Get the current position and state of the touchpad
 * @param data store the read data here
 * @return false: because no more data to be read
 */
bool adcraw_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
	static int16_t last_x = 0;
	static int16_t last_y = 0;

	int16_t x, y;

	x = TouchGetX();
	y = TouchGetY();

	if ((x > 0) && (y > 0)) {
		data->point.x = x;
		data->point.y = y;
		last_x = data->point.x;
		last_y = data->point.y;
		data->state = LV_INDEV_STATE_PR;
	}
	else {
		data->point.x = last_x;
		data->point.y = last_y;
		data->state = LV_INDEV_STATE_REL;
	}

	return false;
}
#endif //CONFIG_LV_TOUCH_CONTROLLER_ADCRAW
