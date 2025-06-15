/* drivers/input/touchscreen/mediatek/gslX680/
 *
 * 2010 - 2016 silead inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the sileadinc's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
// #include "bsp/lcd_gsl3680.h"
#include "gsl_point_id.h"
#include "esp_log.h"
#include "stdio.h"

#define GSL_VERSION                                                            \
	0x20160901 /* NO GESTURE VERSION COME FROM VERSION 20150706 */

#ifndef NULL
#define NULL ((void *)0)
#endif
#ifndef UINT
#define UINT unsigned int
#endif

#define POINT_MAX 10
#define PP_DEEP 10
#define PS_DEEP 10
#define PR_DEEP 10
#define AVG_DEEP 5
#define POINT_DEEP (PP_DEEP + PS_DEEP + PR_DEEP)
#define PRESSURE_DEEP 8
#define INTE_INIT 8
#define CONFIG_LENGTH 512
#define TRUE 1
#define FALSE 0
#define FLAG_ABLE (0x4 << 12)
#define FLAG_FILL (0x2 << 12)
#define FLAG_KEY (0x1 << 12)
#define FLAG_COOR (0x0fff0fff)
#define FLAG_COOR_EX (0xffff0fff)
#define FLAG_ID (0xf0000000)

#define TAG "gsl_point_id"

// struct gsl_touch_info {
// 	int x[10];
// 	int y[10];
// 	int id[10];
// 	int finger_num;
// };

struct gsl_DISTANCE_TYPE {
	unsigned int i;
	unsigned int j;
	unsigned int min;		      /* distance min */
	unsigned int d[POINT_MAX][POINT_MAX]; /* distance; */
};

union gsl_POINT_TYPE {
	struct {
		unsigned y : 12;
		unsigned key : 1;
		unsigned fill : 1;
		unsigned able : 1;
		unsigned predict : 1;
		unsigned x : 16;
	} other;
	struct {
		unsigned y : 13;
		unsigned rev_2 : 3;
		unsigned x : 16;
	} dis;
	unsigned int all;
};

union gsl_DELAY_TYPE {
	struct {
		unsigned delay : 8;
		unsigned report : 8;
		unsigned dele : 8;
		unsigned rev_1 : 4;
		unsigned pres : 1;
		unsigned mask : 1;
		unsigned able : 1;
		unsigned init : 1;
	} other;
	unsigned int all;
};

union gsl_STATE_TYPE {
	struct {
		unsigned rev_0 : 8;
		unsigned rev_1 : 8;

		unsigned rev_2 : 4;
		unsigned active_prev : 1;
		unsigned menu : 1;
		unsigned cc_128 : 1;
		unsigned ex : 1;

		unsigned interpolation : 4;
		unsigned active : 1;
		unsigned only : 1;
		unsigned mask : 1;
		unsigned reset : 1;
	} other;
	unsigned int all;
};

struct gsl_EDGE_TYPE {
	unsigned int rate;
	unsigned int dis;
	union gsl_POINT_TYPE coor;
};

union gsl_DECIMAL_TYPE {
	struct {
		short y;
		short x;
	} other;
	unsigned int all;
};

union gsl_FLAG_TYPE {
	struct {
		unsigned over_report_mask : 1;
		unsigned opposite_x : 1;
		unsigned opposite_y : 1;
		unsigned opposite_xy : 1;
		unsigned line : 1;
		unsigned line_neg : 1;
		unsigned line_half : 1;
		unsigned middle_drv : 1;

		unsigned key_only_one : 1;
		unsigned key_line : 1;
		unsigned refe_rt : 1;
		unsigned refe_var : 1;
		unsigned base_median : 1;
		unsigned key_rt : 1;
		unsigned refe_reset : 1;
		unsigned sub_cross : 1;

		unsigned row_neg : 1;
		unsigned sub_line_coe : 1;
		unsigned sub_row_coe : 1;
		unsigned c2f_able : 1;
		unsigned thumb : 1;
		unsigned graph_h : 1;
		unsigned init_repeat : 1;
		unsigned near_reset_able : 1;

		unsigned emb_dead : 1;
		unsigned emb_point_mask : 1;
		unsigned interpolation : 1;
		unsigned sum2_able : 1;
		unsigned reduce_pin : 1;
		unsigned drv_order_ex : 1;
		unsigned id_over : 1;
		unsigned rev_1 : 1;
	} other;
	unsigned int all;
};
union gsl_ID_FLAG_TYPE {
	struct {
		unsigned reso_y : 1;
		unsigned reso_x : 1;
		unsigned screen_core : 1;
		unsigned screen_real : 1;
		unsigned ignore_pri : 1;
		unsigned id_prec_able : 1;
		unsigned first_avg : 1;
		unsigned round : 1;

		unsigned stretch_off : 1;
		unsigned rev_7 : 7;

		unsigned rev_x : 16;
	} other;
	unsigned int all;
};
static union {
	struct {
		unsigned char id;
		unsigned char num;
		unsigned char rev_1;
		unsigned char rev_2;
	} other;
	unsigned int all;
} prec_id;

static union gsl_POINT_TYPE point_array[POINT_DEEP][POINT_MAX];
static union gsl_POINT_TYPE *point_pointer[PP_DEEP];
static union gsl_POINT_TYPE *point_stretch[PS_DEEP];
static union gsl_POINT_TYPE *point_report[PR_DEEP];
static union gsl_POINT_TYPE point_now[POINT_MAX];
static union gsl_DELAY_TYPE point_delay[POINT_MAX];
static int filter_deep[POINT_MAX];
static int avg[AVG_DEEP];
static struct gsl_EDGE_TYPE point_edge;
static union gsl_DECIMAL_TYPE point_decimal[POINT_MAX];

static unsigned int pressure_now[POINT_MAX];
static unsigned int pressure_array[PRESSURE_DEEP][POINT_MAX];
static unsigned int pressure_report[POINT_MAX];
static unsigned int *pressure_pointer[PRESSURE_DEEP];

#define pp point_pointer
#define ps point_stretch
#define pr point_report
#define point_predict pp[0]
#define pa pressure_pointer

static union gsl_STATE_TYPE global_state;
static int inte_count;
static unsigned int csensor_count;
static int point_n;
static int point_num;
static int prev_num;
static int point_near;
static unsigned int point_shake;
static unsigned int reset_mask_send;
static unsigned int reset_mask_max;
static unsigned int reset_mask_count;
static union gsl_FLAG_TYPE global_flag;
static union gsl_ID_FLAG_TYPE id_flag;
static unsigned int id_first_coe;
static unsigned int id_speed_coe;
static unsigned int id_static_coe;
static unsigned int average;
static unsigned int soft_average;
static unsigned int report_delay;
static unsigned int delay_key;
static unsigned int report_ahead;
static unsigned int report_delete;
static unsigned char median_dis[4];
static unsigned int shake_min;
static int match_y[2];
static int match_x[2];
static int ignore_y[2];
static int ignore_x[2];
static int screen_y_max;
static int screen_x_max;
static int point_num_max;
static unsigned int drv_num;
static unsigned int sen_num;
static unsigned int drv_num_nokey;
static unsigned int sen_num_nokey;
static unsigned int coordinate_correct_able;
static unsigned int coordinate_correct_coe_x[64];
static unsigned int coordinate_correct_coe_y[64];
static unsigned int edge_cut[4];
static unsigned int stretch_array[4 * 4 * 2];
static unsigned int stretch_active[4 * 4 * 2];
static unsigned int shake_all_array[2 * 8];
static unsigned int edge_start;
static unsigned int reset_mask_dis;
static unsigned int reset_mask_type;
static unsigned int key_map_able;
static unsigned int key_range_array[8 * 3];
static int filter_able;
static unsigned int filter_coe[4];
static unsigned int multi_x_array[4], multi_y_array[4];
static unsigned int multi_group[4][64];
static int ps_coe[4][8], pr_coe[4][8];
static int point_repeat[2];
/* static	int near_set[2]; */
static int diagonal;
static int point_extend;
static unsigned int press_mask;
static union gsl_POINT_TYPE point_press_move;
static unsigned int press_move;
/* unsigned int key_dead_time			; */
/* unsigned int point_dead_time		; */
/* unsigned int point_dead_time2		; */
/* unsigned int point_dead_distance	; */
/* unsigned int point_dead_distance2	; */
/* unsigned int pressure_able; */
/* unsigned int pressure_save[POINT_MAX]; */
static unsigned int edge_first;
static unsigned int edge_first_coe;
static unsigned int point_corner;
static unsigned int stretch_mult;
/* ------------------------------------------------- */
static unsigned int config_static[CONFIG_LENGTH];
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
static void SortBubble(int t[], int size)
{
	int temp = 0;
	int m, n;

	for (m = 0; m < size; m++) {
		for (n = m + 1; n < size; n++) {
			temp = t[m];
			if (temp > t[n]) {
				t[m] = t[n];
				t[n] = temp;
			}
		}
	}
}

static int Sqrt(int d)
{
	int ret = 0;
	int i;

	for (i = 14; i >= 0; i--) {
		if ((ret + (0x1 << i)) * (ret + (0x1 << i)) <= d)
			ret |= (0x1 << i);
	}
	return ret;
}

static UINT PointRange(int x0, int y0, int x1, int y1)
{
	if (x0 < 1) /* && x1>=1 */ {
		if (x0 != x1)
			y0 = y1 + (y0 - y1) * (1 - x1) / (x0 - x1);
		x0 = 1;
	}
	if (x0 >= (int)drv_num_nokey * 64) {
		if (x0 != x1)
			y0 = y1 +
			     (y0 - y1) * ((int)drv_num_nokey * 64 - x1) /
				     (x0 - x1);
		x0 = drv_num_nokey * 64 - 1;
	}
	if (y0 < 1) {
		if (y0 != y1)
			x0 = x1 + (x0 - x1) * (1 - y1) / (y0 - y1);
		y0 = 1;
	}
	if (y0 >= (int)sen_num_nokey * 64) {
		if (y0 != y1)
			x0 = x1 +
			     (x0 - x1) * ((int)sen_num_nokey * 64 - y1) /
				     (y0 - y1);
		y0 = sen_num_nokey * 64 - 1;
	}
	if (x0 < 1)
		x0 = 1;
	if (x0 >= (int)drv_num_nokey * 64)
		x0 = drv_num_nokey * 64 - 1;
	if (y0 < 1)
		y0 = 1;
	if (y0 >= (int)sen_num_nokey * 64)
		y0 = sen_num_nokey * 64 - 1;
	return (x0 << 16) + y0;
}

static void PointCoor(void)
{
	int i;

	for (i = 0; i < point_num; i++) {
		if (global_state.other.ex)
			point_now[i].all &=
				(FLAG_COOR_EX | FLAG_KEY | FLAG_ABLE);
		else
			point_now[i].all &= (FLAG_COOR | FLAG_KEY | FLAG_ABLE);
	}
}

static void PointRepeat(void)
{
	int i, j;
	int x, y;
	int x_min, x_max, y_min, y_max;
	int pn;

	if (point_near)
		point_near--;
	if (prev_num > point_num)
		point_near = 8;
	if (point_repeat[0] == 0 || point_repeat[1] == 0) {
		if (point_near)
			pn = 96;
		else
			pn = 32;
	} else {
		if (point_near)
			pn = point_repeat[1];
		else
			pn = point_repeat[0];
	}
	for (i = 0; i < POINT_MAX; i++) {
		if (point_now[i].all == 0)
			continue;
		if (point_now[i].other.key)
			continue;
		x_min = point_now[i].other.x - pn;
		x_max = point_now[i].other.x + pn;
		y_min = point_now[i].other.y - pn;
		y_max = point_now[i].other.y + pn;
		for (j = i + 1; j < POINT_MAX; j++) {
			if (point_now[j].all == 0)
				continue;
			if (point_now[j].other.key)
				continue;
			x = point_now[j].other.x;
			y = point_now[j].other.y;
			if (x > x_min && x < x_max && y > y_min && y < y_max) {
				point_now[i].other.x =
					(point_now[i].other.x +
					 point_now[j].other.x + 1) /
					2;
				point_now[i].other.y =
					(point_now[i].other.y +
					 point_now[j].other.y + 1) /
					2;
				point_now[j].all = 0;
				pressure_now[i] =
					pressure_now[i] > pressure_now[j]
						? pressure_now[i]
						: pressure_now[j];
				pressure_now[j] = 0;
				i--;
				point_near = 8;
				break;
			}
		}
	}
	for (i = 0, j = 0; i < point_num; i++) {
		if (point_now[i].all == 0)
			continue;
		point_now[j].all = point_now[i].all;
		pressure_now[j++] = pressure_now[i];
	}
	point_num = j;
	for (; j < POINT_MAX; j++) {
		point_now[j].all = 0;
		pressure_now[j] = 0;
	}
}

static void PointPointer(void)
{
	int i, pn;

	point_n++;
	if (point_n >= PP_DEEP * PS_DEEP * PR_DEEP * PRESSURE_DEEP)
		point_n = 0;
	pn = point_n % PP_DEEP;
	for (i = 0; i < PP_DEEP; i++) {
		pp[i] = point_array[pn];
		if (pn == 0)
			pn = PP_DEEP - 1;
		else
			pn--;
	}
	pn = point_n % PS_DEEP;
	for (i = 0; i < PS_DEEP; i++) {
		ps[i] = point_array[pn + PP_DEEP];
		if (pn == 0)
			pn = PS_DEEP - 1;
		else
			pn--;
	}
	pn = point_n % PR_DEEP;
	for (i = 0; i < PR_DEEP; i++) {
		pr[i] = point_array[pn + PP_DEEP + PS_DEEP];
		if (pn == 0)
			pn = PR_DEEP - 1;
		else
			pn--;
	}
	pn = point_n % PRESSURE_DEEP;
	for (i = 0; i < PRESSURE_DEEP; i++) {
		pa[i] = pressure_array[pn];
		if (pn == 0)
			pn = PRESSURE_DEEP - 1;
		else
			pn--;
	}

	for (i = 0; i < POINT_MAX; i++) {
		pp[0][i].all = 0;
		ps[0][i].all = 0;
		pr[0][i].all = 0;
		pa[0][i] = 0;
	}
}

static unsigned int CC128(unsigned int x, unsigned int **coe, int k)
{
	if (k == 3) {
		return (x & ~127) + (coe[((x >> 6) & 1) ^ 1][x & 63] & 127);
	} else if (k == 4) {
		if (x & 128)
			return (x & ~127) + 127 -
			       (coe[(((127 - (x & 127)) >> 6) & 1) ^ 1]
				   [(127 - (x & 127)) & 63] &
				127);
		else
			return (x & ~127) +
			       (coe[((x >> 6) & 1) ^ 1][x & 63] & 127);
	}
	return 0;
}
static unsigned int CCO(unsigned int x, unsigned int coe[], int k)
{
	if (k == 0) {
		if (x & 32)
			return (x & ~31) + (31 - (coe[31 - (x & 31)] & 31));
		else
			return (x & ~31) + (coe[x & 31] & 31);
	}
	if (k == 1) {
		if (x & 64)
			return (x & ~63) + (63 - (coe[63 - (x & 63)] & 63));
		else
			return (x & ~63) + (coe[x & 63] & 63);
	}
	if (k == 2)
		return (x & ~63) + (coe[x & 63] & 63);

	return 0;
}

static void CoordinateCorrect(void)
{
	struct MULTI_TYPE {
		unsigned int range;
		unsigned int group;
	};
#ifdef LINE_MULTI_SIZE
#define LINE_SIZE LINE_MULTI_SIZE
#else
#define LINE_SIZE 4
#endif
	int i, j;
	unsigned int *px[LINE_SIZE + 1], *py[LINE_SIZE + 1];
	struct MULTI_TYPE multi_x[LINE_SIZE], multi_y[LINE_SIZE];
	unsigned int edge_size = 64;
	int kx, ky;

	if ((coordinate_correct_able & 0xf) == 0)
		return;
	kx = (coordinate_correct_able >> 4) & 0xf;
	ky = (coordinate_correct_able >> 8) & 0xf;
	px[0] = coordinate_correct_coe_x;
	py[0] = coordinate_correct_coe_y;
	for (i = 0; i < LINE_SIZE; i++) {
		px[i + 1] = NULL;
		py[i + 1] = NULL;
		multi_x[i].range = 0;
		multi_x[i].group = 0;
		multi_y[i].range = 0;
		multi_y[i].group = 0;
	}
	if (kx == 3 || ky == 3 || kx == 4 || ky == 4) {
		i = 0;
		if (kx == 3 || kx == 4)
			px[1] = multi_group[i++];
		if (ky == 3 || ky == 4)
			py[1] = multi_group[i++];
	} else {
		for (i = 0; i < LINE_SIZE; i++) {
			multi_x[i].range = multi_x_array[i] & 0xffff;
			multi_x[i].group = multi_x_array[i] >> 16;
			multi_y[i].range = multi_y_array[i] & 0xffff;
			multi_y[i].group = multi_y_array[i] >> 16;
		}
		j = 1;
		for (i = 0; i < LINE_SIZE; i++)
			if (multi_x[i].range && multi_x[i].group < LINE_SIZE)
				px[j++] = multi_group[multi_x[i].group];
		j = 1;
		for (i = 0; i < LINE_SIZE; i++)
			if (multi_y[i].range && multi_y[i].group < LINE_SIZE)
				py[j++] = multi_group[multi_y[i].group];
	}
	for (i = 0; i < (int)point_num && i < POINT_MAX; i++) {
		if (point_now[i].all == 0)
			break;
		if (point_now[i].other.key != 0)
			continue;
		if (point_now[i].other.x >= edge_size &&
		    point_now[i].other.x <= drv_num_nokey * 64 - edge_size) {
			if (global_state.other.active) {
				point_now[i].other.x =
					CCO(point_now[i].other.x,
					    multi_group[LINE_SIZE - 2], 2);
			} else if ((kx == 3 || kx == 4) &&
				   global_state.other.cc_128) {
				point_now[i].other.x =
					CC128(point_now[i].other.x, px, kx);
			} else if (kx == 3) {
				if (point_now[i].other.x & 64)
					point_now[i].other.x = CCO(
						point_now[i].other.x, px[0], 2);
				else
					point_now[i].other.x = CCO(
						point_now[i].other.x, px[1], 2);
			} else {
				for (j = 0; j < LINE_SIZE + 1; j++) {
					if (!(j >= LINE_SIZE ||
					      px[j + 1] == NULL ||
					      multi_x[j].range == 0 ||
					      point_now[i].other.x <
						      multi_x[j].range))
						continue;
					point_now[i].other.x =
						CCO(point_now[i].other.x, px[j],
						    kx);
					break;
				}
			}
		}
		if (point_now[i].other.y >= edge_size &&
		    point_now[i].other.y <= sen_num_nokey * 64 - edge_size) {
			if (global_state.other.active) {
				point_now[i].other.y =
					CCO(point_now[i].other.y,
					    multi_group[LINE_SIZE - 1], 2);
			} else if ((ky == 3 || ky == 4) &&
				   global_state.other.cc_128) {
				point_now[i].other.y =
					CC128(point_now[i].other.y, py, ky);
			} else if (ky == 3) {
				if (point_now[i].other.y & 64)
					point_now[i].other.y = CCO(
						point_now[i].other.y, py[0], 2);
				else
					point_now[i].other.y = CCO(
						point_now[i].other.y, py[1], 2);
			} else {
				for (j = 0; j < LINE_SIZE + 1; j++) {
					if (!(j >= LINE_SIZE ||
					      py[j + 1] == NULL ||
					      multi_y[j].range == 0 ||
					      point_now[i].other.y <
						      multi_y[j].range))
						continue;
					point_now[i].other.y =
						CCO(point_now[i].other.y, py[j],
						    ky);
					break;
				}
			}
		}
	}
#undef LINE_SIZE
}

static void PointPredictOne(unsigned int n)
{
	pp[0][n].all = pp[1][n].all & FLAG_COOR;
	pp[0][n].other.predict = 0;
}

static void PointPredictD2(unsigned int n)
{
	int x, y;

	x = (int)pp[1][n].other.x * 2 - (int)pp[3][n].other.x;
	y = (int)pp[1][n].other.y * 2 - (int)pp[3][n].other.y;
	pp[0][n].all = PointRange(x, y, pp[1][n].other.x, pp[1][n].other.y);
	pp[0][n].other.predict = 1;
}
static void PointPredictTwo(unsigned int n)
{
	int x, y;

	x = pp[1][n].other.x * 2 - pp[2][n].other.x;
	y = pp[1][n].other.y * 2 - pp[2][n].other.y;
	pp[0][n].all = PointRange(x, y, pp[1][n].other.x, pp[1][n].other.y);
	pp[0][n].other.predict = 1;
}

static void PointPredictSpeed(unsigned int n)
{
	int x, y;

	x = ((int)pp[1][n].other.x - (int)pp[2][n].other.x) * avg[0] / avg[1] +
	    (int)pp[1][n].other.x;
	y = ((int)pp[1][n].other.y - (int)pp[2][n].other.y) * avg[0] / avg[1] +
	    (int)pp[1][n].other.y;
	pp[0][n].all = PointRange(x, y, pp[1][n].other.x, pp[1][n].other.y);
	pp[0][n].other.predict = 1;
}
static void PointPredictD3(unsigned int n)
{
	int x, y;

	x = (int)pp[1][n].other.x * 5 + (int)pp[5][n].other.x -
	    (int)pp[3][n].other.x * 4;
	x /= 2;
	y = (int)pp[1][n].other.y * 5 + (int)pp[5][n].other.y -
	    (int)pp[3][n].other.y * 4;
	y /= 2;
	pp[0][n].all = PointRange(x, y, pp[1][n].other.x, pp[1][n].other.y);
	pp[0][n].other.predict = 1;
}

static void PointPredictThree(unsigned int n)
{
	int x, y;

	x = pp[1][n].other.x * 5 + pp[3][n].other.x - pp[2][n].other.x * 4;
	x /= 2;
	y = pp[1][n].other.y * 5 + pp[3][n].other.y - pp[2][n].other.y * 4;
	y /= 2;
	pp[0][n].all = PointRange(x, y, pp[1][n].other.x, pp[1][n].other.y);
	pp[0][n].other.predict = 1;
}

static void PointPredict(void)
{
	int i;

	for (i = 0; i < POINT_MAX; i++) {
		if (pp[1][i].all != 0) {
			if (global_state.other.interpolation != 0 &&
			    global_state.other.interpolation != INTE_INIT &&
			    pp[3][i].all && pp[3][i].other.fill == 0) {
				if (pp[4][i].all && pp[5][i].all &&
				    pp[5][i].other.fill == 0)
					PointPredictD3(i);
				else
					PointPredictD2(i);
			} else if (global_state.other.interpolation ||
				   pp[2][i].all == 0 ||
				   pp[2][i].other.fill != 0 ||
				   pp[3][i].other.fill != 0 ||
				   pp[1][i].other.key != 0 ||
				   global_state.other.only) {
				PointPredictOne(i);
			} else if (pp[2][i].all != 0 &&
				   (avg[0] != avg[1] || avg[1] != avg[2]) &&
				   avg[0] != 0 && avg[1] != 0) {
				PointPredictSpeed(i);
			} else if (pp[2][i].all != 0) {
				if (pp[3][i].all != 0)
					PointPredictThree(i);
				else
					PointPredictTwo(i);
			}
			pp[0][i].all |= FLAG_FILL;
			pa[0][i] = pa[1][i];
		} else
			pp[0][i].all = 0x0fff0fff;
		if (pp[1][i].other.key)
			pp[0][i].all |= FLAG_KEY;
	}
}

static unsigned int PointDistance(union gsl_POINT_TYPE *p1,
				  union gsl_POINT_TYPE *p2)
{
	int a, b, ret;

	if (id_flag.other.reso_y) {
		a = p1->dis.x;
		b = p2->dis.x;
		ret = (a - b) * (a - b);
		a = p1->dis.y * 64 * (int)screen_y_max / (int)screen_x_max *
		    ((int)drv_num_nokey * 64) / ((int)sen_num_nokey * 64) / 64;
		b = p2->dis.y * 64 * (int)screen_y_max / (int)screen_x_max *
		    ((int)drv_num_nokey * 64) / ((int)sen_num_nokey * 64) / 64;
		ret += (a - b) * (a - b);
	} else if (id_flag.other.reso_x) {
		a = p1->dis.x * 64 * (int)screen_x_max / (int)screen_y_max *
		    ((int)sen_num_nokey * 64) / ((int)drv_num_nokey * 64) / 64;
		b = p2->dis.x * 64 * (int)screen_x_max / (int)screen_y_max *
		    ((int)sen_num_nokey * 64) / ((int)drv_num_nokey * 64) / 64;
		ret = (a - b) * (a - b);
		a = p1->dis.y;
		b = p2->dis.y;
		ret += (a - b) * (a - b);
	} else {
		a = p1->dis.x;
		b = p2->dis.x;
		ret = (a - b) * (a - b);
		a = p1->dis.y;
		b = p2->dis.y;
		ret += (a - b) * (a - b);
	}
	return ret;
}

static void DistanceInit(struct gsl_DISTANCE_TYPE *p)
{
	int i;
	unsigned int *p_int = &(p->d[0][0]);

	for (i = 0; i < POINT_MAX * POINT_MAX; i++)
		*p_int++ = 0x7fffffff;
}

static int DistanceMin(struct gsl_DISTANCE_TYPE *p)
{
	int i, j;

	p->min = 0x7fffffff;
	for (j = 0; j < POINT_MAX; j++) {
		for (i = 0; i < POINT_MAX; i++) {
			if (p->d[j][i] < p->min) {
				p->i = i;
				p->j = j;
				p->min = p->d[j][i];
			}
		}
	}
	if (p->min == 0x7fffffff)
		return 0;
	return 1;
}

static void DistanceIgnore(struct gsl_DISTANCE_TYPE *p)
{
	int i, j;

	for (i = 0; i < POINT_MAX; i++)
		p->d[p->j][i] = 0x7fffffff;
	for (j = 0; j < POINT_MAX; j++)
		p->d[j][p->i] = 0x7fffffff;
}

static int SpeedGet(int d)
{
	int i;

	for (i = 8; i > 0; i--) {
		if (d > 0x100 << i)
			break;
	}
	return i;
}

static void PointId(void)
{
	int i, j;
	struct gsl_DISTANCE_TYPE distance;
	unsigned int id_speed[POINT_MAX];

	DistanceInit(&distance);
	for (i = 0; i < POINT_MAX; i++) {
		if (pp[0][i].other.predict == 0 || pp[1][i].other.fill != 0)
			id_speed[i] = id_first_coe;
		else {
			id_speed[i] =
				SpeedGet(PointDistance(&pp[1][i], &pp[0][i]));
			j = SpeedGet(PointDistance(&pp[2][i], &pp[1][i]));
			if (id_speed[i] < (unsigned int)j)
				id_speed[i] = j;
		}
	}
	for (i = 0; i < POINT_MAX; i++) {
		if (pp[0][i].all == FLAG_COOR)
			continue;
		for (j = 0; j < point_num && j < POINT_MAX; j++)
			distance.d[j][i] =
				PointDistance(&point_now[j], &pp[0][i]);
	}
	if (point_num == 0)
		return;
	if (global_state.other.only || global_state.other.active) {
		do {
			if (DistanceMin(&distance)) {
				if (pp[1][0].all != 0 &&
				    pp[1][0].other.key !=
					    point_now[distance.j].other.key) {
					DistanceIgnore(&distance);
					break; /*continue;*/
				}
				pp[0][0].all = point_now[distance.j].all;
			} else
				pp[0][0].all = point_now[0].all;
			for (i = 0; i < POINT_MAX; i++)
				point_now[i].all = 0;
		} while (0);
		point_num = 1;
	} else {
		for (j = 0; j < point_num && j < POINT_MAX; j++) {
			if (DistanceMin(&distance) == 0)
				break;
			if (distance.min >=
			    (id_static_coe +
			     id_speed[distance.i] * id_speed_coe)
			    /**average/(soft_average+1)*/) {
				/* point_now[distance.j].id = 0xf;//new id */
				continue;
			}
			pp[0][distance.i].all = point_now[distance.j].all;
			pa[0][distance.i] = pressure_now[distance.j];
			point_now[distance.j].all = 0;
			DistanceIgnore(&distance);
		}
	}
}

static int ClearLenPP(int i)
{
	int n;

	for (n = 0; n < PP_DEEP; n++) {
		if (pp[n][i].all)
			break;
	}
	return n;
}

static void PointNewId(void)
{
	int id, j;

	for (j = 0; j < POINT_MAX; j++)
		if ((pp[0][j].all & FLAG_COOR) == FLAG_COOR)
			pp[0][j].all = 0;
	for (j = 0; j < POINT_MAX; j++) {
		if (point_now[j].all != 0) {
			if (point_now[j].other.able)
				continue;
			for (id = 1; id <= POINT_MAX; id++) {
				if (ClearLenPP(id - 1) > (int)(1 + 1)) {
					pp[0][id - 1].all = point_now[j].all;
					pa[0][id - 1] = pressure_now[j];
					point_now[j].all = 0;
					break;
				}
			}
		}
	}
}

static void PointOrder(void)
{
	int i;

	for (i = 0; i < POINT_MAX; i++) {
		if (pp[0][i].other.fill == 0)
			continue;
		if (pp[1][i].all == 0 || pp[1][i].other.fill != 0 ||
		    filter_able == 0 || filter_able == 1) {
			pp[0][i].all = 0;
			pressure_now[i] = 0;
		}
	}
}

static void PointCross(void)
{
	unsigned int i, j;
	unsigned int t;

	for (j = 0; j < POINT_MAX; j++) {
		for (i = j + 1; i < POINT_MAX; i++) {
			if (pp[0][i].all == 0 || pp[0][j].all == 0 ||
			    pp[1][i].all == 0 || pp[1][j].all == 0)
				continue;
			if (((pp[0][j].other.x < pp[0][i].other.x &&
			      pp[1][j].other.x > pp[1][i].other.x) ||
			     (pp[0][j].other.x > pp[0][i].other.x &&
			      pp[1][j].other.x < pp[1][i].other.x)) &&
			    ((pp[0][j].other.y < pp[0][i].other.y &&
			      pp[1][j].other.y > pp[1][i].other.y) ||
			     (pp[0][j].other.y > pp[0][i].other.y &&
			      pp[1][j].other.y < pp[1][i].other.y))) {
				t = pp[0][i].all;
				pp[0][i].all = pp[0][j].all;
				pp[0][j].all = t;
				t = pa[0][i];
				pa[0][i] = pa[0][j];
				pa[0][j] = t;
			}
		}
	}
}

static void GetPointNum(union gsl_POINT_TYPE *pt)
{
	int i;

	point_num = 0;
	for (i = 0; i < POINT_MAX; i++)
		if (pt[i].all != 0)
			point_num++;
}

static unsigned int PointDelayAvg(int i)
{
	UINT j, len;
	int sum_x = 0;
	int sum_y = 0;

	if (id_flag.other.first_avg == 0)
		return TRUE;
	if (pp[0][i].all) {
		for (j = 0; j <= point_delay[i].other.report; j++) {
			sum_x += pp[j][i].other.x;
			sum_y += pp[j][i].other.y;
		}
		sum_x /= j;
		sum_y /= j;
		for (j = 0; j <= point_delay[i].other.report; j++) {
			ps[j][i].other.x = sum_x;
			ps[j][i].other.y = sum_y;
			pr[j][i].other.x = sum_x;
			pr[j][i].other.y = sum_y;
		}
		return TRUE;
	}
	if (pp[1][i].all == 0)
		return FALSE;
	for (j = 1; j <= point_delay[i].other.delay; j++)
		if (pp[j][i].all == 0)
			break;
	len = j - 1;
	if (len <
	    1 + (point_delay[i].other.delay - point_delay[i].other.report))
		return FALSE;
	len -= (point_delay[i].other.delay - point_delay[i].other.report);
	for (j = 1; j <= len; j++) {
		sum_x += pp[j][i].other.x;
		sum_y += pp[j][i].other.y;
	}
	if (j - 1 == 0)
		return FALSE;
	sum_x /= j - 1;
	sum_y /= j - 1;
	for (j = 1; j <= len; j++) {
		if (pp[j][i].all == 0)
			break;
		ps[j][i].other.x = sum_x;
		ps[j][i].other.y = sum_y;
		pr[j][i].other.x = sum_x;
		pr[j][i].other.y = sum_y;
	}
	return TRUE;
}
static void PointDelay(void)
{
	int i, j;

	for (i = 0; i < POINT_MAX; i++) {
		if (report_delay == 0 && delay_key == 0) {
			point_delay[i].all = 0;
			if (pp[0][i].all)
				point_delay[i].other.able = 1;
			if (pr[0][i].all == 0)
				point_delay[i].other.mask = 0;
			continue;
		}
		if (pp[0][i].all != 0 && point_delay[i].other.init == 0 &&
		    point_delay[i].other.able == 0) {
			if (point_num == 0)
				continue;
			if (delay_key && pp[0][i].other.key) {
				point_delay[i].other.delay =
					(delay_key >>
					 3 * ((point_num > 10 ? 10
							      : point_num) -
					      1)) &
					0x7;
				point_delay[i].other.report = 0;
				point_delay[i].other.dele = 0;
			} else {
				point_delay[i].other.delay =
					(report_delay >>
					 3 * ((point_num > 10 ? 10
							      : point_num) -
					      1)) &
					0x7;
				point_delay[i].other.report =
					(report_ahead >>
					 3 * ((point_num > 10 ? 10
							      : point_num) -
					      1)) &
					0x7;
				point_delay[i].other.dele =
					(report_delete >>
					 3 * ((point_num > 10 ? 10
							      : point_num) -
					      1)) &
					0x7;
				if (point_delay[i].other.report >
				    point_delay[i].other.delay)
					point_delay[i].other.report =
						point_delay[i].other.delay;
				point_delay[i].other.report =
					point_delay[i].other.delay -
					point_delay[i].other.report;
				if (point_delay[i].other.dele >
				    point_delay[i].other.report)
					point_delay[i].other.dele =
						point_delay[i].other.report;
				point_delay[i].other.dele =
					point_delay[i].other.report -
					point_delay[i].other.dele;
			}
			point_delay[i].other.init = 1;
		}
		if (id_flag.other.first_avg && pp[0][i].all == 0 &&
		    pp[1][i].all != 0 && point_delay[i].other.able == 0 &&
		    point_delay[i].other.init != 0) {
			if (PointDelayAvg(i)) {
				point_delay[i].other.able = 1;
				point_delay[i].other.report = 1;
				point_delay[i].other.dele = 1;
			} else {
				point_delay[i].other.init = 0;
			}
		} else if (pp[0][i].all == 0) {
			point_delay[i].other.init = 0;
		}
		if (point_delay[i].other.able == 0 &&
		    point_delay[i].other.init != 0) {
			for (j = 0; j <= (int)point_delay[i].other.delay; j++) {
				if (pp[j][i].all == 0 ||
				    pp[j][i].other.fill != 0 ||
				    pp[j][i].other.able != 0)
					break;
			}
			if (j <= (int)point_delay[i].other.delay)
				continue;
			if (PointDelayAvg(i))
				point_delay[i].other.able = 1;
			else
				j = 0;
			if (id_flag.other.first_avg)
				point_delay[i].other.report =
					point_delay[i].other.dele;
		}
		if (pp[point_delay[i].other.dele][i].all == 0) {
			point_delay[i].other.able = 0;
			point_delay[i].other.mask = 0;
			continue;
		}
		if (point_delay[i].other.able == 0)
			continue;
		if (report_delete == 0 && point_delay[i].other.report) {
			if (PointDistance(
				    &pp[point_delay[i].other.report][i],
				    &pp[point_delay[i].other.report - 1][i]) <
			    3 * 3) {
				point_delay[i].other.report--;
				if (point_delay[i].other.dele)
					point_delay[i].other.dele--;
			}
		}
	}
}

static unsigned int PointMOne(unsigned int x0, unsigned int x1)
{
	int e1, e2;

	e1 = (edge_start >> 24) & 0xff;
	e2 = (edge_start >> 16) & 0xff;
	if (e1 == 0)
		e1 = 18;
	if (e2 == 0)
		e2 = 24;
	if (x1 >= x0)
		return 0;
	if (x1 < (edge_start & 0xff) && x1 * e1 / 16 < x0)
		return 1;
	else if (x1 < (edge_start & 0xff) * 2 && x1 * e2 / 16 < x0)
		return 1;
	return 0;
}

static void PointMenu(void)
{
	unsigned int edge_dis;
	unsigned int edge_e;

	if (edge_start == 0)
		return;
	if (pp[0][0].all == 0 || pp[1][0].all == 0 ||
	    (pp[2][0].all != 0 && global_state.other.menu == 0) ||
	    pp[3][0].all != 0) {
		global_state.other.menu = FALSE;
		return;
	}
	if (point_delay[0].other.delay < 1 || point_delay[0].other.report < 1)
		return;
	edge_e = edge_start & 0xff;
	edge_dis = (edge_start & 0xff00) >> 8;
	edge_dis = edge_dis == 0 ? 8 * 8 : edge_dis * edge_dis;
	if (PointDistance(&pp[0][0], &pp[1][0]) >= edge_dis) {
		if (PointMOne(pp[0][0].other.x, pp[1][0].other.x))
			pr[1][0].other.x = 1;
		if (PointMOne(pp[0][0].other.y, pp[1][0].other.y))
			pr[1][0].other.y = 1;
		if (PointMOne(drv_num_nokey * 64 - pp[0][0].other.x,
			      drv_num_nokey * 64 - pp[1][0].other.x))
			pr[1][0].other.x = drv_num_nokey * 64 - 1;
		if (PointMOne(sen_num_nokey * 64 - pp[0][0].other.y,
			      sen_num_nokey * 64 - pp[1][0].other.y))
			pr[1][0].other.y = sen_num_nokey * 64 - 1;
	} else if (global_state.other.menu == 0) {
		if ((pp[0][0].other.x < edge_e && pp[1][0].other.x < edge_e) ||
		    (pp[0][0].other.y < edge_e && pp[1][0].other.y < edge_e) ||
		    (pp[0][0].other.x > drv_num_nokey * 64 - edge_e &&
		     pp[1][0].other.x > drv_num_nokey * 64 - edge_e) ||
		    (pp[0][0].other.y > sen_num_nokey * 64 - edge_e &&
		     pp[1][0].other.y > sen_num_nokey * 64 - edge_e)) {
			point_delay[0].other.able = FALSE;
			global_state.other.menu = TRUE;
		}
	}
}

static void FilterOne(int i, int *ps_c, int *pr_c, int denominator)
{
	int j;
	int x = 0, y = 0;

	pr[0][i].all = ps[0][i].all;
	if (pr[0][i].all == 0)
		return;
	if (denominator <= 0)
		return;
	for (j = 0; j < 8; j++) {
		x += (int)pr[j][i].other.x * (int)pr_c[j] +
		     (int)ps[j][i].other.x * (int)ps_c[j];
		y += (int)pr[j][i].other.y * (int)pr_c[j] +
		     (int)ps[j][i].other.y * (int)ps_c[j];
	}
	x = (x + denominator / 2) / denominator;
	y = (y + denominator / 2) / denominator;
	if (x < 0)
		x = 0;
	if (x > 0xffff)
		x = 0xffff;
	if (y < 0)
		y = 0;
	if (y > 0xfff)
		y = 0xfff;
	pr[0][i].other.x = x;
	pr[0][i].other.y = y;
}

static unsigned int FilterSpeed(int i)
{
	return (Sqrt(PointDistance(&ps[0][i], &ps[1][i])) +
		Sqrt(PointDistance(&ps[1][i], &ps[2][i]))) /
	       2;
}

static int MedianSpeedOver(int id, int deep)
{
	int i;
	unsigned int dis;
	int speed_over = 0;

	deep = deep / 2 - 1;
	if (deep < 0 || deep > 3)
		return TRUE;
	dis = median_dis[deep] * median_dis[deep];
	for (i = 0; i <= deep && i < POINT_DEEP; i++) {
		if (PointDistance(&ps[i][id], &ps[i + 1][id]) > dis)
			speed_over++;
	}
	if (speed_over >= 2)
		return TRUE;
	return FALSE;
}

static void PointMedian(void)
{
	int i, j;
	int deep;
	int buf_x[PS_DEEP], buf_y[PS_DEEP];

	for (i = 0; i < POINT_MAX; i++) {
		if (filter_deep[i] < 3)
			deep = 3;
		else
			deep = filter_deep[i] + 2;
		if (deep >= PS_DEEP)
			deep = PS_DEEP - 1;
		deep |= 1;
		for (; deep >= 3; deep -= 2) {
			if (MedianSpeedOver(i, deep))
				continue;
			for (j = 0; j < deep; j++) {
				buf_x[j] = ps[j][i].other.x;
				buf_y[j] = ps[j][i].other.y;
			}
			SortBubble(buf_x, deep);
			SortBubble(buf_y, deep);
			pr[0][i].other.x = buf_x[deep / 2];
			pr[0][i].other.y = buf_y[deep / 2];
			break;
		}
		filter_deep[i] = deep;
	}
}

static void PointFilter(void)
{
	int i, j;
	int speed_now;
	int filter_speed[6];
	int ps_c[8];
	int pr_c[8];

	for (i = 0; i < POINT_MAX; i++)
		pr[0][i].all = ps[0][i].all;

	for (i = 0; i < POINT_MAX; i++) {
		if (pr[0][i].all != 0 && pr[1][i].all == 0) {
			for (j = 1; j < PR_DEEP; j++)
				pr[j][i].all = ps[0][i].all;
			for (j = 1; j < PS_DEEP; j++)
				ps[j][i].all = ps[0][i].all;
		}
	}
	if (filter_able >= 0 && filter_able <= 1)
		return;
	if (filter_able > 1) {
		for (i = 0; i < 8; i++) {
			ps_c[i] = (filter_coe[i / 4] >> ((i % 4) * 8)) & 0xff;
			pr_c[i] =
				(filter_coe[i / 4 + 2] >> ((i % 4) * 8)) & 0xff;
			if (ps_c[i] >= 0x80)
				ps_c[i] |= 0xffffff00;
			if (pr_c[i] >= 0x80)
				pr_c[i] |= 0xffffff00;
		}
		for (i = 0; i < POINT_MAX; i++)
			FilterOne(i, ps_c, pr_c, filter_able);

	} else if (filter_able == -1) {
		PointMedian();
	} else if (filter_able < 0) {
		for (i = 0; i < 4; i++)
			filter_speed[i + 1] = median_dis[i];
		filter_speed[0] = median_dis[0] * 2 - median_dis[1];
		filter_speed[5] = median_dis[3] / 2;
		for (i = 0; i < POINT_MAX; i++) {
			if (pr[0][i].all == 0) {
				filter_deep[i] = 0;
				continue;
			}
			speed_now = FilterSpeed(i);
			if (filter_deep[i] > 0 &&
			    speed_now > filter_speed[filter_deep[i] + 1 - 2])
				filter_deep[i]--;
			else if (filter_deep[i] < 3 &&
				 speed_now <
					 filter_speed[filter_deep[i] + 1 + 2])
				filter_deep[i]++;

			FilterOne(i, ps_coe[filter_deep[i]],
				  pr_coe[filter_deep[i]], 0 - filter_able);
		}
	}
}

static unsigned int KeyMap(int *drv, int *sen)
{
	struct KEY_TYPE_RANGE {
		unsigned int up_down, left_right;
		unsigned int coor;
	};
	struct KEY_TYPE_RANGE *key_range =
		(struct KEY_TYPE_RANGE *)key_range_array;
	int i;

	for (i = 0; i < 8; i++) {
		if ((unsigned int)*drv >= (key_range[i].up_down >> 16) &&
		    (unsigned int)*drv <= (key_range[i].up_down & 0xffff) &&
		    (unsigned int)*sen >= (key_range[i].left_right >> 16) &&
		    (unsigned int)*sen <= (key_range[i].left_right & 0xffff)) {
			*sen = key_range[i].coor >> 16;
			*drv = key_range[i].coor & 0xffff;
			return key_range[i].coor;
		}
	}
	return 0;
}

static unsigned int ScreenResolution(union gsl_POINT_TYPE *p)
{
	int x, y;

	x = p->other.x;
	y = p->other.y;
	if (p->other.key == FALSE) {
		y = ((y - match_y[1]) * match_y[0] + 2048) / 4096;
		x = ((x - match_x[1]) * match_x[0] + 2048) / 4096;
	}
	y = y * (int)screen_y_max / ((int)sen_num_nokey * 64);
	x = x * (int)screen_x_max / ((int)drv_num_nokey * 64);
	if (p->other.key == FALSE) {
		if (id_flag.other.ignore_pri == 0) {
			if (ignore_y[0] != 0 || ignore_y[1] != 0) {
				if (y < ignore_y[0])
					return 0;
				if (ignore_y[1] <= screen_y_max / 2 &&
				    y > screen_y_max - ignore_y[1])
					return 0;
				if (ignore_y[1] >= screen_y_max / 2 &&
				    y > ignore_y[1])
					return 0;
			}
			if (ignore_x[0] != 0 || ignore_x[1] != 0) {
				if (x < ignore_x[0])
					return 0;
				if (ignore_x[1] <= screen_x_max / 2 &&
				    x > screen_x_max - ignore_x[1])
					return 0;
				if (ignore_x[1] >= screen_x_max / 2 &&
				    x > ignore_x[1])
					return 0;
			}
		}
		if (y <= (int)edge_cut[2])
			y = (int)edge_cut[2] + 1;
		if (y >= screen_y_max - (int)edge_cut[3])
			y = screen_y_max - (int)edge_cut[3] - 1;
		if (x <= (int)edge_cut[0])
			x = (int)edge_cut[0] + 1;
		if (x >= screen_x_max - (int)edge_cut[1])
			x = screen_x_max - (int)edge_cut[1] - 1;
		if (global_flag.other.opposite_x)
			y = screen_y_max - y;
		if (global_flag.other.opposite_y)
			x = screen_x_max - x;
		if (global_flag.other.opposite_xy) {
			y ^= x;
			x ^= y;
			y ^= x;
		}
	} else {
		if (y < 0)
			y = 0;
		if (x < 0)
			x = 0;
		if ((key_map_able & 0x1) != FALSE && KeyMap(&x, &y) == 0)
			return 0;
	}
	return ((y << 16) & 0x0fff0000) + (x & 0x0000ffff);
}

static void PointReport(struct gsl_touch_info *cinfo)
{
	int i;
	unsigned int data[POINT_MAX];
	unsigned int dp[POINT_MAX];
	int num = 0;

	if (point_num > point_num_max &&
	    global_flag.other.over_report_mask != 0) {
		point_num = 0;
		cinfo->finger_num = 0;
		prec_id.all = 0;
		return;
	}
	for (i = 0; i < POINT_MAX; i++)
		data[i] = dp[i] = 0;
	num = 0;
	if (global_flag.other.id_over) {
		for (i = 0; i < POINT_MAX && num < point_num_max; i++) {
			if (point_delay[i].other.mask ||
			    point_delay[i].other.able == 0)
				continue;
			if (point_delay[i].other.report >= PR_DEEP - 1)
				continue;
			if (pr[point_delay[i].other.report + 1][i].other.able ==
			    0)
				continue;
			if (pr[point_delay[i].other.report][i].all) {
				pr[point_delay[i].other.report][i].other.able =
					1;
				data[i] = ScreenResolution(
					&pr[point_delay[i].other.report][i]);
				if (data[i]) {
					dp[i] = pressure_report[i];
					data[i] |= (i + 1) << 28;
					num++;
				}
			}
		}
		for (i = 0; i < POINT_MAX && num < point_num_max; i++) {
			if (point_delay[i].other.mask ||
			    point_delay[i].other.able == 0)
				continue;
			if (point_delay[i].other.report >= PR_DEEP)
				continue;
			if (pr[point_delay[i].other.report][i].all == 0)
				continue;
			if (pr[point_delay[i].other.report][i].other.able ==
			    0) {
				pr[point_delay[i].other.report][i].other.able =
					1;
				data[i] = ScreenResolution(
					&pr[point_delay[i].other.report][i]);
				if (data[i]) {
					dp[i] = pressure_report[i];
					data[i] |= (i + 1) << 28;
					num++;
				}
			}
		}
	} else {
		num = 0;
		for (i = 0; i < point_num_max && i < POINT_MAX; i++) {
			if (point_delay[i].other.mask ||
			    point_delay[i].other.able == 0)
				continue;
			if (point_delay[i].other.report >= PR_DEEP)
				continue;
			data[num] = ScreenResolution(
				&pr[point_delay[i].other.report][i]);
			if (data[num]) {
				dp[num] = pressure_report[i];
				data[num++] |= (i + 1) << 28;
			}
		}
	}
	num = 0;
	for (i = 0; i < POINT_MAX; i++) {
		if (data[i] == 0)
			continue;
		point_now[num].all = data[i];
		cinfo->x[num] = (data[i] >> 16) & 0xfff;
		cinfo->y[num] = data[i] & 0xfff;
		cinfo->id[num] = data[i] >> 28;
		pressure_now[num] = dp[i];
		num++;
	}
	for (i = num; i < POINT_MAX; i++) {
		point_now[i].all = 0;
		pressure_now[i] = 0;
	}
	point_num = num;
	cinfo->finger_num = point_num;
	if (id_flag.other.id_prec_able == FALSE)
		return;
	if (prec_id.all == 0 && point_num == 1) {
		if ((point_now[0].all >> 28) > 1)
			prec_id.other.id = (point_now[0].all >> 28);
		else
			prec_id.other.id = 0xff;
	}
	if (prec_id.other.id != 0 && prec_id.other.id != 0xff) {
		for (i = 0; i < point_num; i++) {
			if ((point_now[i].all >> 28) == 1) {
				point_now[i].all &= ~(0xf << 28);
				point_now[i].all |= prec_id.other.id << 28;
				cinfo->id[i] = prec_id.other.id;
			} else if ((point_now[i].all >> 28) ==
				   prec_id.other.id) {
				point_now[i].all &= ~(0xf << 28);
				point_now[i].all |= 1 << 28;
				cinfo->id[i] = 1;
			}
		}
	}
	if (point_num == 0)
		prec_id.all = 0;
	else
		prec_id.other.num = (unsigned char)point_num;
}

static void PointRound(void)
{
	int id, i;
	int x, y;
	int x0, y0;
	int dis, r[4], coe[4];
	struct STRETCH_TYPE {
		int range;
		int coe;
	};
	struct STRETCH_TYPE_ALL {
		struct STRETCH_TYPE up[4];
		struct STRETCH_TYPE down[4];
		struct STRETCH_TYPE left[4];
		struct STRETCH_TYPE right[4];
	}; /* stretch; */
	struct STRETCH_TYPE_ALL *stretch;
	int sac[4 * 4 * 2]; /* stretch_array_copy */
	int data[2];

	if (id_flag.other.round == 0 || id_flag.other.stretch_off)
		return;
	if (screen_x_max == 0 || screen_y_max == 0)
		return;
	id = 0;
	for (i = 0; i < 4 * 4 * 2; i++) {
		sac[i] = stretch_array[i];
		if (sac[i])
			id++;
	}
	if (id == 0)
		return;
	stretch = (struct STRETCH_TYPE_ALL *)sac;
	for (i = 0; i < 4; i++) {
		if (stretch->up[i].range)
			stretch->up[i].range = stretch->up[i].range *
					       sen_num_nokey * drv_num_nokey *
					       64 / screen_x_max;
		if (stretch->down[i].range)
			stretch->down[i].range = stretch->down[i].range *
						 sen_num_nokey * drv_num_nokey *
						 64 / screen_x_max;
		if (stretch->left[i].range)
			stretch->left[i].range = stretch->left[i].range *
						 sen_num_nokey * drv_num_nokey *
						 64 / screen_y_max;
		if (stretch->right[i].range)
			stretch->right[i].range =
				stretch->right[i].range * sen_num_nokey *
				drv_num_nokey * 64 / screen_y_max;
	}

	x0 = 64 * sen_num_nokey * drv_num_nokey / 2;
	y0 = x0;
	for (id = 0; id < POINT_MAX; id++) {
		if (point_now[id].all == 0 || point_now[id].other.key != 0)
			continue;
		x = point_now[id].other.x * sen_num_nokey;
		y = point_now[id].other.y * drv_num_nokey;
		dis = Sqrt((x - x0) * (x - x0) + (y - y0) * (y - y0));

		for (i = 0; i < 4; i++) {
			r[i] = 0;
			coe[i] = 0;
			if (x < x0) {
				r[i] += (x0 - x) * stretch->up[i].range / dis *
					(x0 - x) * stretch->up[i].range / dis;
				coe[i] += (x0 - x) * stretch->up[i].coe / dis *
					  (x0 - x) * stretch->up[i].coe / dis;
			}
			if (x > x0) {
				r[i] += (x - x0) * stretch->down[i].range /
					dis * (x - x0) *
					stretch->down[i].range / dis;
				coe[i] += (x - x0) * stretch->down[i].coe /
					  dis * (x - x0) *
					  stretch->down[i].coe / dis;
			}
			if (y < y0) {
				r[i] += (y0 - y) * stretch->left[i].range /
					dis * (y0 - y) *
					stretch->left[i].range / dis;
				coe[i] += (y0 - y) * stretch->left[i].coe /
					  dis * (y0 - y) *
					  stretch->left[i].coe / dis;
			}
			if (y > y0) {
				r[i] += (y - y0) * stretch->right[i].range /
					dis * (y - y0) *
					stretch->right[i].range / dis;
				coe[i] += (y - y0) * stretch->right[i].coe /
					  dis * (y - y0) *
					  stretch->right[i].coe / dis;
			}
			r[i] = Sqrt(r[i]);
			coe[i] = Sqrt(coe[i]);
		}
		data[0] = 0;
		data[1] = dis;
		for (i = 3; i >= 0; i--) {
			if (r[i] == 0 || coe[i] <= 1)
				continue;
			if (data[1] > r[i]) {
				data[0] += (data[1] - r[i]) * coe[i] / 128;
				data[1] = r[i];
			}
		}
		data[0] += data[1];
		x = (x - x0) * data[0] / dis + x0;
		y = (y - y0) * data[0] / dis + y0;

		for (i = 1; i < 4; i++) {
			if (r[i] == 0)
				break;
			if (coe[i] > 1)
				continue;
			dis = Sqrt((x - x0) * (x - x0) + (y - y0) * (y - y0));
			if (dis <= r[i] || dis <= 0)
				break;
			x = (x - x0) * r[i] / dis + x0;
			y = (y - y0) * r[i] / dis + y0;
		}
		x /= (int)sen_num_nokey;
		if (x <= 0)
			x = 1;
		if (x > 0xfff)
			x = 0xfff;
		point_now[id].other.x = x;
		y /= (int)drv_num_nokey;
		if (y <= 0)
			y = 1;
		if (y > 0xfff)
			y = 0xfff;
		point_now[id].other.y = y;
	}
}

static void PointEdge(void)
{
	struct STRETCH_TYPE {
		int range;
		int coe;
	};
	struct STRETCH_TYPE_ALL {
		struct STRETCH_TYPE up[4];
		struct STRETCH_TYPE down[4];
		struct STRETCH_TYPE left[4];
		struct STRETCH_TYPE right[4];
	};
	struct STRETCH_TYPE_ALL *stretch;
	int i, id;
	int data[2];
	int x, y;
	int sac[4 * 4 * 2];

	if (id_flag.other.round || id_flag.other.stretch_off)
		return;
	if (screen_x_max == 0 || screen_y_max == 0)
		return;
	id = 0;
	for (i = 0; i < 4 * 4 * 2; i++) {
		if (global_state.other.active)
			sac[i] = stretch_active[i];
		else
			sac[i] = stretch_array[i];
		if (sac[i])
			id++;
	}
	if (id == 0)
		return;
	stretch = (struct STRETCH_TYPE_ALL *)sac;
	for (i = 0; i < 4; i++) {
		if (id_flag.other.screen_core)
			break;
		if (stretch->right[i].range > screen_y_max * 64 / 128 ||
		    stretch->down[i].range > screen_x_max * 64 / 128 ||
		    id_flag.other.screen_real) {
			for (i = 0; i < 4; i++) {
				if (stretch->up[i].range)
					stretch->up[i].range =
						stretch->up[i].range *
						drv_num_nokey * 64 /
						screen_x_max;
				if (stretch->down[i].range)
					stretch->down[i].range =
						(screen_x_max -
						 stretch->down[i].range) *
						drv_num_nokey * 64 /
						screen_x_max;
				if (stretch->left[i].range)
					stretch->left[i].range =
						stretch->left[i].range *
						sen_num_nokey * 64 /
						screen_y_max;
				if (stretch->right[i].range)
					stretch->right[i].range =
						(screen_y_max -
						 stretch->right[i].range) *
						sen_num_nokey * 64 /
						screen_y_max;
			}
			break;
		}
	}
	for (id = 0; id < POINT_MAX; id++) {
		if (point_now[id].all == 0 || point_now[id].other.key != 0)
			continue;
		x = point_now[id].other.x;
		y = point_now[id].other.y;

		data[0] = 0;
		data[1] = y;
		for (i = 0; i < 4; i++) {
			if (stretch->left[i].range == 0)
				break;
			if (data[1] < stretch->left[i].range) {
				data[0] += (stretch->left[i].range - data[1]) *
					   stretch->left[i].coe / 128;
				data[1] = stretch->left[i].range;
			}
		}
		y = data[1] - data[0];
		if (y <= 0)
			y = 1;
		if (y >= (int)sen_num_nokey * 64)
			y = sen_num_nokey * 64 - 1;

		data[0] = 0;
		data[1] = sen_num_nokey * 64 - y;
		for (i = 0; i < 4; i++) {
			if (stretch->right[i].range == 0)
				break;
			if (data[1] < stretch->right[i].range) {
				data[0] += (stretch->right[i].range - data[1]) *
					   stretch->right[i].coe / 128;
				data[1] = stretch->right[i].range;
			}
		}
		y = sen_num_nokey * 64 - (data[1] - data[0]);
		if (y <= 0)
			y = 1;
		if (y >= (int)sen_num_nokey * 64)
			y = sen_num_nokey * 64 - 1;

		data[0] = 0;
		data[1] = x;
		for (i = 0; i < 4; i++) {
			if (stretch->up[i].range == 0)
				break;
			if (data[1] < stretch->up[i].range) {
				data[0] += (stretch->up[i].range - data[1]) *
					   stretch->up[i].coe / 128;
				data[1] = stretch->up[i].range;
			}
		}
		x = data[1] - data[0];
		if (x <= 0)
			x = 1;
		if (x >= (int)drv_num_nokey * 64)
			x = drv_num_nokey * 64 - 1;

		data[0] = 0;
		data[1] = drv_num_nokey * 64 - x;
		for (i = 0; i < 4; i++) {
			if (stretch->down[i].range == 0)
				break;
			if (data[1] < stretch->down[i].range) {
				data[0] += (stretch->down[i].range - data[1]) *
					   stretch->down[i].coe / 128;
				data[1] = stretch->down[i].range;
			}
		}
		x = drv_num_nokey * 64 - (data[1] - data[0]);
		if (x <= 0)
			x = 1;
		if (x >= (int)drv_num_nokey * 64)
			x = drv_num_nokey * 64 - 1;

		point_now[id].other.x = x;
		point_now[id].other.y = y;
	}
}

static void PointStretch_for(int *dc_p, int *ds_p)
{
	static int save_dr[POINT_MAX], save_dn[POINT_MAX];
	int i, j;
	int dn;
	int dr;
	int *dc, *ds;
	int len = 8;

	dc = dc_p;
	ds = ds_p;
	for (i = 0; i < POINT_MAX; i++) {
		if (ps[1][i].all == 0) {
			for (j = 1; j < PS_DEEP; j++)
				ps[j][i].all = ps[0][i].all;
			save_dr[i] = 128;
			save_dn[i] = 0;
			continue;
		}
		if (id_flag.other.first_avg && point_delay[i].other.able == 0)
			continue;
		if ((point_shake & (0x1 << i)) == 0)
			continue;
		if (dc[len] == 3) /* dc == 2 */ {
			dn = pp[0][i].other.x > ps[1][i].other.x
				     ? pp[0][i].other.x - ps[1][i].other.x
				     : ps[1][i].other.x - pp[0][i].other.x;
			if (dn < ds[0]) {
				for (j = 0; j <= len; j++) {
					if (j == len || dn == 0) {
						ps[0][i].other.x =
							ps[1][i].other.x;
						break;
					} else if (ds[j] > dn &&
						   dn >= ds[j + 1]) {
						dr = dc[j + 1] +
						     ((dn - ds[j + 1]) *
						      (dc[j] - dc[j + 1])) /
							     (ds[j] -
							      ds[j + 1]);
						ps[0][i].other.x =
							(int)ps[1][i].other.x +
							(((int)pp[0][i]
								  .other.x -
							  (int)ps[1][i]
								  .other.x) *
								 dr +
							 64) / 128;
						break;
					}
				}
			}
			dn = pp[0][i].other.y > ps[1][i].other.y
				     ? pp[0][i].other.y - ps[1][i].other.y
				     : ps[1][i].other.y - pp[0][i].other.y;
			if (dn < ds[0]) {
				for (j = 0; j <= len; j++) {
					if (j == len || dn == 0) {
						ps[0][i].other.y =
							ps[1][i].other.y;
						break;
					} else if (ds[j] > dn &&
						   dn >= ds[j + 1]) {
						dr = dc[j + 1] +
						     ((dn - ds[j + 1]) *
						      (dc[j] - dc[j + 1])) /
							     (ds[j] -
							      ds[j + 1]);
						ps[0][i].other.y =
							(int)ps[1][i].other.y +
							(((int)pp[0][i]
								  .other.y -
							  (int)ps[1][i]
								  .other.y) *
								 dr +
							 64) / 128;
						break;
					}
				}
			}
		} else {
			dn = PointDistance(&pp[0][i], &ps[1][i]);
			dn = Sqrt(dn);
			if (dn >= ds[0])
				continue;

			if (dn < save_dn[i]) {
				dr = save_dr[i];
				save_dn[i] = dn;
				ps[0][i].other.x = (int)ps[1][i].other.x +
						   (((int)pp[0][i].other.x -
						     (int)ps[1][i].other.x) *
						    dr) / 128;
				ps[0][i].other.y = (int)ps[1][i].other.y +
						   (((int)pp[0][i].other.y -
						     (int)ps[1][i].other.y) *
						    dr) / 128;
				continue;
			}
			for (j = 0; j <= len; j++) {
				if (j == len || dn == 0) {
					ps[0][i].other.x = ps[1][i].other.x;
					ps[0][i].other.y = ps[1][i].other.y;
					break;
				} else if (ds[j] > dn && dn >= ds[j + 1]) {
					dr = dc[j + 1] +
					     ((dn - ds[j + 1]) *
					      (dc[j] - dc[j + 1])) /
						     (ds[j] - ds[j + 1]);
					save_dr[i] = dr;
					save_dn[i] = dn;
					ps[0][i].other.x =
						(int)ps[1][i].other.x +
						(((int)pp[0][i].other.x -
						  (int)ps[1][i].other.x) *
							 dr +
						 64) / 128;
					ps[0][i].other.y =
						(int)ps[1][i].other.y +
						(((int)pp[0][i].other.y -
						  (int)ps[1][i].other.y) *
							 dr +
						 64) / 128;
					break;
				}
			}
		}
	}
}

static void PointStretch(void)
{
	struct SHAKE_TYPE {
		int dis;
		int coe;
	};
	struct SHAKE_TYPE *shake_all = (struct SHAKE_TYPE *)shake_all_array;
	int i, j;
	int dn;
	int dr;
	int dc[9], ds[9];
	int len = 8;
	unsigned int temp;

	for (i = 0; i < POINT_MAX; i++)
		ps[0][i].all = pp[0][i].all;

	for (i = 0; i < POINT_MAX; i++) {
		if (pp[0][i].all == 0 || pp[0][i].other.key) {
			point_shake &= ~(0x1 << i);
			if (i == 0)
				point_edge.rate = 0;
			continue;
		}
		if (i == 0) {
			if (edge_first != 0 && ps[1][i].all == 0) {
				point_edge.coor.all = ps[0][i].all;
				if (point_edge.coor.other.x <
				    (unsigned int)((edge_first >> 24) & 0xff))
					point_edge.coor.other.x =
						((edge_first >> 24) & 0xff);
				if (point_edge.coor.other.x >
				    drv_num_nokey * 64 -
					    ((edge_first >> 16) & 0xff))
					point_edge.coor.other.x =
						drv_num_nokey * 64 -
						((edge_first >> 16) & 0xff);
				if (point_edge.coor.other.y <
				    (unsigned int)((edge_first >> 8) & 0xff))
					point_edge.coor.other.y =
						((edge_first >> 8) & 0xff);
				if (point_edge.coor.other.y >
				    sen_num_nokey * 64 -
					    ((edge_first >> 0) & 0xff))
					point_edge.coor.other.y =
						sen_num_nokey * 64 -
						((edge_first >> 0) & 0xff);
				if (point_edge.coor.all != ps[0][i].all) {
					point_edge.dis = PointDistance(
						&ps[0][i], &point_edge.coor);
					if (point_edge.dis)
						point_edge.rate = 0x1000;
				}
			}
			if (point_edge.rate != 0 && point_edge.dis != 0) {
				temp = PointDistance(&ps[0][i],
						     &point_edge.coor);
				if (temp >=
				    point_edge.dis * edge_first_coe / 0x80) {
					point_edge.rate = 0;
				} else if (temp > point_edge.dis) {
					temp = (point_edge.dis *
							edge_first_coe / 0x80 -
						temp) *
					       0x1000 / point_edge.dis;
					if (temp < point_edge.rate)
						point_edge.rate = temp;
				}
				ps[0][i].other.x =
					point_edge.coor.other.x +
					(ps[0][i].other.x -
					 point_edge.coor.other.x) *
						(0x1000 - point_edge.rate) /
						0x1000;
				ps[0][i].other.y =
					point_edge.coor.other.y +
					(ps[0][i].other.y -
					 point_edge.coor.other.y) *
						(0x1000 - point_edge.rate) /
						0x1000;
			}
		}
		if (ps[1][i].all == 0) {
			continue;
		} else if (id_flag.other.first_avg &&
			   (point_shake & (0x1 << i)) == 0 && pp[0][i].all &&
			   point_delay[i].other.able == 0 && shake_min != 0) {
			dn = 0;
			for (j = 1; j < PP_DEEP /* && j < PS_DEEP*/; j++) {
				if (pp[j][i].all == 0)
					break;
			}
			j--;
			dn = PointDistance(&ps[0][i], &ps[j][i]);
			if (PointDistance(&ps[0][i], &ps[j][i]) >=
			    (unsigned int)shake_min * 4) {
				point_delay[i].other.init = 1;
				point_delay[i].other.able = 1;
				point_delay[i].other.report = 1;
				point_delay[i].other.dele = 1;
			}
		} else if ((point_shake & (0x1 << i)) == 0) {
			if (PointDistance(&ps[0][i], &ps[1][i]) <
			    (unsigned int)shake_min) {
				if (point_delay[i].other.able)
					ps[0][i].all = ps[1][i].all;
				else {
					for (j = 1; j < PS_DEEP; j++)
						ps[j][i].all = ps[0][i].all;
					for (j = 0; j < PR_DEEP; j++)
						pr[j][i].all = ps[0][i].all;
				}
				continue;
			} else
				point_shake |= (0x1 << i);
		}
	}
	for (i = 0; i < len; i++) {
		if (shake_all[i].dis == 0) {
			len = i;
			break;
		}
	}
	if (len == 1) {
		ds[0] = shake_all[0].dis;
		dc[0] = (shake_all[0].coe * 100 + 64) / 128;
		for (i = 0; i < POINT_MAX; i++) {
			if (ps[1][i].all == 0) {
				for (j = 1; j < PS_DEEP; j++)
					ps[j][i].all = ps[0][i].all;
				continue;
			}
			if ((point_shake & (0x1 << i)) == 0)
				continue;
			dn = PointDistance(&pp[0][i], &ps[1][i]);
			dn = Sqrt(dn);
			dr = dn > ds[0] ? dn - ds[0] : 0;
			temp = ps[0][i].all;
			if (dn == 0 || dr == 0) {
				ps[0][i].other.x = ps[1][i].other.x;
				ps[0][i].other.y = ps[1][i].other.y;
			} else {
				ps[0][i].other.x = (int)ps[1][i].other.x +
						   ((int)pp[0][i].other.x -
						    (int)ps[1][i].other.x) *
							   dr / dn;
				ps[0][i].other.y = (int)ps[1][i].other.y +
						   ((int)pp[0][i].other.y -
						    (int)ps[1][i].other.y) *
							   dr / dn;
			}
			if (dc[0] > 0) {
				if (ps[0][i].all == ps[1][i].all &&
				    temp != ps[0][i].all) {
					ps[0][i].all = temp;
					point_decimal[i].other.x +=
						ps[0][i].other.x -
						ps[1][i].other.x;
					point_decimal[i].other.y +=
						ps[0][i].other.y -
						ps[1][i].other.y;
					ps[0][i].other.x = ps[1][i].other.x;
					ps[0][i].other.y = ps[1][i].other.y;
					if (point_decimal[i].other.x > dc[0] &&
					    ps[1][i].other.x < 0xffff) {
						ps[0][i].other.x += 1;
						point_decimal[i].other.x = 0;
					}
					if (point_decimal[i].other.x < -dc[0] &&
					    ps[1][i].other.x > 0) {
						ps[0][i].other.x -= 1;
						point_decimal[i].other.x = 0;
					}
					if (point_decimal[i].other.y > dc[0] &&
					    ps[1][i].other.y < 0xfff) {
						ps[0][i].other.y += 1;
						point_decimal[i].other.y = 0;
					}
					if (point_decimal[i].other.y < -dc[0] &&
					    ps[1][i].other.y > 0) {
						ps[0][i].other.y -= 1;
						point_decimal[i].other.y = 0;
					}
				} else {
					point_decimal[i].other.x = 0;
					point_decimal[i].other.y = 0;
				}
			}
		}

	} else if (len >= 2) {
		temp = 0;
		for (i = 0; i < POINT_MAX; i++)
			if (pp[0][i].all)
				temp++;
		if (temp > 5)
			temp = 5;
		for (i = 0; i < 8 && i < len; i++) {
			if (stretch_mult)
				ds[i + 1] = shake_all[i].dis *
					    (stretch_mult *
						     (temp > 1 ? temp - 1 : 0) +
					     0x80) /
					    0x80;
			else
				ds[i + 1] = shake_all[i].dis;
			dc[i + 1] =
				shake_all[i]
					.coe; /* ;ds[i+1] * shake_all[i].coe; */
		}
		if (shake_all[0].coe >= 128 ||
		    shake_all[0].coe <= shake_all[1].coe) {
			ds[0] = ds[1];
			dc[0] = dc[1];
		} else {
			ds[0] = ds[1] +
				(128 - shake_all[0].coe) * (ds[1] - ds[2]) /
					(shake_all[0].coe - shake_all[1].coe);
			dc[0] = 128;
		}
		PointStretch_for(dc, ds);
	} else {
		return;
	}
}

static void ResetMask(void)
{
	if (reset_mask_send)
		reset_mask_send = 0;

	if (global_state.other.mask)
		return;
	if (reset_mask_dis == 0 || reset_mask_type == 0)
		return;
	if (reset_mask_max == 0xfffffff1) {
		if (point_num == 0)
			reset_mask_max = 0xf0000000 + 1;
		return;
	}
	if (reset_mask_max > 0xf0000000) {
		reset_mask_max--;
		if (reset_mask_max == 0xf0000000) {
			reset_mask_send = reset_mask_type;
			global_state.other.mask = 1;
		}
		return;
	}
	if (point_num > 1 || pp[0][0].all == 0) {
		reset_mask_count = 0;
		reset_mask_max = 0;
		reset_mask_count = 0;
		return;
	}
	reset_mask_count++;
	if (reset_mask_max == 0)
		reset_mask_max = pp[0][0].all;
	else if (PointDistance((union gsl_POINT_TYPE *)(&reset_mask_max),
			       pp[0]) >
			 (((unsigned int)reset_mask_dis) & 0xffffff) &&
		 reset_mask_count > (((unsigned int)reset_mask_dis) >> 24))
		reset_mask_max = 0xfffffff1;
}

static int ConfigCoorMulti(unsigned int data[])
{
	int i, j;
	int n = 0;

	for (i = 0; i < 4; i++) {
		if (data[247 + i] != 0) {
			if ((data[247 + i] & 63) == 0 &&
			    (data[247 + i] >> 16) < 4)
				n++;
			else
				return FALSE;
		}
		if (data[251 + i] != 0) {
			if ((data[251 + i] & 63) == 0 &&
			    (data[251 + i] >> 16) < 4)
				n++;
			else
				return FALSE;
		}
	}
	if (n == 0 || n > 4)
		return FALSE;
	for (j = 0; j < n; j++) {
		for (i = 0; i < 64; i++) {
			if (data[256 + j * 64 + i] >= 64)
				return FALSE;
			if (i) {
				if (data[256 + j * 64 + i] <
				    data[256 + j * 64 + i - 1])
					return FALSE;
			}
		}
	}
	return TRUE;
}

static int ConfigFilter(unsigned int data[])
{
	int i;
	unsigned int ps_c[8];
	unsigned int pr_c[8];
	unsigned int sum = 0;
	/* if(data[242]>1 && (data[255]>=0 && data[255]<=256)) */
	if (data[242] > 1 && (data[255] <= 256)) {
		for (i = 0; i < 8; i++) {
			ps_c[i] = (data[243 + i / 4] >> ((i % 4) * 8)) & 0xff;
			pr_c[i] =
				(data[243 + i / 4 + 2] >> ((i % 4) * 8)) & 0xff;
			if (ps_c[i] >= 0x80)
				ps_c[i] |= 0xffffff00;
			if (pr_c[i] >= 0x80)
				pr_c[i] |= 0xffffff00;
			sum += ps_c[i];
			sum += pr_c[i];
		}
		if (sum == data[242] || sum + data[242] == 0)
			return TRUE;
	}
	return FALSE;
}

static int ConfigKeyMap(unsigned int data[])
{
	int i;

	if (data[217] != 1)
		return FALSE;
	for (i = 0; i < 8; i++) {
		if (data[218 + 2] == 0)
			return FALSE;
		if ((data[218 + i * 3 + 0] >> 16) >
		    (data[218 + i * 3 + 0] & 0xffff))
			return FALSE;
		if ((data[218 + i * 3 + 1] >> 16) >
		    (data[218 + i * 3 + 1] & 0xffff))
			return FALSE;
	}
	return TRUE;
}

static int DiagonalDistance(union gsl_POINT_TYPE *p, int type)
{
	int divisor, square;

	divisor = ((int)sen_num_nokey * (int)sen_num_nokey +
		   (int)drv_num_nokey * (int)drv_num_nokey) /
		  16;
	if (divisor == 0)
		divisor = 1;
	if (type == 0)
		square = ((int)sen_num_nokey * (int)(p->other.x) -
			  (int)drv_num_nokey * (int)(p->other.y)) /
			 4;
	else
		square = ((int)sen_num_nokey * (int)(p->other.x) +
			  (int)drv_num_nokey * (int)(p->other.y) -
			  (int)sen_num_nokey * (int)drv_num_nokey * 64) /
			 4;
	return square * square / divisor;
}

static void DiagonalCompress(union gsl_POINT_TYPE *p, int type, int dis,
			     int dis_max)
{
	int x, y;
	int tx, ty;
	int cp_ceof;

	if (dis_max == 0)
		return;
	if (dis > dis_max)
		cp_ceof = (dis - dis_max) * 128 / (3 * dis_max) + 128;
	else
		cp_ceof = 128;
	if (cp_ceof > 256)
		cp_ceof = 256;
	x = p->other.x;
	y = p->other.y;
	if (type)
		y = (int)sen_num_nokey * 64 - y;
	x *= (int)sen_num_nokey;
	y *= (int)drv_num_nokey;
	tx = x;
	ty = y;
	x = ((tx + ty) + (tx - ty) * cp_ceof / 256) / 2;
	y = ((tx + ty) + (ty - tx) * cp_ceof / 256) / 2;
	x /= (int)sen_num_nokey;
	y /= (int)drv_num_nokey;
	if (type)
		y = sen_num_nokey * 64 - y;
	if (x < 1)
		x = 1;
	if (y < 1)
		y = 1;
	if (x >= (int)drv_num_nokey * 64)
		x = drv_num_nokey * 64 - 1;
	if (y >= (int)sen_num_nokey * 64)
		y = (int)sen_num_nokey * 64 - 1;
	p->other.x = x;
	p->other.y = y;
}

static void PointDiagonal(void)
{
	int i;
	int diagonal_size;
	int dis;
	unsigned int diagonal_start;

	if (diagonal == 0)
		return;
	diagonal_size = diagonal * diagonal;
	diagonal_start = diagonal * 3 / 2;
	for (i = 0; i < POINT_MAX; i++) {
		if (ps[0][i].all == 0 || ps[0][i].other.key != 0) {
			point_corner &= ~(0x3 << i * 2);
			continue;
		} else if ((point_corner & (0x3 << i * 2)) == 0) {
			if ((ps[0][i].other.x <= diagonal_start &&
			     ps[0][i].other.y <= diagonal_start) ||
			    (ps[0][i].other.x >=
				     drv_num_nokey * 64 - diagonal_start &&
			     ps[0][i].other.y >=
				     sen_num_nokey * 64 - diagonal_start))
				point_corner |= 0x2 << i * 2;
			else if ((ps[0][i].other.x <= diagonal_start &&
				  ps[0][i].other.y >= sen_num_nokey * 64 -
							      diagonal_start) ||
				 (ps[0][i].other.x >=
					  drv_num_nokey * 64 - diagonal_start &&
				  ps[0][i].other.y <= diagonal_start))
				point_corner |= 0x3 << i * 2;
			else
				point_corner |= 0x1 << i * 2;
		}
		if (point_corner & (0x2 << i * 2)) {
			dis = DiagonalDistance(&(ps[0][i]),
					       point_corner & (0x1 << i * 2));
			if (dis <= diagonal_size * 4) {
				DiagonalCompress(&(ps[0][i]),
						 point_corner & (0x1 << i * 2),
						 dis, diagonal_size);
			} else if (dis > diagonal_size * 4) {
				point_corner &= ~(0x3 << i * 2);
				point_corner |= 0x1 << i * 2;
			}
		}
	}
}

static int PointSlope(int i, int j)
{
	int x, y;

	x = pr[j][i].other.x - pr[j + 1][i].other.x;
	x = x * x;
	y = pr[j][i].other.y - pr[j + 1][i].other.y;
	y = y * y;
	if (x + y == 0)
		return -1;
	if (x > y)
		return x * 1024 / (x + y);
	else
		return y * 1024 / (x + y);
}

static void PointExtend(void)
{
	int i, j;
	int x, y;
	int t, t2;
	int extend_len = 5;

	if (point_extend == 0)
		return;
	for (i = 0; i < POINT_MAX; i++) {
		if (pr[0][i].other.fill == 0)
			continue;
		for (j = 0; j < extend_len; j++) {
			if (pr[j][i].all == 0)
				break;
		}
		if (j < extend_len)
			continue;
		if (PointDistance(&pr[1][i], &pr[2][i]) < 16 * 16)
			continue;
		t = PointSlope(i, 1);
		for (j = 2; j < extend_len - 1; j++) {
			t2 = PointSlope(i, j);
			if (t2 < 0 || t2 < t * (128 - point_extend) / 128 ||
			    t2 > t * (128 + point_extend) / 128)
				break;
		}
		if (j < extend_len - 1)
			continue;
		x = 3 * pr[1][i].other.x - 2 * pr[2][i].other.x;
		y = 3 * pr[1][i].other.y - 2 * pr[2][i].other.y;
		pr[0][i].all =
			PointRange(x, y, pr[1][i].other.x, pr[1][i].other.y);
	}
}

static void PressureSave(void)
{
	int i;

	if ((point_num & 0x1000) == 0) {
		for (i = 0; i < POINT_MAX; i++) {
			pressure_now[i] = 0;
			pressure_report[i] = 0;
		}
		return;
	}
	for (i = 0; i < POINT_MAX; i++) {
		pressure_now[i] = point_now[i].all >> 28;
		point_now[i].all &= ~(0xf << 28);
	}
}

static void PointPressure(void)
{
	int i, j;

	for (i = 0; i < POINT_MAX; i++) {
		if (pa[0][i] != 0 && pa[1][i] == 0) {
			pressure_report[i] = pa[0][i] * 5;
			for (j = 1; j < PRESSURE_DEEP; j++)
				pa[j][i] = pa[0][i];
			continue;
		}
		j = (pressure_report[i] + 1) / 2 + pa[0][i] + pa[1][i] +
		    (pa[2][i] + 1) / 2 - pressure_report[i];
		if (j >= 2)
			j -= 2;
		else if (j <= -2)
			j += 2;
		else
			j = 0;
		pressure_report[i] = pressure_report[i] + j;
	}
}

static void PressMask(void)
{
	int i, j;
	unsigned int press_max = press_mask & 0xff;
	unsigned int press_range_s = (press_mask >> 8) & 0xff;
	unsigned int press_range_d = (press_mask >> 16) & 0xff;
	unsigned int press_range;

	if (press_max == 0)
		return;
	for (i = 0; i < POINT_MAX; i++) {
		if (point_delay[i].other.able == 0) {
			point_delay[i].other.pres = 0;
			continue;
		}
		if (point_delay[i].other.delay >= 1 &&
		    point_delay[i].other.pres == 0) {
			if (pa[0][i] > pa[1][i])
				point_delay[i].other.able = 0;
			else
				point_delay[i].other.pres = 1;
		}
	}
	for (i = 0; i < POINT_MAX; i++) {
		if (pr[0][i].all == 0)
			continue;
		if (point_delay[i].other.mask == 0 &&
		    pressure_report[i] < press_max + 7)
			continue;
		point_delay[i].other.able = 0;
		point_delay[i].other.mask = 1;
		press_range = press_range_s * 64;
		if (pressure_report[i] > 7 + press_max)
			press_range += (pressure_report[i] - 7 - press_max) *
				       press_range_d;
		if (press_range == 0)
			continue;
		for (j = 0; j < POINT_MAX; j++) {
			if (i == j)
				continue;
			if (pr[0][j].all == 0 || point_delay[j].other.able == 0)
				continue;

			if (PointDistance(&pp[0][i], &pp[0][j]) <
			    press_range * press_range)
				point_delay[j].other.able = 0;
		}
	}
}

static void PressMove(void)
{
	int i;
	/* POINT_TYPE_ID point_press_move; */
	/* unsigned int press_move=0x01000010; */
	if (press_move == 0)
		return;
	if (pr[0][0].all == 0)
		goto press_move_err;
	for (i = 1; i < POINT_MAX; i++) {
		if (pr[0][i].all)
			goto press_move_err;
	}
	if (pressure_report[0] < (press_move & 0xff) + 7)
		goto press_move_err;
	if (point_press_move.all == 0) {
		point_press_move.all = pr[0][0].all;
	} else if (point_press_move.other.x && point_press_move.other.y) {
		if (PointDistance(&point_press_move, &pr[0][0]) >
		    (press_move >> 16) * (press_move >> 16)) {
			/* #define	x0		point_press_move.x */
			/* #define	y0		point_press_move.y */
			/* #define	x1		pr[0][0].x */
			/* #define	y1		pr[0][0].y */
			/* if(x1<x0 && y1<y0+(x0-x1) && y1+(x0-x1)>y0) */
			/* press_move = 1; */
			/* if(x1>x0 && y1<y0+(x1-x0) && y1+(x1-x0)>y0) */
			/* press_move = 2; */
			/* if(y1<y0 && x1<x0+(y0-y1) && x1+(y0-y1)>x0) */
			/* press_move = 3; */
			/* if(y1>y0 && x1<x0+(y1-y0) && x1+(y1-y0)>x0) */
			/* press_move = 4; */
			if (pr[0][0].other.x < point_press_move.other.x &&
			    pr[0][0].other.y <
				    point_press_move.other.y +
					    (point_press_move.other.x -
					     pr[0][0].other.x) &&
			    pr[0][0].other.y + (point_press_move.other.x -
						pr[0][0].other.x) >
				    point_press_move.other.y)
				point_press_move.all = 1;
			else if (pr[0][0].other.x > point_press_move.other.x &&
				 pr[0][0].other.y <
					 point_press_move.other.y +
						 (pr[0][0].other.x -
						  point_press_move.other.x) &&
				 pr[0][0].other.y + (pr[0][0].other.x -
						     point_press_move.other.x) >
					 point_press_move.other.y)
				point_press_move.all = 2;
			else if (pr[0][0].other.y < point_press_move.other.y &&
				 pr[0][0].other.x <
					 point_press_move.other.x +
						 (point_press_move.other.y -
						  pr[0][0].other.y) &&
				 pr[0][0].other.x + (point_press_move.other.y -
						     pr[0][0].other.y) >
					 point_press_move.other.x)
				point_press_move.all = 3;
			else if (pr[0][0].other.y > point_press_move.other.y &&
				 pr[0][0].other.x <
					 point_press_move.other.x +
						 (pr[0][0].other.y -
						  point_press_move.other.y) &&
				 pr[0][0].other.x + (pr[0][0].other.y -
						     point_press_move.other.y) >
					 point_press_move.other.x)
				point_press_move.all = 4;
		}
	} else {
	}
	return;
press_move_err:
	point_press_move.all = 0;
}

int gsl_PressMove(void)
{
	if (point_press_move.all <= 4)
		return point_press_move.all;
	else
		return 0;
}
/* EXPORT_SYMBOL(gsl_PressMove); */

void gsl_ReportPressure(unsigned int *p)
{
	int i;

	for (i = 0; i < POINT_MAX; i++) {
		if (i < point_num) {
			if (pressure_now[i] == 0)
				p[i] = 0;
			else if (pressure_now[i] <= 7)
				p[i] = 1;
			else if (pressure_now[i] > 63 + 7)
				p[i] = 63;
			else
				p[i] = pressure_now[i] - 7;
		} else
			p[i] = 0;
	}
}
/* EXPORT_SYMBOL(gsl_ReportPressure); */

int gsl_TouchNear(void)
{
	return 0;
}
/* EXPORT_SYMBOL(gsl_TouchNear); */

static void gsl_id_reg_init(int flag)
{
	int i, j;

	for (j = 0; j < POINT_DEEP; j++)
		for (i = 0; i < POINT_MAX; i++)
			point_array[j][i].all = 0;
	for (j = 0; j < PRESSURE_DEEP; j++)
		for (i = 0; i < POINT_MAX; i++)
			pressure_array[j][i] = 0;
	for (i = 0; i < POINT_MAX; i++) {
		point_delay[i].all = 0;
		filter_deep[i] = 0;
		point_decimal[i].all = 0;
	}
	for (i = 0; i < AVG_DEEP; i++)
		avg[i] = 0;
	point_edge.rate = 0;
	point_n = 0;
	if (flag)
		point_num = 0;
	prev_num = 0;
	point_shake = 0;
	reset_mask_send = 0;
	reset_mask_max = 0;
	reset_mask_count = 0;
	point_near = 0;
	point_corner = 0;
	global_state.all = 0;
	inte_count = 0;
	csensor_count = 0;
	point_press_move.all = 0;
	global_state.other.cc_128 = 0;
	prec_id.all = 0;
	for (i = 0; i < 64; i++) {
		if (coordinate_correct_coe_x[i] > 64 ||
		    coordinate_correct_coe_y[i] > 64) {
			global_state.other.cc_128 = 1;
			break;
		}
	}
}

static int DataCheck(void)
{
	if (drv_num == 0 || drv_num_nokey == 0 || sen_num == 0 ||
	    sen_num_nokey == 0)
		return 0;
	if (screen_x_max == 0 || screen_y_max == 0)
		return 0;
	return 1;
}

void gsl_DataInit(unsigned int *conf_in)
{
	ESP_LOGI(TAG,"gsl_DataInit");
	int i, j;
	unsigned int *conf;
	int len;

	gsl_id_reg_init(1);
	for (i = 0; i < POINT_MAX; i++)
		point_now[i].all = 0;
	conf = config_static;
	coordinate_correct_able = 0;
	for (i = 0; i < 32; i++) {
		coordinate_correct_coe_x[i] = i;
		coordinate_correct_coe_y[i] = i;
	}
	id_first_coe = 8;
	id_speed_coe = 128 * 128;
	id_static_coe = 64 * 64;
	average = 3 + 1;
	soft_average = 3;
	report_delay = 0;
	delay_key = 0;
	report_ahead = 0x9249249;
	report_delete = 0;

	for (i = 0; i < 4; i++)
		median_dis[i] = 0;
	shake_min = 0 * 0;
	for (i = 0; i < 2; i++) {
		match_y[i] = 0;
		match_x[i] = 0;
		ignore_y[i] = 0;
		ignore_x[i] = 0;
	}
	match_y[0] = 4096;
	match_x[0] = 4096;
	screen_y_max = 480;
	screen_x_max = 800;
	point_num_max = 10;
	drv_num = 16;
	sen_num = 10;
	drv_num_nokey = 16;
	sen_num_nokey = 10;
	for (i = 0; i < 4; i++)
		edge_cut[i] = 0;
	for (i = 0; i < 32; i++)
		stretch_array[i] = 0;
	for (i = 0; i < 16; i++)
		shake_all_array[i] = 0;
	reset_mask_dis = 0;
	reset_mask_type = 0;
	edge_start = 0;
	diagonal = 0;
	point_extend = 0;
	key_map_able = 0;
	for (i = 0; i < 8 * 3; i++)
		key_range_array[i] = 0;
	filter_able = 0;
	filter_coe[0] = (0 << 6 * 4) + (0 << 6 * 3) + (0 << 6 * 2) +
			(40 << 6 * 1) + (24 << 6 * 0);
	filter_coe[1] = (0 << 6 * 4) + (0 << 6 * 3) + (16 << 6 * 2) +
			(24 << 6 * 1) + (24 << 6 * 0);
	filter_coe[2] = (0 << 6 * 4) + (16 << 6 * 3) + (24 << 6 * 2) +
			(16 << 6 * 1) + (8 << 6 * 0);
	filter_coe[3] = (6 << 6 * 4) + (16 << 6 * 3) + (24 << 6 * 2) +
			(12 << 6 * 1) + (6 << 6 * 0);
	for (i = 0; i < 4; i++) {
		multi_x_array[i] = 0;
		multi_y_array[i] = 0;
	}
	point_repeat[0] = 32;
	point_repeat[1] = 96;
	edge_first = 0;
	edge_first_coe = 0x80;
	id_flag.all = 0;
	press_mask = 0;
	press_move = 0;
	stretch_mult = 0;
	/* ---------------------------------------------- */
	if (conf_in == NULL)
		return;

	if (conf_in[0] <= 0xfff) {
		if (ConfigCoorMulti(conf_in))
			len = 512;
		else if (ConfigFilter(conf_in))
			len = 256;
		else if (ConfigKeyMap(conf_in))
			len = 241;
		else
			len = 215;
	} else if (conf_in[1] <= CONFIG_LENGTH)
		len = conf_in[1];
	else
		len = CONFIG_LENGTH;
	for (i = 0; i < len; i++)
		conf[i] = conf_in[i];
	for (; i < CONFIG_LENGTH; i++)
		conf[i] = 0;
	if (conf_in[0] <= 0xfff) {
		coordinate_correct_able = conf[0];
		drv_num = conf[1];
		sen_num = conf[2];
		drv_num_nokey = conf[3];
		sen_num_nokey = conf[4];
		id_first_coe = conf[5];
		id_speed_coe = conf[6];
		id_static_coe = conf[7];
		average = conf[8];
		soft_average = conf[9];

		report_delay = conf[13];
		shake_min = conf[14];
		screen_y_max = conf[15];
		screen_x_max = conf[16];
		point_num_max = conf[17];
		global_flag.all = conf[18];
		for (i = 0; i < 4; i++)
			median_dis[i] = (unsigned char)conf[19 + i];
		for (i = 0; i < 2; i++) {
			match_y[i] = conf[23 + i];
			match_x[i] = conf[25 + i];
			ignore_y[i] = conf[27 + i];
			ignore_x[i] = conf[29 + i];
		}
		for (i = 0; i < 64; i++) {
			coordinate_correct_coe_x[i] = conf[31 + i];
			coordinate_correct_coe_y[i] = conf[95 + i];
		}
		for (i = 0; i < 4; i++)
			edge_cut[i] = conf[159 + i];
		for (i = 0; i < 32; i++)
			stretch_array[i] = conf[163 + i];
		for (i = 0; i < 16; i++)
			shake_all_array[i] = conf[195 + i];
		reset_mask_dis = conf[213];
		reset_mask_type = conf[214];
		edge_start = conf[216];
		key_map_able = conf[217];
		for (i = 0; i < 8 * 3; i++)
			key_range_array[i] = conf[218 + i];
		filter_able = conf[242];
		for (i = 0; i < 4; i++)
			filter_coe[i] = conf[243 + i];
		for (i = 0; i < 4; i++)
			multi_x_array[i] = conf[247 + i];
		for (i = 0; i < 4; i++)
			multi_y_array[i] = conf[251 + i];
		diagonal = conf[255];
		for (j = 0; j < 4; j++)
			for (i = 0; i < 64; i++)
				multi_group[j][i] = conf[256 + i + j * 64];
		for (j = 0; j < 4; j++) {
			for (i = 0; i < 8; i++) {
				ps_coe[j][i] = conf[256 + 64 * 3 + i + j * 8];
				pr_coe[j][i] =
					conf[256 + 64 * 3 + i + j * 8 + 32];
			}
		}
		/* ----------------------- */
		/* near_set[0] = 0; */
		/* near_set[1] = 0; */
	} else {
		global_flag.all = conf[0x10];
		point_num_max = conf[0x11];
		drv_num = conf[0x12] & 0xffff;
		sen_num = conf[0x12] >> 16;
		drv_num_nokey = conf[0x13] & 0xffff;
		sen_num_nokey = conf[0x13] >> 16;
		screen_x_max = conf[0x14] & 0xffff;
		screen_y_max = conf[0x14] >> 16;
		average = conf[0x15];
		reset_mask_dis = conf[0x16];
		reset_mask_type = conf[0x17];
		point_repeat[0] = conf[0x18] >> 16;
		point_repeat[1] = conf[0x18] & 0xffff;
		/* conf[0x19~0x1f] */
		/* near_set[0] = conf[0x19]>>16; */
		/* near_set[1] = conf[0x19]&0xffff; */
		diagonal = conf[0x1a];
		point_extend = conf[0x1b];
		edge_start = conf[0x1c];
		press_move = conf[0x1d];
		press_mask = conf[0x1e];
		id_flag.all = conf[0x1f];
		/* ------------------------- */

		id_first_coe = conf[0x20];
		id_speed_coe = conf[0x21];
		id_static_coe = conf[0x22];
		match_y[0] = conf[0x23] >> 16;
		match_y[1] = conf[0x23] & 0xffff;
		match_x[0] = conf[0x24] >> 16;
		match_x[1] = conf[0x24] & 0xffff;
		ignore_y[0] = conf[0x25] >> 16;
		ignore_y[1] = conf[0x25] & 0xffff;
		ignore_x[0] = conf[0x26] >> 16;
		ignore_x[1] = conf[0x26] & 0xffff;
		edge_cut[0] = (conf[0x27] >> 24) & 0xff;
		edge_cut[1] = (conf[0x27] >> 16) & 0xff;
		edge_cut[2] = (conf[0x27] >> 8) & 0xff;
		edge_cut[3] = (conf[0x27] >> 0) & 0xff;
		report_delay = conf[0x28];
		shake_min = conf[0x29];
		for (i = 0; i < 16; i++) {
			stretch_array[i * 2 + 0] = conf[0x2a + i] & 0xffff;
			stretch_array[i * 2 + 1] = conf[0x2a + i] >> 16;
		}
		for (i = 0; i < 8; i++) {
			shake_all_array[i * 2 + 0] = conf[0x3a + i] & 0xffff;
			shake_all_array[i * 2 + 1] = conf[0x3a + i] >> 16;
		}
		report_ahead = conf[0x42];
		/* key_dead_time			= conf[0x43]; */
		/* point_dead_time			= conf[0x44]; */
		/* point_dead_time2		= conf[0x45]; */
		/* point_dead_distance		= conf[0x46]; */
		/* point_dead_distance2	= conf[0x47]; */
		edge_first = conf[0x48];
		edge_first_coe = conf[0x49];
		delay_key = conf[0x4a];
		report_delete = conf[0x4b];
		stretch_mult = conf[0x4c];

		for (i = 0; i < 16; i++) {
			stretch_active[i * 2 + 0] = conf[0x50 + i] & 0xffff;
			stretch_active[i * 2 + 1] = conf[0x50 + i] >> 16;
		}
		/* goto_test */

		key_map_able = conf[0x60];
		for (i = 0; i < 8 * 3; i++)
			key_range_array[i] = conf[0x61 + i];

		coordinate_correct_able = conf[0x100];
		for (i = 0; i < 4; i++) {
			multi_x_array[i] = conf[0x101 + i];
			multi_y_array[i] = conf[0x105 + i];
		}
		for (i = 0; i < 64; i++) {
			coordinate_correct_coe_x[i] =
				(conf[0x109 + i / 4] >> (i % 4 * 8)) & 0xff;
			coordinate_correct_coe_y[i] =
				(conf[0x109 + 64 / 4 + i / 4] >> (i % 4 * 8)) &
				0xff;
		}
		for (j = 0; j < 4; j++)
			for (i = 0; i < 64; i++)
				multi_group[j][i] = (conf[0x109 + 64 / 4 * 2 +
							  (i + j * 64) / 4] >>
						     ((i + j * 64) % 4 * 8)) &
						    0xff;

		filter_able = conf[0x180];
		for (i = 0; i < 4; i++)
			filter_coe[i] = conf[0x181 + i];
		for (i = 0; i < 4; i++)
			median_dis[i] = (unsigned char)conf[0x185 + i];
		for (j = 0; j < 4; j++) {
			for (i = 0; i < 8; i++) {
				ps_coe[j][i] = conf[0x189 + i + j * 8];
				pr_coe[j][i] = conf[0x189 + i + j * 8 + 32];
			}
		}
	}
	/* --------------------------------------------- */
	gsl_id_reg_init(0);
	/* --------------------------------------------- */
	if (average == 0)
		average = 4;
	for (i = 0; i < 8; i++) {
		if (shake_all_array[i * 2] & 0x8000)
			shake_all_array[i * 2] =
				shake_all_array[i * 2] & ~0x8000;
		else
			shake_all_array[i * 2] = Sqrt(shake_all_array[i * 2]);
	}
	for (i = 0; i < 2; i++) {
		if (match_x[i] & 0x8000)
			match_x[i] |= 0xffff0000;
		if (match_y[i] & 0x8000)
			match_y[i] |= 0xffff0000;
		if (ignore_x[i] & 0x8000)
			ignore_x[i] |= 0xffff0000;
		if (ignore_y[i] & 0x8000)
			ignore_y[i] |= 0xffff0000;
	}
	for (i = 0; i < CONFIG_LENGTH; i++)
		config_static[i] = 0;
}


unsigned int gsl_version_id(void)
{
	return GSL_VERSION;
}


unsigned int gsl_mask_tiaoping(void)
{
	// printf("reset_mask_send:%d\r\n",reset_mask_send);
	return reset_mask_send;
}

static void GetFlag(void)
{
	int i = 0;
	int num_save;

	for (i = AVG_DEEP - 1; i; i--)
		avg[i] = avg[i - 1];
	avg[0] = 0;
	if ((point_num & 0x8000) != 0) {

		if ((point_num & 0xff000000) == 0x59000000)
			avg[0] = (point_num >> 16) & 0xff;
	}
	if (((point_num & 0x100) != 0) ||
	    ((point_num & 0x200) != 0 && global_state.other.reset == 1)) {
		gsl_id_reg_init(0);
	}
	if ((point_num & 0x300) == 0)
		global_state.other.reset = 1;

	if (point_num & 0x400)
		global_state.other.only = 1;
	else
		global_state.other.only = 0;
	if (point_num & 0x2000)
		global_state.other.interpolation = INTE_INIT;
	else if (global_state.other.interpolation)
		global_state.other.interpolation--;
	if (point_num & 0x4000)
		global_state.other.ex = 1;
	else
		global_state.other.ex = 0;
	if ((point_num & 0xff) != 0) {
		global_state.other.active_prev = global_state.other.active;
		if ((point_num & 0x800) != 0)
			global_state.other.active = 1;
		else
			global_state.other.active = 0;
		if (global_state.other.active !=
		    global_state.other.active_prev) {
			if (global_state.other.active) {
				if (prec_id.other.num)
					gsl_id_reg_init(1);
				else
					gsl_id_reg_init(0);
				global_state.other.active = 1;
				global_state.other.active_prev = 1;
			} else
				gsl_id_reg_init(0);
		}
	}
	inte_count++;
	csensor_count = ((unsigned int)point_num) >> 16;
	num_save = point_num & 0xff;
	if (num_save > POINT_MAX)
		num_save = POINT_MAX;
	for (i = 0; i < POINT_MAX; i++) {
		if (i >= num_save)
			point_now[i].all = 0;
	}
	point_num = (point_num & (~0xff)) + num_save;
}

static void PointIgnore(void)
{
	int i, x, y;

	if (id_flag.other.ignore_pri == 0)
		return;
	for (i = 0; i < point_num; i++) {
		if (point_now[i].other.key)
			continue;
		y = point_now[i].other.y * (int)screen_y_max /
		    ((int)sen_num_nokey * 64);
		x = point_now[i].other.x * (int)screen_x_max /
		    ((int)drv_num_nokey * 64);
		if ((ignore_y[0] != 0 || ignore_y[1] != 0)) {
			if (y < ignore_y[0])
				point_now[i].all = 0;
			if (ignore_y[1] <= screen_y_max / 2 &&
			    y > screen_y_max - ignore_y[1])
				point_now[i].all = 0;
			if (ignore_y[1] >= screen_y_max / 2 && y > ignore_y[1])
				point_now[i].all = 0;
		}
		if (ignore_x[0] != 0 || ignore_x[1] != 0) {
			if (x < ignore_x[0])
				point_now[i].all = 0;
			if (ignore_x[1] <= screen_x_max / 2 &&
			    x > screen_x_max - ignore_x[1])
				point_now[i].all = 0;
			if (ignore_x[1] >= screen_x_max / 2 && x > ignore_x[1])
				point_now[i].all = 0;
		}
	}
	x = 0;
	for (i = 0; i < point_num; i++) {
		if (point_now[i].all == 0)
			continue;
		point_now[x++] = point_now[i];
	}
	point_num = x;
}

void gsl_alg_id_main(struct gsl_touch_info *cinfo)
{
	int i;
	// ESP_LOGI(TAG,"gsl_alg_id_main");
	point_num = cinfo->finger_num;
	for (i = 0; i < POINT_MAX; i++)
		point_now[i].all = (cinfo->id[i] << 28) | (cinfo->x[i] << 16) |
				   cinfo->y[i];

	GetFlag();
	if (DataCheck() == 0) {
		point_num = 0;
		cinfo->finger_num = 0;
		return;
	}
	PressureSave();
	point_num &= 0xff;
	PointIgnore();
	PointCoor();
	CoordinateCorrect();
	PointEdge();
	PointRound();
	PointRepeat();
	GetPointNum(point_now);
	PointPointer();
	PointPredict();
	PointId();
	PointNewId();
	PointOrder();
	PointCross();
	GetPointNum(pp[0]);

	prev_num = point_num;
	ResetMask();
	PointStretch();
	PointDiagonal();
	PointFilter();
	GetPointNum(pr[0]);

	PointDelay();
	PointMenu();
	PointExtend();
	PointPressure();
	PressMove();
	PressMask();
	PointReport(cinfo);
}

