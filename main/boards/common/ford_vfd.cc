#include "ford_vfd.h"
#include "driver/usb_serial_jtag.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>
#include <esp_log.h>
#include "settings.h"

#define TAG "FORD_VFD"

FORD_VFD::FORD_VFD(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num) : _cs(cs)
{
	// Initialize the SPI bus configuration structure
	spi_bus_config_t buscfg = {0};

	// Set the clock and data pins for the SPI bus
	buscfg.sclk_io_num = clk;
	buscfg.data0_io_num = din;

	// Set the maximum transfer size in bytes
	buscfg.max_transfer_sz = 1024;

	// Initialize the SPI bus with the specified configuration
	ESP_ERROR_CHECK(spi_bus_initialize(spi_num, &buscfg, SPI_DMA_CH_AUTO));

	// Initialize the SPI device interface configuration structure
	spi_device_interface_config_t devcfg = {
		.mode = 0,				  // Set the SPI mode to 0
		.clock_speed_hz = 400000, // Set the clock speed to 400kHz
		.spics_io_num = cs,		  // Set the chip select pin
		.flags = 0,
		.queue_size = 7,
	};

	// Add the PT6324 device to the SPI bus with the specified configuration
	ESP_ERROR_CHECK(spi_bus_add_device(spi_num, &devcfg, &spi_device_));

	init();

	ESP_LOGI(TAG, "FORD_VFD Initalized");
}

void FORD_VFD::write_data8(uint8_t dat)
{
	write_data8(&dat, 1);
}

void FORD_VFD::write_data8(uint8_t *dat, int len)
{
	// Create an SPI transaction structure and initialize it to zero
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));

	// Set the length of the transaction in bits
	t.length = len * 8;

	// Set the pointer to the data buffer to be transmitted
	t.tx_buffer = dat;

	// Queue the SPI transaction. This function will block until the transaction can be queued.
	ESP_ERROR_CHECK(spi_device_queue_trans(spi_device_, &t, portMAX_DELAY));
	// The following code can be uncommented if you need to wait for the transaction to complete and verify the result
	spi_transaction_t *ret_trans;
	ESP_ERROR_CHECK(spi_device_get_trans_result(spi_device_, &ret_trans, portMAX_DELAY));
	assert(ret_trans == &t);

	return;
}

void FORD_VFD::refrash(uint8_t *gram, int size)
{

	write_data8((uint8_t *)gram, size);
}

void FORD_VFD::init()
{

	write_data8(0x55);

	write_data8((uint8_t *)init_data_block1, sizeof(init_data_block1));

	write_data8((uint8_t *)init_data_block2, sizeof(init_data_block2));

	write_data8((uint8_t *)init_data_block3, sizeof(init_data_block3));

	write_data8((uint8_t *)init_data_block4, sizeof(init_data_block4));

	write_data8((uint8_t *)init_data_block5, sizeof(init_data_block5));

	write_data8((uint8_t *)gram, sizeof(gram));

	write_data8((uint8_t *)init_data_block7, sizeof(init_data_block7));

	write_data8((uint8_t *)init_data_block8, sizeof(init_data_block8));
}

uint8_t FORD_VFD::get_oddgroup(int x, uint8_t dot, uint8_t group)
{
	if (x % 3 == 0)
	{
		if (dot)
			group |= 2;
		else
			group &= ~2;
	}
	else if (x % 3 == 1)
	{
		if (dot)
			group |= 4;
		else
			group &= ~4;
	}
	else if (x % 3 == 2)
	{
		if (dot)
			group |= 1;
		else
			group &= ~1;
	}
	return group & 0x7;
}

uint8_t FORD_VFD::get_evengroup(int x, uint8_t dot, uint8_t group)
{
	if (x % 3 == 0)
	{
		if (dot)
			group |= 2;
		else
			group &= ~2;
	}
	else if (x % 3 == 1)
	{
		if (dot)
			group |= 1;
		else
			group &= ~1;
	}
	else if (x % 3 == 2)
	{
		if (dot)
			group |= 4;
		else
			group &= ~4;
	}
	return group & 0x7;
}

void FORD_VFD::draw_point(int x, int y, uint8_t dot)
{
	uint16_t index;
	uint8_t temp;
	index = 2 + 16 * (x / 3) + y / 4 * 3; //+y%16
	if (index > 480)
		index += 32;
	else if (index > 256)
		index += 16;

	if (y % 4 == 0)
	{
		if (16 * (x / 3) / 16 % 2)
		{
			temp = (get_oddgroup(x, dot, (gram[index] >> 5) & 0x7) << 5);
			gram[index] &= ~0xe0;
			gram[index] |= temp;
		} // 前三
		else
		{
			temp = (get_evengroup(x, dot, (gram[index] >> 2) & 0x7) << 2);
			gram[index] &= ~0x1c;
			gram[index] |= temp;
		}
	}
	else if (y % 4 == 1)
	{
		if (16 * (x / 3) / 16 % 2)
		{
			temp = (get_oddgroup(x, dot, (gram[index] << 1) & 0x7) >> 1);
			gram[index] &= ~0x3;
			gram[index] |= temp;

			temp = (get_oddgroup(x, dot, (gram[index + 1] >> 7) & 0x7) << 7);
			gram[index + 1] &= ~0x80;
			gram[index + 1] |= temp;
		} // 前三
		else
		{
			temp = (get_evengroup(x, dot, (gram[index + 1] >> 4) & 0x7) << 4);
			gram[index + 1] &= ~0x70;
			gram[index + 1] |= temp;
		}
	}
	else if (y % 4 == 2)
	{
		if (16 * (x / 3) / 16 % 2)
		{
			temp = (get_oddgroup(x, dot, (gram[index + 1] >> 1) & 0x7) << 1);
			gram[index + 1] &= ~0xe;
			gram[index + 1] |= temp;
		} // 前三
		else
		{
			temp = (get_evengroup(x, dot, (gram[index + 1] << 2) & 0x7) >> 2);
			gram[index + 1] &= ~0x1;
			gram[index + 1] |= temp;
			temp = (get_evengroup(x, dot, (gram[index + 2] >> 6) & 0x7) << 6);
			gram[index + 2] &= ~0xc0;
			gram[index + 2] |= temp;
		}
	}
	else if (y % 4 == 3)
	{
		if (16 * (x / 3) / 16 % 2)
		{
			temp = (get_oddgroup(x, dot, (gram[index + 2] >> 3) & 0x7) << 3);
			gram[index + 2] &= ~0x38;
			gram[index + 2] |= temp;
		} // 前三
		else
		{
			temp = (get_evengroup(x, dot, (gram[index + 2]) & 0x7));
			gram[index + 2] &= ~0x7;
			gram[index + 2] |= temp;
		}
	}
}
void FORD_VFD::find_enum_code(FORD_Symbols flag, int *byteIndex, int *bitIndex)
{
	*byteIndex = symbolPositions[flag].byteIndex;
	*bitIndex = symbolPositions[flag].bitIndex;
}

void FORD_VFD::symbolhelper(FORD_Symbols symbol, bool is_on)
{
	if (symbol >= FORD_SYMBOL_MAX)
		return;

	int byteIndex, bitIndex;
	find_enum_code(symbol, &byteIndex, &bitIndex);

	if (is_on)
		gram[byteIndex] |= bitIndex;
	else
		gram[byteIndex] &= ~bitIndex;
}

uint8_t process_bit(uint8_t real, uint8_t realbitdelta, uint8_t phy, uint8_t phybitdelta)
{
	if (phy & (1 << phybitdelta))
		return real | (1 << realbitdelta);
	else
		return real & (~(1 << realbitdelta));
}

uint8_t FORD_VFD::find_hex_code(char ch)
{
	if (ch >= ' ' && ch <= 'Z')
		return hex_codes[ch - ' '];
	else if (ch >= 'a' && ch <= 'z')
		return hex_codes[ch - 'a' + 'A' - ' '];
	return 0;
}

void FORD_VFD::charhelper(int index, char ch)
{
	if (index >= 9)
		return;
	uint8_t val = find_hex_code(ch);
	charhelper(index, val);
}

void FORD_VFD::charhelper(int index, uint8_t code)
{
	if (index >= 9)
		return;
	switch (index)
	{
	case 0:
		gram[270 + 3] = process_bit(gram[270 + 3], 0, code, 2);
		break;
	case 1:
		gram[270 + 0] = process_bit(gram[270 + 0], 2, code, 3);
		gram[270 + 1] = process_bit(gram[270 + 1], 6, code, 4);
		gram[270 + 1] = process_bit(gram[270 + 1], 2, code, 2);
		gram[270 + 2] = process_bit(gram[270 + 2], 1, code, 5);
		gram[270 + 3] = process_bit(gram[270 + 3], 5, code, 1);
		gram[270 + 2] = process_bit(gram[270 + 2], 6, code, 6);
		gram[270 + 3] = process_bit(gram[270 + 3], 1, code, 0);
		break;
	case 2:
		gram[270 + 0] = process_bit(gram[270 + 0], 3, code, 3);
		gram[270 + 1] = process_bit(gram[270 + 1], 7, code, 4);
		gram[270 + 1] = process_bit(gram[270 + 1], 3, code, 2);
		gram[270 + 2] = process_bit(gram[270 + 2], 2, code, 5);
		gram[270 + 3] = process_bit(gram[270 + 3], 6, code, 1);
		gram[270 + 2] = process_bit(gram[270 + 2], 7, code, 6);
		gram[270 + 3] = process_bit(gram[270 + 3], 2, code, 0);
		break;
	case 3:
		gram[270 + 0] = process_bit(gram[270 + 0], 4, code, 3);
		gram[270 + 0] = process_bit(gram[270 + 0], 0, code, 4);
		gram[270 + 1] = process_bit(gram[270 + 1], 4, code, 2);
		gram[270 + 2] = process_bit(gram[270 + 2], 3, code, 5);
		gram[270 + 3] = process_bit(gram[270 + 3], 7, code, 1);
		gram[270 + 1] = process_bit(gram[270 + 1], 0, code, 6);
		gram[270 + 3] = process_bit(gram[270 + 3], 3, code, 0);
		break;
	case 4:
		gram[270 + 0] = process_bit(gram[270 + 0], 5, code, 3);
		gram[270 + 0] = process_bit(gram[270 + 0], 1, code, 4);
		gram[270 + 1] = process_bit(gram[270 + 1], 5, code, 2);
		gram[270 + 2] = process_bit(gram[270 + 2], 4, code, 5);
		gram[270 + 1] = process_bit(gram[270 + 1], 1, code, 6);
		gram[270 + 2] = process_bit(gram[270 + 2], 0, code, 1);
		gram[270 + 3] = process_bit(gram[270 + 3], 4, code, 0);
		break;
	case 5:
		gram[814 + 0] = process_bit(gram[814 + 0], 2, code, 3);
		gram[814 + 1] = process_bit(gram[814 + 1], 6, code, 4);
		gram[814 + 1] = process_bit(gram[814 + 1], 2, code, 2);
		gram[814 + 2] = process_bit(gram[814 + 2], 1, code, 5);
		gram[814 + 3] = process_bit(gram[814 + 3], 5, code, 1);
		gram[814 + 2] = process_bit(gram[814 + 2], 6, code, 6);
		gram[814 + 3] = process_bit(gram[814 + 3], 1, code, 0);
		break;
	case 6:
		gram[814 + 0] = process_bit(gram[814 + 0], 3, code, 3);
		gram[814 + 1] = process_bit(gram[814 + 1], 7, code, 4);
		gram[814 + 1] = process_bit(gram[814 + 1], 3, code, 2);
		gram[814 + 2] = process_bit(gram[814 + 2], 2, code, 5);
		gram[814 + 3] = process_bit(gram[814 + 3], 6, code, 1);
		gram[814 + 2] = process_bit(gram[814 + 2], 7, code, 6);
		gram[814 + 3] = process_bit(gram[814 + 3], 2, code, 0);
		break;
	case 7:
		gram[814 + 0] = process_bit(gram[814 + 0], 4, code, 3);
		gram[814 + 0] = process_bit(gram[814 + 0], 0, code, 4);
		gram[814 + 1] = process_bit(gram[814 + 1], 4, code, 2);
		gram[814 + 2] = process_bit(gram[814 + 2], 3, code, 5);
		gram[814 + 3] = process_bit(gram[814 + 3], 7, code, 1);
		gram[814 + 1] = process_bit(gram[814 + 1], 0, code, 6);
		gram[814 + 3] = process_bit(gram[814 + 3], 3, code, 0);
		break;
	case 8:
		gram[814 + 0] = process_bit(gram[814 + 0], 5, code, 3);
		gram[814 + 0] = process_bit(gram[814 + 0], 1, code, 4);
		gram[814 + 1] = process_bit(gram[814 + 1], 5, code, 2);
		gram[814 + 2] = process_bit(gram[814 + 2], 4, code, 5);
		gram[814 + 1] = process_bit(gram[814 + 1], 1, code, 6);
		gram[814 + 2] = process_bit(gram[814 + 2], 0, code, 1);
		gram[814 + 3] = process_bit(gram[814 + 3], 4, code, 0);
		break;
	default:
		break;
	}
}

void FORD_VFD::test()
{
	xTaskCreate(
		[](void *arg)
		{
			int count = 0;
			FORD_VFD *vfd = static_cast<FORD_VFD *>(arg);
			int64_t start_time = esp_timer_get_time() / 1000;
			ESP_LOGI(TAG, "FORD_VFD Test");
			while (1)
			{
				int64_t current_time = esp_timer_get_time() / 1000;
				int64_t elapsed_time = current_time - start_time;

				if (elapsed_time >= 500)
				{
					count++;
					start_time = current_time;
				}
				vfd->symbolhelper(BT, true);
				for (size_t i = 0; i < 9; i++)
				{
					vfd->charhelper(i, (char)('0' + count % 10));
				}
				for (size_t i = 0; i < FORD_WIDTH; i++)
				{
					for (size_t j = 0; j < FORD_HEIGHT; j++)
					{
						vfd->draw_point(i, FORD_HEIGHT - 1 - j, (j + i) % 2);
					}
				}
				vfd->refrash(vfd->gram, sizeof vfd->gram);

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
