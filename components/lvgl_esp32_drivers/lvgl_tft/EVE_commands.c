/*
@file    EVE_commands.c
@brief   contains FT8xx / BT8xx functions
@version 4.0
@date    2020-04-13
@author  Rudolph Riedel, David Jade

@section info

This file needs to be renamed to EVE_command.cpp for use with Arduino.
At least für ATSAM I had the best result with -O2.
The c-standard is C99.


@section LICENSE

MIT License

Copyright (c) 2016-2020 Rudolph Riedel and David Jade

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


@section History

4.1 LvGL edition

	- This version is a heavily modified version of the MIT licensed FT81x code from https://github.com/RudolphRiedel/FT800-FT813
	  This version is based on a fork by David Jade that added native SPI DMA support and stripped out non-ESP32 code.
	  It has also been trimmed down to suit LvGL's needs. Extra features can be enabled by defining FT81X_FULL
*/


#include <stdio.h>

#include "EVE.h"
#include "EVE_commands.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/soc_memory_layout.h"

#include "esp_log.h"

#include "disp_spi.h"

#include <string.h>

#if defined (BT81X_ENABLE)
#include <stdarg.h>
#endif

#define TAG_LOG "FT81X"

/* data structure for SPI reading that has (optional) space for inserted dummy byte */
typedef struct _spi_read_data {
#if defined(DISP_SPI_FULL_DUPLEX)
    uint8_t _dummy_byte;
#endif
    union {
        uint8_t byte;
        uint16_t word;
        uint32_t dword;
    } __attribute__((packed));
} spi_read_data __attribute__((aligned(4)));


/* Receive data helpers */
#define member_size(type, member)	sizeof(((type *)0)->member)

#if defined(DISP_SPI_FULL_DUPLEX)
#define SPI_READ_DUMMY_LEN	member_size(spi_read_data, _dummy_byte)
#else
#define SPI_READ_DUMMY_LEN 	0
#endif
#define SPI_READ_BYTE_LEN	(SPI_READ_DUMMY_LEN + member_size(spi_read_data, byte))
#define SPI_READ_WORD_LEN	(SPI_READ_DUMMY_LEN + member_size(spi_read_data, word))
#define SPI_READ_DWORD_LEN	(SPI_READ_DUMMY_LEN + member_size(spi_read_data, dword))


// EVE Memory Commands - used with EVE_memWritexx and EVE_memReadxx
#define MEM_WRITE		0x80 		// EVE Host Memory Write
#define MEM_READ		0x00 		// EVE Host Memory Read
#define MEM_WRITE_24 	0x800000	// EVE Host Memory Write (24-bit format)

volatile uint16_t cmdOffset = 0x0000; /* offset for the 4k co-processor FIFO */

volatile uint8_t cmd_burst = 0; /* flag to indicate cmd-burst is active */

// Buffer for SPI transactions
uint8_t SPIBuffer[SPI_BUFFER_SIZE];				// must be in DMA capable memory if DMA is used!
uint16_t SPIBufferIndex = 0;
disp_spi_send_flag_t SPIInherentSendFlags = 0;	// additional inherent SPI flags (for DIO/QIO mode switching)
uint8_t SPIDummyReadBits = 0;					// Dummy bits for reading in DIO/QIO modes

// Macros to make SPI use explicit and less verbose
// (macros do obscure code a little but they also help code search and readability)

// Buffer a byte
#define BUFFER_SPI_BYTE(byte) SPIBuffer[SPIBufferIndex++] = (byte);

// Buffer a Word - little Endian format
#define BUFFER_SPI_WORD(word) \
	BUFFER_SPI_BYTE((uint8_t)(word)) \
	BUFFER_SPI_BYTE((uint8_t)((word) >> 8))

// Buffer a DWord - little Endian format
#define BUFFER_SPI_DWORD(dword) \
	BUFFER_SPI_BYTE((uint8_t)(dword)) \
	BUFFER_SPI_BYTE((uint8_t)((dword) >> 8)) \
	BUFFER_SPI_BYTE((uint8_t)((dword) >> 16)) \
	BUFFER_SPI_BYTE((uint8_t)((dword) >> 24))

// Buffer a 24-bit SPI Memory Write address - big Endian format
#define BUFFER_SPI_WRITE_ADDRESS(addr) \
	BUFFER_SPI_BYTE((uint8_t)((addr) >> 16) | MEM_WRITE) \
	BUFFER_SPI_BYTE((uint8_t)((addr) >> 8)) \
	BUFFER_SPI_BYTE((uint8_t)(addr))

// Send buffer
#define SEND_SPI_BUFFER() \
	disp_spi_transaction(SPIBuffer, SPIBufferIndex, (disp_spi_send_flag_t)(DISP_SPI_SEND_QUEUED | SPIInherentSendFlags), NULL, 0, 0); \
	SPIBufferIndex = 0;

// Wait for DMA queued SPI transactions to complete
#define WAIT_SPI() \
	disp_wait_for_pending_transactions();



void DELAY_MS(uint16_t ms)
{
	vTaskDelay(ms / portTICK_PERIOD_MS);
}

#if EVE_USE_PDN
void EVE_pdn_set(void)
{
	gpio_set_level(EVE_PDN, 0);	/* Power-Down low */
}


void EVE_pdn_clear(void)
{
	gpio_set_level(EVE_PDN, 1);	/* Power-Down high */
}
#endif

void spi_acquire()
{
	disp_spi_acquire();
}


void spi_release()
{
	disp_spi_release();
}


void EVE_cmdWrite(uint8_t command, uint8_t parameter)
{
	const uint8_t data[] =
	{
		command,
		parameter,
		0x00
	};

	disp_spi_transaction(data, sizeof(data), (disp_spi_send_flag_t)(DISP_SPI_SEND_POLLING | SPIInherentSendFlags), NULL, 0, 0);
}


uint8_t EVE_memRead8(uint32_t ftAddress)
{
	spi_read_data data = {};
	disp_spi_send_flag_t readflags = (disp_spi_send_flag_t)(DISP_SPI_RECEIVE | DISP_SPI_SEND_POLLING | DISP_SPI_ADDRESS_24 | SPIInherentSendFlags);

#if defined(DISP_SPI_HALF_DUPLEX)
	// in half-duplex mode the FT81x requires dummy bits
	readflags |= DISP_SPI_VARIABLE_DUMMY;
#endif

	disp_spi_transaction(NULL, SPI_READ_BYTE_LEN, readflags, (uint8_t*)&data, ftAddress, SPIDummyReadBits);
	return data.byte;	/* return byte read */
}


uint16_t EVE_memRead16(uint32_t ftAddress)
{
#if defined(DISP_SPI_HALF_DUPLEX)
	// There are esp32 issues when reading in DMA half-duplex mode that prevents reading more than 1 byte at a time so we work around that
	uint16_t word = 0;
	for(int i = 0; i < sizeof(word); i++)
	{
		word |= (EVE_memRead8(ftAddress + i) << i * 8);
	}
	return word;
#endif

	spi_read_data data = {};
	disp_spi_send_flag_t readflags = (disp_spi_send_flag_t)(DISP_SPI_RECEIVE | DISP_SPI_SEND_POLLING | DISP_SPI_ADDRESS_24 | SPIInherentSendFlags);

#if defined(DISP_SPI_HALF_DUPLEX)
	// in half-duplex mode the FT81x requires dummy bits
	readflags |= DISP_SPI_VARIABLE_DUMMY;
#endif

	disp_spi_transaction(NULL, SPI_READ_WORD_LEN, readflags, (uint8_t*)&data, ftAddress, SPIDummyReadBits);
	return data.word; /* return integer read */
}


uint32_t EVE_memRead32(uint32_t ftAddress)
{
#if defined(DISP_SPI_HALF_DUPLEX)
	// There are esp32 issues when reading in DMA half-duplex mode that prevents reading more than 1 byte at a time so we work around that
	uint32_t dword = 0;
	for(int i = 0; i < sizeof(dword); i++)
	{
		dword |= (EVE_memRead8(ftAddress + i) << i * 8);
	}
	return dword;
#endif

	spi_read_data data = {};
	disp_spi_send_flag_t readflags = (disp_spi_send_flag_t)(DISP_SPI_RECEIVE | DISP_SPI_SEND_POLLING | DISP_SPI_ADDRESS_24 | SPIInherentSendFlags);

#if defined(DISP_SPI_HALF_DUPLEX)
	// in half-duplex mode the FT81x requires dummy bits
	readflags |= DISP_SPI_VARIABLE_DUMMY;
#endif

	disp_spi_transaction(NULL, SPI_READ_DWORD_LEN, readflags, (uint8_t*)&data, ftAddress, SPIDummyReadBits);
	return data.dword; /* return integer read */
}


void EVE_memWrite8(uint32_t ftAddress, uint8_t ftData8)
{
	disp_spi_transaction(&ftData8, sizeof(ftData8), (disp_spi_send_flag_t)(DISP_SPI_SEND_POLLING | DISP_SPI_ADDRESS_24 | SPIInherentSendFlags), NULL, (ftAddress | MEM_WRITE_24), 0);
}


void EVE_memWrite16(uint32_t ftAddress, uint16_t ftData16)
{
	disp_spi_transaction((uint8_t*)&ftData16, sizeof(ftData16), (disp_spi_send_flag_t)(DISP_SPI_SEND_POLLING | DISP_SPI_ADDRESS_24 | SPIInherentSendFlags), NULL, (ftAddress | MEM_WRITE_24), 0);
}


void EVE_memWrite32(uint32_t ftAddress, uint32_t ftData32)
{
	disp_spi_transaction((uint8_t*)&ftData32, sizeof(ftData32), (disp_spi_send_flag_t)(DISP_SPI_SEND_POLLING | DISP_SPI_ADDRESS_24 | SPIInherentSendFlags), NULL, (ftAddress | MEM_WRITE_24), 0);
}


// write to EVE memory via SPI memory-mapped protocol using queued SPI transactions (i.e. DMA)
// Note: data should be in DMA-capable memory!
void EVE_memWrite_buffer(uint32_t ftAddress, const uint8_t *data, uint32_t len, bool LvGL_Flush)
{
	// chunk by DMA transfer size (SPI_TRANSER_SIZE)
	uint32_t bytes_left = len;
	while(bytes_left > 0)
	{
		uint32_t block_len = (bytes_left > SPI_TRANSER_SIZE ? SPI_TRANSER_SIZE : bytes_left);

		// only send flush on last chunk
		disp_spi_send_flag_t flush_flag = 0;
		if(LvGL_Flush && bytes_left - block_len == 0)
		{
			flush_flag = DISP_SPI_SIGNAL_FLUSH;
		}

		disp_spi_transaction(data, block_len, (disp_spi_send_flag_t)(DISP_SPI_SEND_QUEUED | DISP_SPI_ADDRESS_24 | SPIInherentSendFlags | flush_flag), NULL, (ftAddress | MEM_WRITE_24), 0);
		data += block_len;
		ftAddress += block_len;
		bytes_left -= block_len;
	}
}


/* Check if the graphics processor completed executing the current command list. */
/* This is the case when REG_CMD_READ matches cmdOffset, indicating that all commands have been executed. */
uint8_t EVE_busy(void)
{
	uint16_t cmdBufferRead;

	WAIT_SPI();	// can't tell if EVE is busy if SPI is taking place

	cmdBufferRead = EVE_memRead16(REG_CMD_READ); /* read the graphics processor read pointer */

	if(cmdBufferRead == 0xFFF) /* we have a co-processor fault, make EVE play with us again */
	{
		#if defined (BT81X_ENABLE)

		uint16_t copro_patch_pointer;

		copro_patch_pointer = EVE_memRead16(REG_COPRO_PATCH_DTR);

		#endif

		EVE_memWrite8(REG_CPURESET, 1);   /* hold co-processor engine in the reset condition */
		EVE_memWrite16(REG_CMD_READ, 0);  /* set REG_CMD_READ to 0 */
		EVE_memWrite16(REG_CMD_WRITE, 0); /* set REG_CMD_WRITE to 0 */
		EVE_memWrite32(REG_CMD_DL, 0);    /* reset REG_CMD_DL to 0 as required by the BT81x programming guide, should not hurt FT8xx */
		cmdOffset = 0;
		EVE_memWrite8(REG_CPURESET, 0);  /* set REG_CMD_WRITE to 0 to restart the co-processor engine*/

		#if defined (BT81X_ENABLE)

		EVE_memWrite16(REG_COPRO_PATCH_DTR, copro_patch_pointer);

		DELAY_MS(5); /* just to be safe */

		// reset the SPI buffer just to be cautious
		SPIBufferIndex = 0;

		BUFFER_SPI_WRITE_ADDRESS(EVE_RAM_CMD + cmdOffset)
		BUFFER_SPI_DWORD(CMD_FLASHATTACH)
		BUFFER_SPI_DWORD(CMD_FLASHFAST)
		SEND_SPI_BUFFER()
		WAIT_SPI()

		cmdOffset += 8;

		BUFFER_SPI_BYTE
		BUFFER_SPI_BYTE(MEM_WRITE | 0x30); /* send Memory Write plus high address byte of REG_CMD_WRITE for EVE81x */
		BUFFER_SPI_BYTE(0x20);	/* send middle address byte of REG_CMD_WRITE for EVE81x */
		BUFFER_SPI_BYTE(0xfc);	/* send low address byte of REG_CMD_WRITE for EVE81x */
		BUFFER_SPI_WORD(cmdOffset);
		SEND_SPI_BUFFER()
		WAIT_SPI()

		EVE_memWrite8(REG_PCLK, EVE_PCLK); /* restore REG_PCLK in case it was set to zero by an error */

		DELAY_MS(5); /* just to be safe */

		#endif
	}

	if(cmdOffset != cmdBufferRead)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


void EVE_get_cmdoffset(void)
{
	cmdOffset = EVE_memRead16(REG_CMD_WRITE);
}


void EVE_inc_cmdoffset(uint16_t increment)
{
	cmdOffset += increment;
	cmdOffset &= 0x0fff;	// circular
}


/* order the command co-processor to start processing its FIFO queue and do not wait for completion */
void EVE_cmd_start(void)
{
	WAIT_SPI();

	EVE_memWrite16(REG_CMD_WRITE, cmdOffset);
}


/* order the command co-processor to start processing its FIFO queue and wait for completion */
void EVE_cmd_execute(void)
{
	EVE_cmd_start();
	while (EVE_busy());
}


/* begin a co-processor command, this is used for all non-display-list commands */
void EVE_begin_cmd(uint32_t command)
{
	BUFFER_SPI_WRITE_ADDRESS(EVE_RAM_CMD + cmdOffset)
	BUFFER_SPI_DWORD(command)

	EVE_inc_cmdoffset(4);	// only count the command size - not the write address directive
}


/* co-processor commands that are not used in displays lists, these are no to be used with async transfers */


/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_memzero(uint32_t ptr, uint32_t num)
{
	EVE_begin_cmd(CMD_MEMZERO);
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(8);

	SEND_SPI_BUFFER()
}


/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_memset(uint32_t ptr, uint8_t value, uint32_t num)
{
	EVE_begin_cmd(CMD_MEMSET);
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(value)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(12);

	SEND_SPI_BUFFER()
}


/* this is meant to be called outside display-list building, does not support cmd-burst */
void block_transfer(const uint8_t *data, uint32_t len);	// forward ref

void EVE_cmd_memwrite(uint32_t dest, uint32_t num, const uint8_t *data)
{
	EVE_begin_cmd(CMD_MEMWRITE);
	BUFFER_SPI_DWORD(dest)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(8);

	SEND_SPI_BUFFER()

	block_transfer(data, num);	// block_transfer is immediate - make sure CMD buffer is prepared!
}


/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_memcpy(uint32_t dest, uint32_t src, uint32_t num)
{
	EVE_begin_cmd(CMD_MEMCPY);
	BUFFER_SPI_DWORD(dest)
	BUFFER_SPI_DWORD(src)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(12);

	SEND_SPI_BUFFER()
}



/* commands for loading image data into FT8xx CMD memory, with DWORD padding: */
/* Note: these are immediate SPI transfers and assume the CMD buffer is prepared to process the data, etc... */
// Note: data should be in DMA-capable memory!
void eve_spi_CMD_write(uint64_t addr, const uint8_t *data, uint16_t len)
{
	// we can use a direct transaction because it is already chunked
	disp_spi_transaction(data, len, (disp_spi_send_flag_t)(DISP_SPI_SEND_QUEUED | DISP_SPI_ADDRESS_24 | SPIInherentSendFlags), NULL, (addr | MEM_WRITE_24), 0);

	uint8_t padding = len & 0x03; /* 0, 1, 2 or 3 */
	padding = 4 - padding; /* 4, 3, 2 or 1 */
	padding &= 3; /* 3, 2, 1 or 0 */

	// padding is another transaction and needs it's own address (this is a DMA requirement)
	if(padding)
	{
		addr += len;

		uint8_t padData[4] = {0};
		disp_spi_transaction(padData, padding, (disp_spi_send_flag_t)(DISP_SPI_SEND_QUEUED | DISP_SPI_ADDRESS_24 | SPIInherentSendFlags), NULL, (addr | MEM_WRITE_24), 0);

		len += padding;
	}

	EVE_inc_cmdoffset(len);
}


void block_transfer(const uint8_t *data, uint32_t len)
{
	WAIT_SPI();		// SPI commands must be in CMD buffer first

	uint32_t bytes_left;

	bytes_left = len;
	while(bytes_left > 0)
	{
		uint32_t block_len;
		block_len = (bytes_left > BLOCK_TRANSFER_SIZE ? BLOCK_TRANSFER_SIZE : bytes_left);

		eve_spi_CMD_write(EVE_RAM_CMD + cmdOffset, data, block_len);

		data += block_len;
		bytes_left -= block_len;

		// signal to process data
		EVE_cmd_execute();
	}
}

#if FT81X_FULL
/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
void EVE_cmd_inflate(uint32_t ptr, const uint8_t *data, uint16_t len)
{
	EVE_begin_cmd(CMD_INFLATE);
	BUFFER_SPI_DWORD(ptr)

	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()

	block_transfer(data, len);	// block_transfer is immediate - make sure CMD buffer is prepared!
}


#if defined (BT81X_ENABLE)
/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
void EVE_cmd_inflate2(uint32_t ptr, uint32_t options, const uint8_t *data, uint16_t len)
{
	EVE_begin_cmd(CMD_INFLATE2);
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(options)

	EVE_inc_cmdoffset(8);

	SEND_SPI_BUFFER()

	if(options == 0) /* direct data, not by Media-FIFO or Flash */
	{
		block_transfer(data, len);	// block_transfer is immediate - make sure CMD buffer is prepared!
	}
}
#endif


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
void EVE_cmd_loadimage(uint32_t ptr, uint32_t options, const uint8_t *data, uint16_t len)
{
	EVE_begin_cmd(CMD_LOADIMAGE);
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(options)

	EVE_inc_cmdoffset(8);

	SEND_SPI_BUFFER()

	#if defined (BT81X_ENABLE)
	if( ((options & EVE_OPT_MEDIAFIFO) == 0) && ((options & EVE_OPT_FLASH) == 0) )/* direct data, neither by Media-FIFO or from Flash */
	#elif defined (FT81X_ENABLE)
	if((options & EVE_OPT_MEDIAFIFO) == 0) /* direct data, not by Media-FIFO */
	#endif
	{
		block_transfer(data, len);	// block_transfer is immediate - make sure CMD buffer is prepared!
	}
}


#if defined (FT81X_ENABLE)
/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_mediafifo(uint32_t ptr, uint32_t size)
{
	EVE_begin_cmd(CMD_MEDIAFIFO);
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(size)

	EVE_inc_cmdoffset(8);

	SEND_SPI_BUFFER()
}
#endif


/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_interrupt(uint32_t ms)
{
	EVE_begin_cmd(CMD_INTERRUPT);
	BUFFER_SPI_DWORD(ms)

	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()
}


/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_setfont(uint32_t font, uint32_t ptr)
{
	EVE_begin_cmd(CMD_SETFONT);
	BUFFER_SPI_DWORD(font)
	BUFFER_SPI_DWORD(ptr)

	EVE_inc_cmdoffset(8);

	SEND_SPI_BUFFER()
}


#if defined (FT81X_ENABLE)
/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_setfont2(uint32_t font, uint32_t ptr, uint32_t firstchar)
{
	EVE_begin_cmd(CMD_SETFONT2);
	BUFFER_SPI_DWORD(font)
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(firstchar)

	EVE_inc_cmdoffset(12);

	SEND_SPI_BUFFER()
}
#endif


#if defined (FT81X_ENABLE)
/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_setrotate(uint32_t r)
{
	EVE_begin_cmd(CMD_SETROTATE);
	BUFFER_SPI_DWORD(r)

	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()
}
#endif


/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_snapshot(uint32_t ptr)
{
	EVE_begin_cmd(CMD_SNAPSHOT);
	BUFFER_SPI_DWORD(ptr)

	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()
}


#if defined (FT81X_ENABLE)
/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_snapshot2(uint32_t fmt, uint32_t ptr, int16_t x0, int16_t y0, int16_t w0, int16_t h0)
{
	EVE_begin_cmd(CMD_SNAPSHOT2);
	BUFFER_SPI_DWORD(fmt)
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(w0)
	BUFFER_SPI_WORD(h0)

	EVE_inc_cmdoffset(16);

	SEND_SPI_BUFFER()
}
#endif


/* this is meant to be called outside display-list building, does not support cmd-burst */
void EVE_cmd_track(int16_t x0, int16_t y0, int16_t w0, int16_t h0, int16_t tag)
{
	EVE_begin_cmd(CMD_TRACK);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(w0)
	BUFFER_SPI_WORD(h0)
	BUFFER_SPI_WORD(tag)
	BUFFER_SPI_WORD(0)

	EVE_inc_cmdoffset(12);

	SEND_SPI_BUFFER()
}



/* commands that return values by writing to the command-fifo */
/* note: yes, these are different than the functions in the Programmers Guide from FTDI,
	this is because I have no idea why anyone would want to pass "result" as an actual argument to these functions
	when this only marks the offset the command-processor is writing to */

/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* crc32 = EVE_cmd_memcrc(my_ptr_to_some_memory_region, some_amount_of_bytes); */
uint32_t EVE_cmd_memcrc(uint32_t ptr, uint32_t num)
{
	EVE_begin_cmd(CMD_MEMCRC);
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(num)
	BUFFER_SPI_DWORD(0)

	EVE_inc_cmdoffset(8);

	uint16_t offset = cmdOffset;
	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();

	return (EVE_memRead32(EVE_RAM_CMD + offset));
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* address = EVE_cmd_getpr(); */
uint32_t EVE_cmd_getptr(void)
{
	EVE_begin_cmd(CMD_GETPTR);

	uint16_t offset = cmdOffset;
	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();

	return (EVE_memRead32(EVE_RAM_CMD + offset));
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* regvalue = EVE_cmd_regread(ptr); */
/* this seems to be completely pointless, there is no real use for it outside a display-list since the register could be read directly */
/* and for what purpose would this be implemented to be used in a display list?? */
uint32_t EVE_cmd_regread(uint32_t ptr)
{
	uint16_t offset;

	EVE_begin_cmd(CMD_REGREAD);
	BUFFER_SPI_DWORD(ptr)

	EVE_inc_cmdoffset(4);

	offset = cmdOffset;
	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();

	return (EVE_memRead32(EVE_RAM_CMD + offset));
}


/*
	Get the properties of an image after a CMD_LOADIMAGE operation.

	uint32 pointer, width, height;
	EVE_LIB_GetProps(&pointer, &width, &height);

	uint32 width, height;
	EVE_LIB_GetProps(0, &width, &height);

	this is meant to be called outside display-list building, it includes executing the command
	and waiting for completion, does not support cmd-burst
*/
void EVE_LIB_GetProps(uint32_t *pointer, uint32_t *width, uint32_t *height)
{
	EVE_begin_cmd(CMD_GETPROPS);

	uint16_t offset = cmdOffset;
	EVE_inc_cmdoffset(12);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();

	if(pointer)
	{
		*pointer = EVE_memRead32(EVE_RAM_CMD + offset);
	}
	if(width)
	{
		*width = EVE_memRead32(EVE_RAM_CMD + offset + 4);
	}
	if(height)
	{
		*height = EVE_memRead32(EVE_RAM_CMD + offset + 8);
	}
}


/* FT811 / FT813 binary-blob from FTDIs AN_336 to patch the touch-engine for Goodix GT911 / GT9271 touch controllers */
#if defined (EVE_HAS_GT911)

#if	defined (__AVR__)
#include <avr/pgmspace.h>
#else
#define PROGMEM
#endif

const uint16_t EVE_GT911_len = 1184;
const uint8_t EVE_GT911_data[1184] PROGMEM =
{
	26,255,255,255,32,32,48,0,4,0,0,0,2,0,0,0,
	34,255,255,255,0,176,48,0,120,218,237,84,221,111,84,69,20,63,51,179,93,160,148,101,111,76,5,44,141,123,111,161,11,219,154,16,9,16,17,229,156,75,26,11,13,21,227,3,16,252,184,179,
	45,219,143,45,41,125,144,72,67,100,150,71,189,113,18,36,17,165,100,165,198,16,32,17,149,196,240,128,161,16,164,38,54,240,0,209,72,130,15,38,125,48,66,82,30,76,19,31,172,103,46,
	139,24,255,4,227,157,204,156,51,115,102,206,231,239,220,5,170,94,129,137,75,194,216,98,94,103,117,115,121,76,131,177,125,89,125,82,123,60,243,58,142,242,204,185,243,188,118,156,
	227,155,203,238,238,195,251,205,229,71,92,28,169,190,184,84,143,113,137,53,244,103,181,237,87,253,113,137,233,48,12,198,165,181,104,139,25,84,253,155,114,74,191,0,54,138,163,
	12,62,131,207,129,23,217,34,91,31,128,65,246,163,175,213,8,147,213,107,35,203,94,108,3,111,40,171,83,24,15,165,177,222,116,97,23,188,140,206,150,42,102,181,87,78,86,182,170,134,
	215,241,121,26,243,252,2,76,115,217,139,222,206,173,136,132,81,61,35,185,39,113,23,46,199,76,178,54,151,183,224,0,40,189,28,149,182,58,131,79,152,30,76,34,98,234,162,216,133,141,
	102,39,170,40,192,101,53,201,146,191,37,77,44,177,209,74,211,5,206,187,5,6,216,47,53,96,123,22,50,103,251,192,84,17,74,227,185,56,106,51,91,161,96,182,163,48,171,141,139,65,152,
	66,66,11,102,43,158,75,36,80,147,184,147,139,112,17,235,216,103,111,239,245,92,10,175,194,40,44,58,125,5,59,112,50,103,245,4,78,192,5,156,194,51,60,191,134,75,110,173,237,46,192,
	121,156,192,115,184,218,120,67,63,115,46,11,102,10,97,232,50,235,114,182,148,118,178,41,188,12,135,77,202,124,12,96,238,35,161,234,189,129,23,249,212,139,230,25,53,48,205,52,93,
	163,117,53,154,170,81,85,163,178,70,69,66,167,241,14,46,241,1,226,136,152,179,197,59,184,148,254,49,132,48,15,176,137,192,76,131,196,105,104,162,86,81,160,165,255,26,173,162,137,
	86,145,210,183,192,55,175,194,211,60,91,120,230,184,174,27,41,131,155,40,224,29,87,179,232,16,55,55,7,165,147,81,23,165,49,101,54,224,75,180,81,108,18,29,226,69,225,110,175,224,
	42,212,25,47,130,193,110,234,192,215,252,56,74,162,24,46,251,174,54,106,68,245,14,9,155,160,22,120,207,104,240,29,90,178,140,28,24,220,47,166,112,61,251,208,192,111,56,239,238,
	93,255,251,62,99,32,193,75,61,190,235,123,229,110,218,194,85,79,225,59,98,20,238,227,235,220,11,221,149,25,180,116,194,159,111,96,192,24,213,59,139,179,156,215,69,230,19,24,35,
	135,117,206,171,206,162,67,129,234,61,235,11,104,103,84,64,223,167,254,40,163,101,92,84,43,150,46,249,219,205,7,116,11,91,104,61,57,75,223,8,48,25,28,119,252,222,113,49,86,249,
	74,180,211,156,181,61,215,168,157,7,251,199,150,242,250,91,58,132,94,121,7,53,151,139,98,6,165,153,69,214,32,110,211,100,101,31,89,45,81,98,23,205,205,197,209,109,186,198,35,
	141,191,249,25,60,132,223,153,251,98,20,239,146,139,20,217,250,41,250,137,58,177,90,57,79,51,108,233,20,253,194,187,49,222,205,114,141,96,48,175,219,107,54,111,138,22,154,103,
	108,79,58,252,179,178,79,164,195,2,153,36,39,170,199,201,167,197,85,106,8,59,177,81,46,56,2,230,75,114,17,55,112,188,65,208,137,77,114,10,115,55,58,208,197,173,122,87,6,140,
	110,42,208,124,163,70,108,241,104,18,245,98,214,187,134,53,42,221,22,182,133,211,116,148,177,194,209,192,85,90,199,58,55,203,2,229,19,137,187,161,228,154,112,203,145,125,244,
	188,220,118,228,41,201,181,41,195,144,215,183,51,80,250,21,217,16,217,200,235,109,227,188,122,218,142,60,170,224,112,240,184,130,229,224,113,5,223,148,163,80,165,183,130,187,
	132,116,64,238,161,85,220,115,139,205,98,227,244,29,102,125,7,37,243,123,223,11,26,92,63,243,116,61,191,138,123,244,160,84,186,74,31,5,174,247,119,135,199,248,253,135,242,97,
	102,145,190,144,14,85,238,221,231,193,158,48,205,25,120,248,15,220,29,158,9,70,185,30,103,229,33,254,23,237,160,172,62,193,90,222,224,232,14,200,56,90,104,142,227,120,110,6,
	21,211,203,65,150,99,151,220,247,87,164,50,159,49,239,234,58,142,0,109,108,123,18,79,227,36,100,248,222,205,96,127,120,26,171,228,69,63,36,17,252,200,17,116,242,187,227,88,143,
	247,2,75,191,6,130,59,188,11,55,240,31,243,122,152,226,183,207,154,73,188,39,219,43,105,222,87,41,143,141,140,175,73,112,184,252,61,184,16,90,250,35,168,82,119,176,57,116,94,
	200,150,22,190,179,44,104,12,235,84,149,102,252,89,154,193,99,228,106,242,125,248,64,194,255,223,127,242,83,11,255,2,70,214,226,128,0,0
};
#endif
#endif // FT81X_FULL


/* init, has to be executed with the SPI setup to 11 MHz or less as required by FT8xx / BT8xx */
uint8_t EVE_init(void)
{
	uint8_t chipid = 0;
	uint16_t timeout = 0;

#if EVE_USE_PDN
	EVE_pdn_set();
	DELAY_MS(6); /* minimum time for power-down is 5ms */

	EVE_pdn_clear();
	DELAY_MS(21); /* minimum time to allow from rising PD_N to first access is 20ms */
#endif

	/*	EVE_cmdWrite(EVE_CORERST,0); */ /* reset, only required for warm-start if PowerDown line is not used */

#if defined (EVE_HAS_CRYSTAL)
	EVE_cmdWrite(EVE_CLKEXT, 0);	/* setup EVE for external clock */
#else
	EVE_cmdWrite(EVE_CLKINT, 0);	/* setup EVE for internal clock */
#endif

#if defined (BT81X_ENABLE)
	EVE_cmdWrite(EVE_CLKSEL ,0x46); /* set clock to 72 MHz */
#endif

	EVE_cmdWrite(EVE_ACTIVE, 0);	/* start EVE */

	/* BRT AN033 BT81X_Series_Programming_Guide V1.2 had a small change to chapter 2.4 "Initialization Sequence during Boot Up" */
	/* Send Host command “ACTIVE” and wait for at least 300 milliseconds. */
	/* Ensure that there is no SPI access during this time. */
	/* I asked Bridgetek for clarification why this has been made stricter. */
	/* From observation with quite a few of different displays I do not agree that either the 300ms are necessary or that */
	/* *reading* the SPI while EVE inits itself is causing any issues. */
	/* But since BT815 at 72MHz need 42ms anyways before they start to answer, here is my compromise, a fixed 40ms delay */
	/* to provide at least a short moment of silence for EVE */
	DELAY_MS(40);

	/* The most reliable DIO/QIO switching point is after EVE start up but before reading the ChipID. */
#if defined(DISP_SPI_TRANS_MODE_DIO)
	ESP_LOGI(TAG_LOG, "Switching to DIO mode");
	DELAY_MS(20);	/* different boards may take a different delay but this generally seems to work */
	EVE_memWrite16(REG_SPI_WIDTH, SPI_WIDTH_DIO);
	SPIInherentSendFlags = DISP_SPI_MODE_DIO | DISP_SPI_MODE_DIOQIO_ADDR;
	SPIDummyReadBits = 4;	/* Esp32 DMA SPI transaction dummy_bits works more like clock cycles, so in DIO 4 dummy_bits == 8 total bits */
#elif defined(DISP_SPI_TRANS_MODE_QIO)
	ESP_LOGI(TAG_LOG, "Switching to QIO mode");
	DELAY_MS(20);	/* different boards may take a different delay but this generally seems to work */
	EVE_memWrite16(REG_SPI_WIDTH, SPI_WIDTH_QIO);
	SPIInherentSendFlags = DISP_SPI_MODE_QIO | DISP_SPI_MODE_DIOQIO_ADDR;
	SPIDummyReadBits = 2;	/* Esp32 DMA SPI transaction dummy_bits works more like clock cycles, so in QIO 2 dummy_bits == 8 total bits */
#elif defined(DISP_SPI_HALF_DUPLEX)
	SPIDummyReadBits = 8;	/* SIO half-duplex mode */
#endif

	while(chipid != 0x7C) /* if chipid is not 0x7c, continue to read it until it is, EVE needs a moment for it's power on self-test and configuration */
	{
		DELAY_MS(1);
		chipid = EVE_memRead8(REG_ID);
		timeout++;
		if(timeout > 400)
		{
			ESP_LOGI(TAG_LOG, "Failed to read ChipID...aborting initialization.");
			return 0;
		}
	}

	timeout = 0;
	while (0x00 != (EVE_memRead8(REG_CPURESET) & 0x03)) /* check if EVE is in working status */
	{
		DELAY_MS(1);
		timeout++;
		if(timeout > 50) /* experimental, 10 was the lowest value to get the BT815 started with, the touch-controller was the last to get out of reset */
		{
			ESP_LOGI(TAG_LOG, "Failed to read CPU status...aborting initialization.");
			return 0;
		}
	}

	/* tell EVE that we changed the frequency from default to 72MHz for BT8xx */
#if defined (BT81X_ENABLE)
	EVE_memWrite32(REG_FREQUENCY, 72000000);
#endif

	/* we have a display with a Goodix GT911 / GT9271 touch-controller on it, so we patch our FT811 or FT813 according to AN_336 or setup a BT815 accordingly */
#if defined (EVE_HAS_GT911)

	#if defined (BT81X_ENABLE)
		EVE_memWrite32(REG_TOUCH_CONFIG, 0x000005d1); /* switch to Goodix touch controller */
	#else

		EVE_get_cmdoffset();

		BUFFER_SPI_WRITE_ADDRESS(EVE_RAM_CMD + cmdOffset)
		SEND_SPI_BUFFER()

		spi_flash_write(EVE_GT911_data, EVE_GT911_len);

		EVE_cmd_execute();

		EVE_memWrite8(REG_TOUCH_OVERSAMPLE, 0x0f); /* setup oversample to 0x0f as "hidden" in binary-blob for AN_336 */
		EVE_memWrite16(REG_TOUCH_CONFIG, 0x05D0); /* write magic cookie as requested by AN_336 */

		/* specific to the EVE2 modules from Matrix-Orbital we have to use GPIO3 to reset GT911 */
		EVE_memWrite16(REG_GPIOX_DIR, 0x8008); /* Reset-Value is 0x8000, adding 0x08 sets GPIO3 to output, default-value for REG_GPIOX is 0x8000 -> Low output on GPIO3 */
		DELAY_MS(1); /* wait more than 100µs */
		EVE_memWrite8(REG_CPURESET, 0x00); /* clear all resets */
		DELAY_MS(56); /* wait more than 55ms */
		EVE_memWrite16(REG_GPIOX_DIR, 0x8000); /* setting GPIO3 back to input */
	#endif
#endif

	/*	EVE_memWrite8(REG_PCLK, 0x00);	*/	/* set PCLK to zero - don't clock the LCD until later, line disabled because zero is reset-default and we just did a reset */

#if defined (EVE_ADAM101)
	EVE_memWrite8(REG_PWM_DUTY, 0x80); /* turn off backlight for Glyn ADAM101 module, it uses inverted values */
#else
	EVE_memWrite8(REG_PWM_DUTY, 0); /* turn off backlight for any other module */
#endif

	/* Initialize Display */
	EVE_memWrite16(REG_HSIZE,   EVE_HSIZE);   /* active display width */
	EVE_memWrite16(REG_HCYCLE,  EVE_HCYCLE);  /* total number of clocks per line, incl front/back porch */
	EVE_memWrite16(REG_HOFFSET, EVE_HOFFSET); /* start of active line */
	EVE_memWrite16(REG_HSYNC0,  EVE_HSYNC0);  /* start of horizontal sync pulse */
	EVE_memWrite16(REG_HSYNC1,  EVE_HSYNC1);  /* end of horizontal sync pulse */
	EVE_memWrite16(REG_VSIZE,   EVE_VSIZE);   /* active display height */
	EVE_memWrite16(REG_VCYCLE,  EVE_VCYCLE);  /* total number of lines per screen, including pre/post */
	EVE_memWrite16(REG_VOFFSET, EVE_VOFFSET); /* start of active screen */
	EVE_memWrite16(REG_VSYNC0,  EVE_VSYNC0);  /* start of vertical sync pulse */
	EVE_memWrite16(REG_VSYNC1,  EVE_VSYNC1);  /* end of vertical sync pulse */
	EVE_memWrite8(REG_SWIZZLE,  EVE_SWIZZLE); /* FT8xx output to LCD - pin order */
	EVE_memWrite8(REG_PCLK_POL, EVE_PCLKPOL); /* LCD data is clocked in on this PCLK edge */
	EVE_memWrite8(REG_CSPREAD,	EVE_CSPREAD); /* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */

	/* do not set PCLK yet - wait for just after the first display list */

	/* configure Touch */
	EVE_memWrite8(REG_TOUCH_MODE, EVE_TMODE_CONTINUOUS); /* enable touch */
	EVE_memWrite16(REG_TOUCH_RZTHRESH, EVE_TOUCH_RZTHRESH);	/* eliminate any false touches */

	/* disable Audio for now */
	EVE_memWrite8(REG_VOL_PB, 0x00); /* turn recorded audio volume down */
	EVE_memWrite8(REG_VOL_SOUND, 0x00); /* turn synthesizer volume off */
	EVE_memWrite16(REG_SOUND, 0x6000); /* set synthesizer to mute */

	/* write a basic display-list to get things started */
	EVE_memWrite32(EVE_RAM_DL, DL_CLEAR_RGB);
	EVE_memWrite32(EVE_RAM_DL + 4, (DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG));
	EVE_memWrite32(EVE_RAM_DL + 8, DL_DISPLAY);	/* end of display list */
	EVE_memWrite32(REG_DLSWAP, EVE_DLSWAP_FRAME);

	/* nothing is being displayed yet... the pixel clock is still 0x00 */
	EVE_memWrite8(REG_GPIO, 0x80); /* enable the DISP signal to the LCD panel, it is set to output in REG_GPIO_DIR by default */
	EVE_memWrite8(REG_PCLK, EVE_PCLK); /* now start clocking data to the LCD panel */

#if defined (EVE_ADAM101)
	EVE_memWrite8(REG_PWM_DUTY, 0x60); /* turn on backlight to 25% for Glyn ADAM101 module, it uses inverted values */
#else
	EVE_memWrite8(REG_PWM_DUTY, 0x20); /* turn on backlight to 25% for any other module */
#endif

	timeout = 0;
	while(EVE_busy()) /* just to be safe, should not even enter the loop */
	{
		DELAY_MS(1);
		timeout++;
		if(timeout > 4)
		{
			break; /* something is wrong here, but since we made it this far through the init, just leave the loop */
		}
	}

	EVE_get_cmdoffset(); /* just to be safe */

#if defined (EVE_DMA)
	EVE_init_dma(); /* prepare DMA */
#endif

	return 1;
}


/*
These eliminate the overhead of transmitting the command-fifo address with every single command, just wrap a sequence of commands
with these and the address is only transmitted once at the start of the block.
Be careful to not use any functions in the sequence that do not address the command-fifo as for example any EVE_mem...() function.
*/
void EVE_start_cmd_burst(void)
{
	cmd_burst = 42;

	WAIT_SPI()	// it is important to wait before writing to the SPI buffer as it might be in a DMA transaction
	BUFFER_SPI_WRITE_ADDRESS(EVE_RAM_CMD + cmdOffset)
}


void EVE_end_cmd_burst(void)
{
	cmd_burst = 0;

	SEND_SPI_BUFFER()
}


/* begin a co-processor command */
void EVE_start_cmd(uint32_t command)
{
	if(!cmd_burst)
	{
		WAIT_SPI()	// it is important to wait before writing to the SPI buffer as it might be in a DMA transaction
		BUFFER_SPI_WRITE_ADDRESS(EVE_RAM_CMD + cmdOffset)
	}

	BUFFER_SPI_DWORD(command)

	EVE_inc_cmdoffset(4); /* update the command-ram pointer */
}


/* generic function for all commands that have no arguments and all display-list specific control words */
/*
 examples:
 EVE_cmd_dl(CMD_DLSTART);
 EVE_cmd_dl(CMD_SWAP);
 EVE_cmd_dl(CMD_SCREENSAVER);
 EVE_cmd_dl(LINE_WIDTH(1*16));
 EVE_cmd_dl(VERTEX2F(0,0));
 EVE_cmd_dl(DL_BEGIN | EVE_RECTS);
*/
void EVE_cmd_dl(uint32_t command)
{
	if(cmd_burst)
	{
		BUFFER_SPI_DWORD(command)

		EVE_inc_cmdoffset(4); /* update the command-ram pointer */
	}
	else
	{
		EVE_start_cmd(command);
		SEND_SPI_BUFFER()
	}
}

#if FT81X_FULL
/* write a string to co-processor memory in context of a command: no chip-select, just plain SPI-transfers */
/* note: assumes cmdOffset is already DWORD aligned */
void EVE_write_string(const char *text)
{
	uint8_t textindex = 0;
	uint8_t padding = 0;
	uint8_t *bytes = (uint8_t *) text; /* need to handle the array as bunch of bytes */

	while(bytes[textindex] != 0)
	{
		BUFFER_SPI_BYTE(bytes[textindex]);
		textindex++;
		if(textindex > 249 || SPIBufferIndex >= SPI_BUFFER_SIZE) /* there appears to be no end for the "string", or no more room in SPI buffer, so leave */
		{
			break;
		}
	}

	/* we need to transmit at least one 0x00 byte and up to four if the string happens to be 4-byte aligned already */
	padding = textindex & 3;  /* 0, 1, 2 or 3 */
	padding = 4-padding; /* 4, 3, 2 or 1 */
	textindex += padding;

	while(padding > 0 && SPIBufferIndex < SPI_BUFFER_SIZE)
	{
		BUFFER_SPI_BYTE(0);
		padding--;
	}

	EVE_inc_cmdoffset(textindex);
}


/* EVE3 FLASH functions */
#if defined (BT81X_ENABLE)

/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* write "num" bytes from *data to the external flash on a BT81x board at address ptr */
/* note: ptr must be 256 byte aligned, num must be a multiple of 256 */
/* note: EVE will not do anything if the alignment requirements are not met */
/* note: the address ptr is relative to the flash so the first address is 0x00000000 not 0x800000 */
/* note: on AVR controllers this expects the data to be located in the controllers flash memory */
void EVE_cmd_flashwrite(uint32_t ptr, uint32_t num, const uint8_t *data)
{
	EVE_begin_cmd(CMD_FLASHWRITE);
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(8);

	SEND_SPI_BUFFER()

	WAIT_SPI()
	block_transfer(data, num);
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* write "num" bytes from src in the external flash on a BT81x board to dest in RAM_G */
/* note: src must be 64-byte aligned, dest must be 4-byte aligned, num must be a multiple of 4 */
/* note: EVE will not do anything if the alignment requirements are not met */
/* note: the src pointer is relative to the flash so the first address is 0x00000000 not 0x800000 */
void EVE_cmd_flashread(uint32_t dest, uint32_t src, uint32_t num)
{
	EVE_begin_cmd(CMD_FLASHREAD);
	BUFFER_SPI_DWORD(dest)
	BUFFER_SPI_DWORD(src)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(12);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* write "num" bytes from src in RAM_G to to the external flash on a BT81x board at address dest */
/* note: dest must be 4096-byte aligned, src must be 4-byte aligned, num must be a multiple of 4096 */
/* note: EVE will not do anything if the alignment requirements are not met */
/* note: the address ptr is relative to the flash so the first address is 0x00000000 not 0x800000 */
void EVE_cmd_flashupdate(uint32_t dest, uint32_t src, uint32_t num)
{
	EVE_begin_cmd(CMD_FLASHUPDATE);
	BUFFER_SPI_DWORD(dest)
	BUFFER_SPI_DWORD(src)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(12);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* this is added for conveniance, using EVE_cmd_dl(CMD_FLASHERASE); followed by EVE_cmd_execute(); would work as well */
void EVE_cmd_flasherase(void)
{
	EVE_begin_cmd(CMD_FLASHERASE);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* this is added for conveniance, using EVE_cmd_dl(CMD_FLASHATTACH); followed by EVE_cmd_execute(); would work as well */
void EVE_cmd_flashattach(void)
{
	EVE_begin_cmd(CMD_FLASHATTACH);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* this is added for conveniance, using EVE_cmd_dl(CMD_FLASHDETACH); followed by EVE_cmd_execute(); would work as well */
void EVE_cmd_flashdetach(void)
{
	EVE_begin_cmd(CMD_FLASHDETACH);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* this is added for conveniance, using EVE_cmd_dl(CMD_FLASHSPIDESEL); followed by EVE_cmd_execute(); would work as well */
void EVE_cmd_flashspidesel(void)
{
	EVE_begin_cmd(CMD_FLASHSPIDESEL);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
uint32_t EVE_cmd_flashfast(void)
{
	uint16_t offset;

	EVE_begin_cmd(CMD_FLASHFAST);
	BUFFER_SPI_DWORD(0)

	offset = cmdOffset;
	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();

	return EVE_memRead32(EVE_RAM_CMD + offset);
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* write "num" bytes from *data to the BT81x SPI interface */
/* note: raw direct access, not really useful for anything */
void EVE_cmd_flashspitx(uint32_t num, const uint8_t *data)
{
	EVE_begin_cmd(CMD_FLASHSPITX);
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()

	WAIT_SPI()
	block_transfer(data, num);
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
/* write "num" bytes from the BT81x SPI interface dest in RAM_G */
/* note: raw direct access, not really useful for anything */
void EVE_cmd_flashspirx(uint32_t dest, uint32_t num)
{
	EVE_begin_cmd(CMD_FLASHREAD);
	BUFFER_SPI_DWORD(dest)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(8);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();
}


/* this is meant to be called outside display-list building, it includes executing the command and waiting for completion, does not support cmd-burst */
void EVE_cmd_flashsource(uint32_t ptr)
{
	EVE_begin_cmd(CMD_FLASHSOURCE);
	BUFFER_SPI_DWORD(ptr)

	EVE_inc_cmdoffset(4);

	SEND_SPI_BUFFER()

	EVE_cmd_execute();
}


/* switch the FLASH attached to a BT815/BT816 to full-speed mode, returns 0 for failing to do so, does not support cmd-burst */
uint8_t EVE_init_flash(void)
{
	uint8_t timeout = 0;
	uint8_t status;

	status = EVE_memRead8(REG_FLASH_STATUS); /* should be 0x02 - FLASH_STATUS_BASIC, power-up is done and the attached flash is detected */

	while(status == 0) /* FLASH_STATUS_INIT - we are somehow still in init, give it a litte more time, this should never happen */
	{
		status = EVE_memRead8(REG_FLASH_STATUS);
		DELAY_MS(1);
		timeout++;
		if(timeout > 100) /* 100ms and still in init, lets call quits now and exit with an error */
		{
			return 0;
		}
	}

	if(status == 1) /* FLASH_STATUS_DETACHED - no flash was found during init, no flash present or the detection failed, but have hope and let the BT81x have annother try */
	{
		EVE_cmd_dl(CMD_FLASHATTACH);
		EVE_cmd_execute();
		status = EVE_memRead8(REG_FLASH_STATUS);
		if(status != 2) /* still not in FLASH_STATUS_BASIC, time to give up */
		{
			return 0;
		}
	}

	if(status == 2) /* FLASH_STATUS_BASIC - flash detected and ready for action, lets move it up to FLASH_STATUS_FULL */
	{
		uint32_t result;

		result = EVE_cmd_flashfast();

		if(result == 0) /* cmd_flashfast was successful */
		{
			return 1;
		}
		else /* room for improvement, cmd_flashfast provided an error code but there is no way to return it without returning a value that is FALSE all the same */
		{
			return 0;
		}
	}

	if(status == 3) /* FLASH_STATUS_FULL - we are already there, why has this function been called? */
	{
		return 1;
	}

	return 0;
}
#endif


/* commands to draw graphics objects: */


#if defined (BT81X_ENABLE)
/* as the name implies, "num_args" is the number of arguments passed to this function as variadic arguments */
void EVE_cmd_text_var(int16_t x0, int16_t y0, int16_t font, uint16_t options, const char* text, uint8_t num_args, ...)
{
	va_list arguments;
	uint8_t counter;
	uint32_t data;

	va_start(arguments, num_args);

	BUFFER_SPI_DWORD(CMD_TEXT)
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(font)
	BUFFER_SPI_WORD(options)

	EVE_inc_cmdoffset(12);

	EVE_write_string(text);

	if(options & EVE_OPT_FORMAT)
	{
		for(counter = 0; counter < num_args; counter++)
		{
			data = (uint32_t) va_arg(arguments, int);
			BUFFER_SPI_DWORD(data)
			EVE_inc_cmdoffset(4);
		}
	}

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}

	va_end(arguments);
}
#endif


void EVE_cmd_text(int16_t x0, int16_t y0, int16_t font, uint16_t options, const char* text)
{
	EVE_start_cmd(CMD_TEXT);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(font)
	BUFFER_SPI_WORD(options)

	EVE_inc_cmdoffset(8);

	EVE_write_string(text);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


#if defined (BT81X_ENABLE)
/* as the name implies, "num_args" is the number of arguments passed to this function as variadic arguments */
void EVE_cmd_button_var(int16_t x0, int16_t y0, int16_t w0, int16_t h0, int16_t font, uint16_t options, const char* text, uint8_t num_args, ...)
{
	va_list arguments;
	uint8_t counter;
	uint32_t data;

	va_start(arguments, num_args);

	EVE_start_cmd(CMD_BUTTON);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(w0)
	BUFFER_SPI_WORD(h0)
	BUFFER_SPI_WORD(font)
	BUFFER_SPI_WORD(options)

	EVE_inc_cmdoffset(12);

	EVE_write_string(text);

	if(options & EVE_OPT_FORMAT)
	{
		for(counter = 0; counter < num_args; counter++)
		{
			data = (uint32_t) va_arg(arguments, int);
			BUFFER_SPI_DWORD(data)
			EVE_inc_cmdoffset(4);
		}
	}

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}

	va_end(arguments);
}
#endif


void EVE_cmd_button(int16_t x0, int16_t y0, int16_t w0, int16_t h0, int16_t font, uint16_t options, const char* text)
{
	EVE_start_cmd(CMD_BUTTON);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(w0)
	BUFFER_SPI_WORD(h0)
	BUFFER_SPI_WORD(font)
	BUFFER_SPI_WORD(options)

	EVE_inc_cmdoffset(12);

	EVE_write_string(text);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


/* draw a clock */
void EVE_cmd_clock(int16_t x0, int16_t y0, int16_t r0, uint16_t options, uint16_t hours, uint16_t minutes, uint16_t seconds, uint16_t millisecs)
{
	EVE_start_cmd(CMD_CLOCK);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(r0)
	BUFFER_SPI_WORD(options)
	BUFFER_SPI_WORD(hours)
	BUFFER_SPI_WORD(minutes)
	BUFFER_SPI_WORD(seconds)
	BUFFER_SPI_WORD(millisecs)

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_color_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
	BUFFER_SPI_BYTE(green)	/* low-byte */
	BUFFER_SPI_BYTE(blue)
	BUFFER_SPI_BYTE(red)
	BUFFER_SPI_BYTE(0x04)	/* encoding for COLOR_RGB */

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_bgcolor(uint32_t color)
{
	EVE_start_cmd(CMD_BGCOLOR);
	BUFFER_SPI_DWORD(color & 0x00ffffff)

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_fgcolor(uint32_t color)
{
	EVE_start_cmd(CMD_FGCOLOR);
	BUFFER_SPI_DWORD(color & 0x00ffffff)

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_gradcolor(uint32_t color)
{
	EVE_start_cmd(CMD_GRADCOLOR);
	BUFFER_SPI_DWORD(color & 0x00ffffff)

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_gauge(int16_t x0, int16_t y0, int16_t r0, uint16_t options, uint16_t major, uint16_t minor, uint16_t val, uint16_t range)
{
	EVE_start_cmd(CMD_GAUGE);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(r0)
	BUFFER_SPI_WORD(options)
	BUFFER_SPI_WORD(major)
	BUFFER_SPI_WORD(minor)
	BUFFER_SPI_WORD(val)
	BUFFER_SPI_WORD(range)

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_gradient(int16_t x0, int16_t y0, uint32_t rgb0, int16_t x1, int16_t y1, uint32_t rgb1)
{
	EVE_start_cmd(CMD_GRADIENT);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_DWORD(rgb0 & 0x00ffffff)
	BUFFER_SPI_WORD(x1)
	BUFFER_SPI_WORD(y1)
	BUFFER_SPI_DWORD(rgb1 & 0x00ffffff)

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_keys(int16_t x0, int16_t y0, int16_t w0, int16_t h0, int16_t font, uint16_t options, const char* text)
{
	EVE_start_cmd(CMD_KEYS);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(w0)
	BUFFER_SPI_WORD(h0)
	BUFFER_SPI_WORD(font)
	BUFFER_SPI_WORD(options)

	EVE_inc_cmdoffset(12);

	EVE_write_string(text);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_progress(int16_t x0, int16_t y0, int16_t w0, int16_t h0, uint16_t options, uint16_t val, uint16_t range)
{
	EVE_start_cmd(CMD_PROGRESS);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(w0)
	BUFFER_SPI_WORD(h0)
	BUFFER_SPI_WORD(options)
	BUFFER_SPI_WORD(val)
	BUFFER_SPI_WORD(range)
	BUFFER_SPI_WORD(0)		/* dummy word for 4-byte alignment */

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_scrollbar(int16_t x0, int16_t y0, int16_t w0, int16_t h0, uint16_t options, uint16_t val, uint16_t size, uint16_t range)
{
	EVE_start_cmd(CMD_SCROLLBAR);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(w0)
	BUFFER_SPI_WORD(h0)
	BUFFER_SPI_WORD(options)
	BUFFER_SPI_WORD(val)
	BUFFER_SPI_WORD(size)
	BUFFER_SPI_WORD(range)

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_slider(int16_t x1, int16_t y1, int16_t w1, int16_t h1, uint16_t options, uint16_t val, uint16_t range)
{
	EVE_start_cmd(CMD_SLIDER);
	BUFFER_SPI_WORD(x1)
	BUFFER_SPI_WORD(y1)
	BUFFER_SPI_WORD(w1)
	BUFFER_SPI_WORD(h1)
	BUFFER_SPI_WORD(options)
	BUFFER_SPI_WORD(val)
	BUFFER_SPI_WORD(range)
	BUFFER_SPI_WORD(0)		/* dummy word for 4-byte alignment */

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_dial(int16_t x0, int16_t y0, int16_t r0, uint16_t options, uint16_t val)
{
	EVE_start_cmd(CMD_DIAL);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(r0)
	BUFFER_SPI_WORD(options)
	BUFFER_SPI_WORD(val)
	BUFFER_SPI_WORD(0)		/* dummy word for 4-byte alignment */

	EVE_inc_cmdoffset(12);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


#if defined (BT81X_ENABLE)
/* as the name implies, "num_args" is the number of arguments passed to this function as variadic arguments */
void EVE_cmd_toggle_var(int16_t x0, int16_t y0, int16_t w0, int16_t font, uint16_t options, uint16_t state, const char* text, uint8_t num_args, ...)
{
	va_list arguments;
	uint8_t counter;
	uint32_t data;

	va_start(arguments, num_args);

	EVE_start_cmd(CMD_TOGGLE);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(w0)
	BUFFER_SPI_WORD(font)
	BUFFER_SPI_WORD(options)
	BUFFER_SPI_WORD(state)

	EVE_inc_cmdoffset(12);

	EVE_write_string(text);

	if(options & EVE_OPT_FORMAT)
	{
		for(counter = 0; counter < num_args; counter++)
		{
			data = (uint32_t) va_arg(arguments, int);
			BUFFER_SPI_DWORD(data)
			EVE_inc_cmdoffset(4);
		}
	}

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}

	va_end(arguments);
}
#endif


void EVE_cmd_toggle(int16_t x0, int16_t y0, int16_t w0, int16_t font, uint16_t options, uint16_t state, const char* text)
{
	EVE_start_cmd(CMD_TOGGLE);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(font)
	BUFFER_SPI_WORD(options)
	BUFFER_SPI_WORD(state)

	EVE_inc_cmdoffset(12);

	EVE_write_string(text);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


#if defined (FT81X_ENABLE)
void EVE_cmd_setbase(uint32_t base)
{
	EVE_start_cmd(CMD_SETBASE);
	BUFFER_SPI_DWORD(base);

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}
#endif
#endif // FT81X_FULL


#if defined (FT81X_ENABLE)
void EVE_cmd_setbitmap(uint32_t addr, uint16_t fmt, uint16_t width, uint16_t height)
{
	EVE_start_cmd(CMD_SETBITMAP);
	BUFFER_SPI_DWORD(addr)
	BUFFER_SPI_WORD(fmt)
	BUFFER_SPI_WORD(width)
	BUFFER_SPI_WORD(height)
	BUFFER_SPI_WORD(0)

	EVE_inc_cmdoffset(12);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}

}
#endif

#if FT81X_FULL
void EVE_cmd_number(int16_t x0, int16_t y0, int16_t font, uint16_t options, int32_t number)
{
	EVE_start_cmd(CMD_NUMBER);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(font)
	BUFFER_SPI_WORD(options)
	BUFFER_SPI_DWORD(number)

	EVE_inc_cmdoffset(12);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_append(uint32_t ptr, uint32_t num)
{
	EVE_start_cmd(CMD_APPEND);
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(8);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


/* commands for setting the bitmap transform matrix: */

/*
 The description in the programmers guide is strange for this function.
 While it is named *get*matrix, parameters 'a' to 'f' are supplied to the function
 and described as "output parameter"
 Best guess is that this one allows to setup the matrix coefficients manually.
 If this assumption is correct it rather should be named cmd_setupmatrix().
*/
void EVE_cmd_getmatrix(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
	EVE_start_cmd(CMD_GETMATRIX);
	BUFFER_SPI_DWORD(a)
	BUFFER_SPI_DWORD(b)
	BUFFER_SPI_DWORD(c)
	BUFFER_SPI_DWORD(d)
	BUFFER_SPI_DWORD(e)
	BUFFER_SPI_DWORD(f)

	EVE_inc_cmdoffset(24);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_translate(int32_t tx, int32_t ty)
{
	EVE_start_cmd(CMD_TRANSLATE);
	BUFFER_SPI_DWORD(tx)
	BUFFER_SPI_DWORD(ty)

	EVE_inc_cmdoffset(8);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_scale(int32_t sx, int32_t sy)
{
	EVE_start_cmd(CMD_SCALE);
	BUFFER_SPI_DWORD(sx)
	BUFFER_SPI_DWORD(sy)

	EVE_inc_cmdoffset(8);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_rotate(int32_t ang)
{
	EVE_start_cmd(CMD_ROTATE);
	BUFFER_SPI_DWORD(ang)

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


#if defined (BT81X_ENABLE)
void EVE_cmd_rotatearound(int32_t x0, int32_t y0, int32_t angle, int32_t scale)
{
	EVE_start_cmd(CMD_ROTATEAROUND);
	BUFFER_SPI_DWORD(x0)
	BUFFER_SPI_DWORD(y0)
	BUFFER_SPI_DWORD(ang)
	BUFFER_SPI_DWORD(scale)

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}
#endif


/* other commands: */
void EVE_cmd_calibrate(void)
{
	EVE_start_cmd(CMD_CALIBRATE);
	BUFFER_SPI_DWORD(0)

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


#if defined (FT81X_ENABLE)
void EVE_cmd_romfont(uint32_t font, uint32_t romslot)
{
	EVE_start_cmd(CMD_ROMFONT);
	BUFFER_SPI_DWORD(font & 0x0000ffff)
	BUFFER_SPI_DWORD(romslot & 0x0000ffff)

	EVE_inc_cmdoffset(8);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}
#endif


#if defined (FT81X_ENABLE)
void EVE_cmd_setscratch(uint32_t handle)
{
	EVE_start_cmd(CMD_SETSCRATCH);
	BUFFER_SPI_DWORD(handle)

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}
#endif


void EVE_cmd_sketch(int16_t x0, int16_t y0, uint16_t w0, uint16_t h0, uint32_t ptr, uint16_t format)
{
	EVE_start_cmd(CMD_SKETCH);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(w0)
	BUFFER_SPI_WORD(h0)
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_WORD(format)
	BUFFER_SPI_WORD(0)		/* dummy word for 4-byte alignment */

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_spinner(int16_t x0, int16_t y0, uint16_t style, uint16_t scale)
{
	EVE_start_cmd(CMD_SPINNER);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_WORD(style)
	BUFFER_SPI_WORD(scale)

	EVE_inc_cmdoffset(8);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


/* various commands new for EVE3 */
#if defined (BT81X_ENABLE)

void EVE_cmd_animstart(int32_t ch, uint32_t aoptr, uint32_t loop)
{
	EVE_start_cmd(CMD_ANIMSTART);
	BUFFER_SPI_DWORD(ch)
	BUFFER_SPI_DWORD(aoptr)
	BUFFER_SPI_DWORD(loop)

	EVE_inc_cmdoffset(12);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_animstop(int32_t ch)
{
	EVE_start_cmd(CMD_ANIMSTOP);
	BUFFER_SPI_DWORD(ch)

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_animxy(int32_t ch, int16_t x0, int16_t y0)
{
	EVE_start_cmd(CMD_ANIMXY);
	BUFFER_SPI_DWORD(ch)
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)

	EVE_inc_cmdoffset(8);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_animdraw(int32_t ch)
{
	EVE_start_cmd(CMD_ANIMDRAW);
	BUFFER_SPI_DWORD(ch)

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_animframe(int16_t x0, int16_t y0, uint32_t aoptr, uint32_t frame)
{
	EVE_start_cmd(CMD_ANIMFRAME);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_DWORD(aoptr)
	BUFFER_SPI_DWORD(frame)

	EVE_inc_cmdoffset(12);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_gradienta(int16_t x0, int16_t y0, uint32_t argb0, int16_t x1, int16_t y1, uint32_t argb1)
{
	EVE_start_cmd(CMD_GRADIENTA);
	BUFFER_SPI_WORD(x0)
	BUFFER_SPI_WORD(y0)
	BUFFER_SPI_DWORD(argb0)
	BUFFER_SPI_WORD(x1)
	BUFFER_SPI_WORD(y1)
	BUFFER_SPI_DWORD(argb1)

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_fillwidth(uint32_t s)
{
	EVE_start_cmd(CMD_FILLWIDTH);
	BUFFER_SPI_DWORD(s)

	EVE_inc_cmdoffset(4);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


void EVE_cmd_appendf(uint32_t ptr, uint32_t num)
{
	EVE_start_cmd(CMD_APPENDF);
	BUFFER_SPI_DWORD(ptr)
	BUFFER_SPI_DWORD(num)

	EVE_inc_cmdoffset(8);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


#endif


/* warning! meta-command! this is a sequence of display-list commands to simplify use at the price of some overhead */
void EVE_cmd_point(int16_t x0, int16_t y0, uint16_t size)
{
	EVE_start_cmd((DL_BEGIN | EVE_POINTS));

	uint32_t calc = POINT_SIZE(size*16);
	BUFFER_SPI_DWORD(calc)

	calc = VERTEX2F(x0 * 16, y0 * 16);
	BUFFER_SPI_DWORD(calc)

	BUFFER_SPI_DWORD(DL_END)

	EVE_inc_cmdoffset(12);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


/* warning! meta-command! this is a sequence of display-list commands to simplify use at the price of some overhead */
void EVE_cmd_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t width)
{
	EVE_start_cmd((DL_BEGIN | EVE_LINES));

	uint32_t calc = LINE_WIDTH(width * 16);
	BUFFER_SPI_DWORD(calc)

	calc = VERTEX2F(x0 * 16, y0 * 16);
	BUFFER_SPI_DWORD(calc)

	calc = VERTEX2F(x1 * 16, y1 * 16);
	BUFFER_SPI_DWORD(calc)

	BUFFER_SPI_DWORD(DL_END)

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


/* warning! meta-command! this is a sequence of display-list commands to simplify use at the price of some overhead */
void EVE_cmd_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t corner)
{
	EVE_start_cmd((DL_BEGIN | EVE_RECTS));

	uint32_t calc = LINE_WIDTH(corner * 16);
	BUFFER_SPI_DWORD(calc)

	calc = VERTEX2F(x0 * 16, y0 * 16);
	BUFFER_SPI_DWORD(calc)

	calc = VERTEX2F(x1 * 16, y1 * 16);
	BUFFER_SPI_DWORD(calc)

	BUFFER_SPI_DWORD(DL_END)

	EVE_inc_cmdoffset(16);

	if(!cmd_burst)
	{
		SEND_SPI_BUFFER()
	}
}


/* this is meant to be called outside display-list building */
/* this function displays an interactive calibration screen, calculates the calibration values and */
/* writes the new values to the touch matrix registers of EVE */
/* unlike the built-in cmd_calibrate() of EVE this also works with displays that are cut down from larger ones like EVE2-38A / EVE2-38G */
/* the height is needed as parameter as EVE_VSIZE for the EVE2-38 is 272 but the visible size is only 116 */
/* so the call would be EVE_calibrate_manual(116); for the EVE2-38A and EVE2-38G while for most other displays */
/* using EVE_calibrate_manual(EVE_VSIZE) would work - but for normal displays the built-in cmd_calibrate would work as expected anyways */
/* this code was taken from the MatrixOrbital EVE2-Library on Github, adapted and modified */
void EVE_calibrate_manual(uint16_t height)
{
	uint32_t displayX[3], displayY[3];
	uint32_t touchX[3], touchY[3];
	uint32_t touchValue;
	int32_t tmp, k;
	int32_t TransMatrix[6];
	uint8_t count = 0;
	char num[2];
	uint8_t touch_lock = 1;

	/* these values determine where your calibration points will be drawn on your display */
	displayX[0] = (EVE_HSIZE * 0.15);
	displayY[0] = (height * 0.15);

	displayX[1] = (EVE_HSIZE * 0.85);
	displayY[1] = (height / 2);

	displayX[2] = (EVE_HSIZE / 2);
	displayY[2] = (height * 0.85);

	while (count < 3)
	{
		// immediate transfer mode
		EVE_cmd_dl(CMD_DLSTART);
		EVE_cmd_dl(DL_CLEAR_RGB | 0x000000);
		EVE_cmd_dl(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);

		/* draw Calibration Point on screen */
		EVE_cmd_dl(DL_COLOR_RGB | 0x0000ff);
		EVE_cmd_dl(POINT_SIZE(20*16));
		EVE_cmd_dl((DL_BEGIN | EVE_POINTS));
		EVE_cmd_dl(VERTEX2F((uint32_t)(displayX[count]) * 16, (uint32_t)((displayY[count])) * 16));
		EVE_cmd_dl(DL_END);
		EVE_cmd_dl(DL_COLOR_RGB | 0xffffff);
		EVE_cmd_text((EVE_HSIZE/2), 50, 27, EVE_OPT_CENTER, "Please tap on the dot.");
		num[0] = count + 0x31; num[1] = 0; /* null terminated string of one character */
		EVE_cmd_text(displayX[count], displayY[count], 27, EVE_OPT_CENTER, num);

		EVE_cmd_dl(DL_DISPLAY);
		EVE_cmd_dl(CMD_SWAP);
		EVE_cmd_execute();

		while(1)
		{
			touchValue = EVE_memRead32(REG_TOUCH_DIRECT_XY); /* read for any new touch tag inputs */

			if(touch_lock)
			{
				if(touchValue & 0x80000000) /* check if we have no touch */
				{
					touch_lock = 0;
				}
			}
			else
			{
				if (!(touchValue & 0x80000000)) /* check if a touch is detected */
				{
					touchX[count] = (touchValue>>16) & 0x03FF; /* raw Touchscreen Y coordinate */
					touchY[count] = touchValue & 0x03FF; /* raw Touchscreen Y coordinate */
					touch_lock = 1;
					count++;
					break; /* leave while(1) */
				}
			}
		}
	}

	k = ((touchX[0] - touchX[2])*(touchY[1] - touchY[2])) - ((touchX[1] - touchX[2])*(touchY[0] - touchY[2]));

	tmp = (((displayX[0] - displayX[2]) * (touchY[1] - touchY[2])) - ((displayX[1] - displayX[2])*(touchY[0] - touchY[2])));
	TransMatrix[0] = ((int64_t)tmp << 16) / k;

	tmp = (((touchX[0] - touchX[2]) * (displayX[1] - displayX[2])) - ((displayX[0] - displayX[2])*(touchX[1] - touchX[2])));
	TransMatrix[1] = ((int64_t)tmp << 16) / k;

	tmp = ((touchY[0] * (((touchX[2] * displayX[1]) - (touchX[1] * displayX[2])))) + (touchY[1] * (((touchX[0] * displayX[2]) - (touchX[2] * displayX[0])))) + (touchY[2] * (((touchX[1] * displayX[0]) - (touchX[0] * displayX[1])))));
	TransMatrix[2] = ((int64_t)tmp << 16) / k;

	tmp = (((displayY[0] - displayY[2]) * (touchY[1] - touchY[2])) - ((displayY[1] - displayY[2])*(touchY[0] - touchY[2])));
	TransMatrix[3] = ((int64_t)tmp << 16) / k;

	tmp = (((touchX[0] - touchX[2]) * (displayY[1] - displayY[2])) - ((displayY[0] - displayY[2])*(touchX[1] - touchX[2])));
	TransMatrix[4] = ((int64_t)tmp << 16) / k;

	tmp = ((touchY[0] * (((touchX[2] * displayY[1]) - (touchX[1] * displayY[2])))) + (touchY[1] * (((touchX[0] * displayY[2]) - (touchX[2] * displayY[0])))) + (touchY[2] * (((touchX[1] * displayY[0]) - (touchX[0] * displayY[1])))));
	TransMatrix[5] = ((int64_t)tmp << 16) / k;

	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, TransMatrix[0]);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, TransMatrix[1]);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, TransMatrix[2]);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, TransMatrix[3]);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, TransMatrix[4]);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, TransMatrix[5]);
}
#endif // FT81X_FULL
