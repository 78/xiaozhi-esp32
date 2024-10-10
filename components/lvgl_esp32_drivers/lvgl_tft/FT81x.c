
#include <stdio.h>

#include "driver/gpio.h"

#include "FT81x.h"

#include "EVE.h"
#include "EVE_commands.h"

/* some pre-definded colors */
#define RED		0xff0000UL
#define ORANGE	0xffa500UL
#define GREEN	0x00ff00UL
#define BLUE	0x0000ffUL
#define BLUE_1	0x5dade2L
#define YELLOW	0xffff00UL
#define PINK	0xff00ffUL
#define PURPLE	0x800080UL
#define WHITE	0xffffffUL
#define BLACK	0x000000UL

/* memory-map defines */
#define SCREEN_BITMAP_ADDR	0x00000000	// full screen buffer (0x00000000 - 0x000‭‭BBE40‬)

uint8_t tft_active = 0;

void touch_calibrate(void)
{

/* send pre-recorded touch calibration values, depending on the display the code is compiled for */

#if defined (EVE_CFAF240400C1_030SC)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x0000ed11);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x00001139);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff76809);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x00000000);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00010690);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xfffadf2e);
#endif

#if defined (EVE_CFAF320240F_035T)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x00005614);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x0000009e);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff43422);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x0000001d);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0xffffbda4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x00f8f2ef);
#endif

#if defined (EVE_CFAF480128A0_039TC)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x00010485);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x0000017f);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfffb0bd3);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x00000073);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0000e293);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x00069904);
#endif

#if defined (EVE_CFAF800480E0_050SC)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000107f9);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0xffffff8c);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff451ae);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x000000d2);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0000feac);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xfffcfaaf);
#endif

#if defined (EVE_PAF90)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x00000159);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x0001019c);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff93625);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x00010157);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00000000);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x0000c101);
#endif

#if defined (EVE_RiTFT43)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000062cd);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0xfffffe45);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff45e0a);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x000001a3);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00005b33);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFFbb870);
#endif

#if defined (EVE_EVE2_38)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x00007bed);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x000001b0);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff60aa5);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x00000095);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0xffffdcda);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x00829c08);
#endif

#if defined (EVE_EVE2_35G)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000109E4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x000007A6);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xFFEC1EBA);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x0000072C);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0001096A);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFF469CF);
#endif

#if defined (EVE_EVE2_43G)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x0000a1ff);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x00000680);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xffe54cc2);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xffffff53);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0000912c);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xfffe628d);
#endif

#if defined (EVE_EVE2_50G)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000109E4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x000007A6);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xFFEC1EBA);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x0000072C);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0001096A);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFF469CF);
#endif

#if defined (EVE_EVE2_70G)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000105BC);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0xFFFFFA8A);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0x00004670);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xFFFFFF75);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00010074);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFFF14C8);
#endif

#if defined (EVE_NHD_35)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x0000f78b);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x00000427);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfffcedf8);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xfffffba4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0000f756);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x0009279e);
#endif

#if defined (EVE_RVT70)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000074df);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x000000e6);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfffd5474);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x000001af);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00007e79);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xffe9a63c);
#endif

#if defined (EVE_FT811CB_HY50HD)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 66353);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 712);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 4293876677);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 4294966157);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 67516);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 418276);
#endif

#if defined (EVE_ADAM101)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000101E3);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x00000114);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xFFF5EEBA);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xFFFFFF5E);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00010226);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x0000C783);
#endif

/* activate this if you are using a module for the first time or if you need to re-calibrate it */
/* write down the numbers on the screen and either place them in one of the pre-defined blocks above or make a new block */
// Note: requires FT81x_FULL to be defined
#if 0
	/* calibrate touch and displays values to screen */
	EVE_cmd_dl(CMD_DLSTART);
	EVE_cmd_dl(DL_CLEAR_RGB | BLACK);
	EVE_cmd_dl(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
	EVE_cmd_text((EVE_HSIZE/2), 50, 26, EVE_OPT_CENTER, "Please tap on the dot.");
	EVE_cmd_calibrate();
	EVE_cmd_dl(DL_DISPLAY);
	EVE_cmd_dl(CMD_SWAP);
	EVE_cmd_execute();

	uint32_t touch_a, touch_b, touch_c, touch_d, touch_e, touch_f;

	touch_a = EVE_memRead32(REG_TOUCH_TRANSFORM_A);
	touch_b = EVE_memRead32(REG_TOUCH_TRANSFORM_B);
	touch_c = EVE_memRead32(REG_TOUCH_TRANSFORM_C);
	touch_d = EVE_memRead32(REG_TOUCH_TRANSFORM_D);
	touch_e = EVE_memRead32(REG_TOUCH_TRANSFORM_E);
	touch_f = EVE_memRead32(REG_TOUCH_TRANSFORM_F);

	EVE_cmd_dl(CMD_DLSTART);
	EVE_cmd_dl(DL_CLEAR_RGB | BLACK);
	EVE_cmd_dl(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
	EVE_cmd_dl(TAG(0));

	EVE_cmd_text(5, 15, 26, 0, "TOUCH_TRANSFORM_A:");
	EVE_cmd_text(5, 30, 26, 0, "TOUCH_TRANSFORM_B:");
	EVE_cmd_text(5, 45, 26, 0, "TOUCH_TRANSFORM_C:");
	EVE_cmd_text(5, 60, 26, 0, "TOUCH_TRANSFORM_D:");
	EVE_cmd_text(5, 75, 26, 0, "TOUCH_TRANSFORM_E:");
	EVE_cmd_text(5, 90, 26, 0, "TOUCH_TRANSFORM_F:");

#if defined (FT81X_ENABLE)
	EVE_cmd_setbase(16L); /* FT81x only */
	EVE_cmd_number(310, 15, 26, EVE_OPT_RIGHTX|8, touch_a);
	EVE_cmd_number(310, 30, 26, EVE_OPT_RIGHTX|8, touch_b);
	EVE_cmd_number(310, 45, 26, EVE_OPT_RIGHTX|8, touch_c);
	EVE_cmd_number(310, 60, 26, EVE_OPT_RIGHTX|8, touch_d);
	EVE_cmd_number(310, 75, 26, EVE_OPT_RIGHTX|8, touch_e);
	EVE_cmd_number(310, 90, 26, EVE_OPT_RIGHTX|8, touch_f);
#else
	EVE_cmd_number(310, 15, 26, EVE_OPT_RIGHTX, touch_a);
	EVE_cmd_number(310, 30, 26, EVE_OPT_RIGHTX, touch_b);
	EVE_cmd_number(310, 45, 26, EVE_OPT_RIGHTX, touch_c);
	EVE_cmd_number(310, 60, 26, EVE_OPT_RIGHTX, touch_d);
	EVE_cmd_number(310, 75, 26, EVE_OPT_RIGHTX, touch_e);
	EVE_cmd_number(310, 90, 26, EVE_OPT_RIGHTX, touch_f);
#endif

	EVE_cmd_dl(DL_DISPLAY);	/* instruct the graphics processor to show the list */
	EVE_cmd_dl(CMD_SWAP); /* make this list active */
	EVE_cmd_execute();

	while(1);
#endif
}


// set up a display list for a fullscreen writable bitmap
void TFT_bitmap_display(void)
{
	if(tft_active != 0)
	{
		EVE_start_cmd_burst(); /* start writing to the cmd-fifo as one stream of bytes, only sending the address once */

		EVE_cmd_dl(CMD_DLSTART); /* start the display list */

		EVE_cmd_dl(DL_CLEAR_RGB | BLACK); /* set the default clear color to black */
		EVE_cmd_dl(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG); /* clear the screen - this and the previous prevent artifacts between lists, Attributes are the color, stencil and tag buffers */

		EVE_cmd_dl(TAG(0));

		// fullscreen bitmap for memory-mapped direct access
		EVE_cmd_dl(TAG(20));
		EVE_cmd_setbitmap(SCREEN_BITMAP_ADDR, EVE_RGB565, EVE_HSIZE, EVE_VSIZE);
		EVE_cmd_dl(DL_BEGIN | EVE_BITMAPS);
		EVE_cmd_dl(VERTEX2F(0, 0));
		EVE_cmd_dl(DL_END);

		EVE_cmd_dl(TAG(0));

		EVE_cmd_dl(DL_DISPLAY);	/* instruct the graphics processor to show the list */

		EVE_cmd_dl(CMD_SWAP); /* make this list active */

		EVE_end_cmd_burst(); /* stop writing to the cmd-fifo */

		EVE_cmd_start(); /* order the command co-processor to start processing its FIFO queue but do not wait for completion */
	}
}


void FT81x_init(void)
{
#if EVE_USE_PDN
	gpio_pad_select_gpio(EVE_PDN);
#endif

	gpio_set_level(EVE_CS, 1);

#if EVE_USE_PDN
	gpio_set_direction(EVE_PDN, GPIO_MODE_OUTPUT);
#endif

	spi_acquire();

	if(EVE_init())
	{
		tft_active = 1;

		EVE_memWrite8(REG_PWM_DUTY, 0x30);	/* setup backlight, range is from 0 = off to 0x80 = max */

		touch_calibrate();

		EVE_cmd_memset(SCREEN_BITMAP_ADDR, BLACK, SCREEN_BUFFER_SIZE);		// clear screen buffer
		EVE_cmd_execute();

		TFT_bitmap_display();	// set DL for fullscreen bitmap display
	}

	spi_release();
}


// write fullscreen bitmap directly
void TFT_WriteScreen(uint8_t* Bitmap)
{
	EVE_memWrite_buffer(SCREEN_BITMAP_ADDR, Bitmap, SCREEN_BUFFER_SIZE, false);
}


// write bitmap directly, line-by-line
void TFT_WriteBitmap(uint8_t* Bitmap, uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height)
{
	// calc base address
	uint32_t addr = SCREEN_BITMAP_ADDR + (Y * BYTES_PER_LINE) + (X * BYTES_PER_PIXEL);

	// can we do a fast full width block transfer?
	if(X == 0 && Width == EVE_HSIZE)
	{
		EVE_memWrite_buffer(addr, Bitmap, (Height * BYTES_PER_LINE), true);
	}
	else
	{
		// line by line mode
		uint32_t bpl = Width * BYTES_PER_PIXEL;
		for (uint16_t i = 0; i < Height; i++)
		{
			EVE_memWrite_buffer(addr, Bitmap + (i * bpl), bpl, (i == Height - 1));
			addr += BYTES_PER_LINE;
		}
	}
}

// LittlevGL flush callback
void FT81x_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
	TFT_WriteBitmap((uint8_t*)color_map, area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area));
}
