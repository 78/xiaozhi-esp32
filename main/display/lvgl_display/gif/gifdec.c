#include "gifdec.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <esp_log.h>

#define TAG "GIF"

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

typedef struct Entry {
    uint16_t length;
    uint16_t prefix;
    uint8_t  suffix;
} Entry;

typedef struct Table {
    int bulk;
    int nentries;
    Entry * entries;
} Table;

#if LV_GIF_CACHE_DECODE_DATA
#define LZW_MAXBITS                 12
#define LZW_TABLE_SIZE              (1 << LZW_MAXBITS)
#define LZW_CACHE_SIZE              (LZW_TABLE_SIZE * 4)
#endif

static gd_GIF  * gif_open(gd_GIF * gif);
static bool f_gif_open(gd_GIF * gif, const void * path, bool is_file);
static void f_gif_read(gd_GIF * gif, void * buf, size_t len);
static int f_gif_seek(gd_GIF * gif, size_t pos, int k);
static void f_gif_close(gd_GIF * gif);

#if LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_HELIUM
    #include "gifdec_mve.h"
#endif

static uint16_t
read_num(gd_GIF * gif)
{
    uint8_t bytes[2];

    f_gif_read(gif, bytes, 2);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}

gd_GIF *
gd_open_gif_file(const char * fname)
{
    gd_GIF gif_base;
    memset(&gif_base, 0, sizeof(gif_base));

    bool res = f_gif_open(&gif_base, fname, true);
    if(!res) return NULL;

    return gif_open(&gif_base);
}

gd_GIF *
gd_open_gif_data(const void * data)
{
    gd_GIF gif_base;
    memset(&gif_base, 0, sizeof(gif_base));

    bool res = f_gif_open(&gif_base, data, false);
    if(!res) return NULL;

    return gif_open(&gif_base);
}

static gd_GIF * gif_open(gd_GIF * gif_base)
{
    uint8_t sigver[3];
    uint16_t width, height, depth;
    uint8_t fdsz, bgidx, aspect;
    uint8_t * bgcolor;
    int gct_sz;
    gd_GIF * gif = NULL;

    /* Header */
    f_gif_read(gif_base, sigver, 3);
    if(memcmp(sigver, "GIF", 3) != 0) {
        ESP_LOGW(TAG, "invalid signature");
        goto fail;
    }
    /* Version */
    f_gif_read(gif_base, sigver, 3);
    if(memcmp(sigver, "89a", 3) != 0 && memcmp(sigver, "87a", 3) != 0) {
        ESP_LOGW(TAG, "invalid version");
        goto fail;
    }
    /* Width x Height */
    width  = read_num(gif_base);
    height = read_num(gif_base);
    /* FDSZ */
    f_gif_read(gif_base, &fdsz, 1);
    /* Presence of GCT */
    if(!(fdsz & 0x80)) {
        ESP_LOGW(TAG, "no global color table");
        goto fail;
    }
    /* Color Space's Depth */
    depth = ((fdsz >> 4) & 7) + 1;
    /* Ignore Sort Flag. */
    /* GCT Size */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    /* Background Color Index */
    f_gif_read(gif_base, &bgidx, 1);
    /* Aspect Ratio */
    f_gif_read(gif_base, &aspect, 1);
    /* Create gd_GIF Structure. */
    if(0 == width || 0 == height){
        ESP_LOGW(TAG, "Zero size image");
        goto fail;
    }
#if LV_GIF_CACHE_DECODE_DATA
    if(0 == (INT_MAX - sizeof(gd_GIF) - LZW_CACHE_SIZE) / width / height / 5){
        ESP_LOGW(TAG, "Image dimensions are too large");
        goto fail;
    } 
    gif = lv_malloc(sizeof(gd_GIF) + 5 * width * height + LZW_CACHE_SIZE);
#else
    if(0 == (INT_MAX - sizeof(gd_GIF)) / width / height / 5){
        ESP_LOGW(TAG, "Image dimensions are too large");
        goto fail;
    } 
    gif = lv_malloc(sizeof(gd_GIF) + 5 * width * height);
#endif
    if(!gif) goto fail;
    memcpy(gif, gif_base, sizeof(gd_GIF));
    gif->width  = width;
    gif->height = height;
    gif->depth  = depth;
    /* Read GCT */
    gif->gct.size = gct_sz;
    f_gif_read(gif, gif->gct.colors, 3 * gif->gct.size);
    gif->palette = &gif->gct;
    gif->bgindex = bgidx;
    gif->canvas = (uint8_t *) &gif[1];
    gif->frame = &gif->canvas[4 * width * height];
    if(gif->bgindex) {
        memset(gif->frame, gif->bgindex, gif->width * gif->height);
    }
    bgcolor = &gif->palette->colors[gif->bgindex * 3];
    #if LV_GIF_CACHE_DECODE_DATA
    gif->lzw_cache = gif->frame + width * height;
    #endif

#ifdef GIFDEC_FILL_BG
    GIFDEC_FILL_BG(gif->canvas, gif->width * gif->height, 1, gif->width * gif->height, bgcolor, 0x00);
#else
    for(int i = 0; i < gif->width * gif->height; i++) {
        gif->canvas[i * 4 + 0] = *(bgcolor + 2);
        gif->canvas[i * 4 + 1] = *(bgcolor + 1);
        gif->canvas[i * 4 + 2] = *(bgcolor + 0);
        gif->canvas[i * 4 + 3] = 0x00;  // 初始化为透明，让第一帧根据自己的透明度设置来渲染
    }
#endif
    gif->anim_start = f_gif_seek(gif, 0, LV_FS_SEEK_CUR);
    gif->loop_count = -1;
    goto ok;
fail:
    f_gif_close(gif_base);
ok:
    return gif;
}

static void
discard_sub_blocks(gd_GIF * gif)
{
    uint8_t size;

    do {
        f_gif_read(gif, &size, 1);
        f_gif_seek(gif, size, LV_FS_SEEK_CUR);
    } while(size);
}

static void
read_plain_text_ext(gd_GIF * gif)
{
    if(gif->plain_text) {
        uint16_t tx, ty, tw, th;
        uint8_t cw, ch, fg, bg;
        size_t sub_block;
        f_gif_seek(gif, 1, LV_FS_SEEK_CUR); /* block size = 12 */
        tx = read_num(gif);
        ty = read_num(gif);
        tw = read_num(gif);
        th = read_num(gif);
        f_gif_read(gif, &cw, 1);
        f_gif_read(gif, &ch, 1);
        f_gif_read(gif, &fg, 1);
        f_gif_read(gif, &bg, 1);
        sub_block = f_gif_seek(gif, 0, LV_FS_SEEK_CUR);
        gif->plain_text(gif, tx, ty, tw, th, cw, ch, fg, bg);
        f_gif_seek(gif, sub_block, LV_FS_SEEK_SET);
    }
    else {
        /* Discard plain text metadata. */
        f_gif_seek(gif, 13, LV_FS_SEEK_CUR);
    }
    /* Discard plain text sub-blocks. */
    discard_sub_blocks(gif);
}

static void
read_graphic_control_ext(gd_GIF * gif)
{
    uint8_t rdit;

    /* Discard block size (always 0x04). */
    f_gif_seek(gif, 1, LV_FS_SEEK_CUR);
    f_gif_read(gif, &rdit, 1);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif);
    f_gif_read(gif, &gif->gce.tindex, 1);
    /* Skip block terminator. */
    f_gif_seek(gif, 1, LV_FS_SEEK_CUR);
}

static void
read_comment_ext(gd_GIF * gif)
{
    if(gif->comment) {
        size_t sub_block = f_gif_seek(gif, 0, LV_FS_SEEK_CUR);
        gif->comment(gif);
        f_gif_seek(gif, sub_block, LV_FS_SEEK_SET);
    }
    /* Discard comment sub-blocks. */
    discard_sub_blocks(gif);
}

static void
read_application_ext(gd_GIF * gif)
{
    char app_id[8];
    char app_auth_code[3];
    uint16_t loop_count;

    /* Discard block size (always 0x0B). */
    f_gif_seek(gif, 1, LV_FS_SEEK_CUR);
    /* Application Identifier. */
    f_gif_read(gif, app_id, 8);
    /* Application Authentication Code. */
    f_gif_read(gif, app_auth_code, 3);
    if(!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
        /* Discard block size (0x03) and constant byte (0x01). */
        f_gif_seek(gif, 2, LV_FS_SEEK_CUR);
        loop_count = read_num(gif);
        if(gif->loop_count < 0) {
            if(loop_count == 0) {
                gif->loop_count = 0;
            }
            else {
                gif->loop_count = loop_count + 1;
            }
        }
        /* Skip block terminator. */
        f_gif_seek(gif, 1, LV_FS_SEEK_CUR);
    }
    else if(gif->application) {
        size_t sub_block = f_gif_seek(gif, 0, LV_FS_SEEK_CUR);
        gif->application(gif, app_id, app_auth_code);
        f_gif_seek(gif, sub_block, LV_FS_SEEK_SET);
        discard_sub_blocks(gif);
    }
    else {
        discard_sub_blocks(gif);
    }
}

static void
read_ext(gd_GIF * gif)
{
    uint8_t label;

    f_gif_read(gif, &label, 1);
    switch(label) {
        case 0x01:
            read_plain_text_ext(gif);
            break;
        case 0xF9:
            read_graphic_control_ext(gif);
            break;
        case 0xFE:
            read_comment_ext(gif);
            break;
        case 0xFF:
            read_application_ext(gif);
            break;
        default:
            ESP_LOGW(TAG, "unknown extension: %02X\n", label);
    }
}

static uint16_t
get_key(gd_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key;

    key = 0;
    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            /* Update byte. */
            if (*sub_len == 0) {
                f_gif_read(gif, sub_len, 1); /* Must be nonzero! */
                if (*sub_len == 0) return 0x1000;
            }
            f_gif_read(gif, byte, 1);
            (*sub_len)--;
        }
        frag_size = MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
    }
    /* Clear extra bits to the left. */
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}

#if LV_GIF_CACHE_DECODE_DATA
/* Decompress image pixels.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table) or parse error. */
static int
read_image_data(gd_GIF *gif, int interlace)
{
    uint8_t sub_len, shift, byte;
    int ret = 0;
    int key_size;
    int y, pass, linesize;
    uint8_t *ptr = NULL;
    uint8_t *ptr_row_start = NULL;
    uint8_t *ptr_base = NULL;
    size_t start, end;
    uint16_t key, clear_code, stop_code, curr_code;
    int frm_off, frm_size,curr_size,top_slot,new_codes,slot;
    /* The first value of the value sequence corresponding to key */
    int first_value;
    int last_key;
    uint8_t *sp = NULL;
    uint8_t *p_stack = NULL;
    uint8_t *p_suffix = NULL;
    uint16_t *p_prefix = NULL;

    /* get initial key size and clear code, stop code */
    f_gif_read(gif, &byte, 1);
    key_size = (int) byte;
    clear_code = 1 << key_size;
    stop_code = clear_code + 1;
    key = 0;

    start = f_gif_seek(gif, 0, LV_FS_SEEK_CUR);
    discard_sub_blocks(gif);
    end = f_gif_seek(gif, 0, LV_FS_SEEK_CUR);
    f_gif_seek(gif, start, LV_FS_SEEK_SET);

    linesize = gif->width;
    ptr_base = &gif->frame[gif->fy * linesize + gif->fx];
    ptr_row_start = ptr_base;
    ptr = ptr_row_start;
    sub_len = shift = 0;
    /* decoder */
    pass = 0;
    y = 0;
    p_stack = gif->lzw_cache;
    p_suffix = gif->lzw_cache + LZW_TABLE_SIZE;
    p_prefix = (uint16_t*)(gif->lzw_cache + LZW_TABLE_SIZE * 2);
    frm_off = 0;
    frm_size = gif->fw * gif->fh;
    curr_size = key_size + 1;
    top_slot = 1 << curr_size;
    new_codes = clear_code + 2;
    slot = new_codes;
    first_value = -1;
    last_key = -1;
    sp = p_stack;

    while (frm_off < frm_size) {
        /* copy data to frame buffer */
        while (sp > p_stack) {
            if(frm_off >= frm_size){
                ESP_LOGW(TAG, "LZW table token overflows the frame buffer");
                return -1;
            }
            *ptr++ = *(--sp);
            frm_off += 1;
            /* read one line */
            if ((ptr - ptr_row_start) == gif->fw) {
                if (interlace) {
                    switch(pass) {
                    case 0:
                    case 1:
                        y += 8;
                        ptr_row_start += linesize * 8;
                        break;
                    case 2:
                        y += 4;
                        ptr_row_start += linesize * 4;
                        break;
                    case 3:
                        y += 2;
                        ptr_row_start += linesize * 2;
                        break;
                    default:
                        break;
                    }
                    while (y >= gif->fh) {
                        y  = 4 >> pass;
                        ptr_row_start = ptr_base + linesize * y;
                        pass++;
                    }
                } else {
                    ptr_row_start += linesize;
                }
                ptr = ptr_row_start;
            }
        }

        key = get_key(gif, curr_size, &sub_len, &shift, &byte);

        if (key == stop_code || key >= LZW_TABLE_SIZE)
            break;

        if (key == clear_code) {
            curr_size = key_size + 1;
            slot = new_codes;
            top_slot = 1 << curr_size;
            first_value = last_key = -1;
            sp = p_stack;
            continue;
        }

        curr_code = key;
        /*
         * If the current code is a code that will be added to the decoding
         * dictionary, it is composed of the data list corresponding to the
         * previous key and its first data.
         * */
        if (curr_code == slot && first_value >= 0) {
            *sp++ = first_value;
            curr_code = last_key;
        }else if(curr_code >= slot)
            break;

        while (curr_code >= new_codes) {
            *sp++ = p_suffix[curr_code];
            curr_code = p_prefix[curr_code];
        }
        *sp++ = curr_code;

        /* Add code to decoding dictionary */
        if (slot < top_slot && last_key >= 0) {
            p_suffix[slot] = curr_code;
            p_prefix[slot++] = last_key;
        }
        first_value = curr_code;
        last_key = key;
        if (slot >= top_slot) {
            if (curr_size < LZW_MAXBITS) {
                top_slot <<= 1;
                curr_size += 1;
            }
        }
    }

    if (key == stop_code) f_gif_read(gif, &sub_len, 1); /* Must be zero! */
    f_gif_seek(gif, end, LV_FS_SEEK_SET);
    return ret;
}
#else
static Table *
new_table(int key_size)
{
    int key;
    int init_bulk = MAX(1 << (key_size + 1), 0x100);
    Table * table = lv_malloc(sizeof(*table) + sizeof(Entry) * init_bulk);
    if(table) {
        table->bulk = init_bulk;
        table->nentries = (1 << key_size) + 2;
        table->entries = (Entry *) &table[1];
        for(key = 0; key < (1 << key_size); key++)
            table->entries[key] = (Entry) {
            1, 0xFFF, key
        };
    }
    return table;
}

/* Add table entry. Return value:
 *  0 on success
 *  +1 if key size must be incremented after this addition
 *  -1 if could not realloc table */
static int
add_entry(Table ** tablep, uint16_t length, uint16_t prefix, uint8_t suffix)
{
    Table * table = *tablep;
    if(table->nentries == table->bulk) {
        table->bulk *= 2;
        table = lv_realloc(table, sizeof(*table) + sizeof(Entry) * table->bulk);
        if(!table) return -1;
        table->entries = (Entry *) &table[1];
        *tablep = table;
    }
    table->entries[table->nentries] = (Entry) {
        length, prefix, suffix
    };
    table->nentries++;
    if((table->nentries & (table->nentries - 1)) == 0)
        return 1;
    return 0;
}

/* Compute output index of y-th input line, in frame of height h. */
static int
interlaced_line_index(int h, int y)
{
    int p; /* number of lines in current pass */

    p = (h - 1) / 8 + 1;
    if(y < p)  /* pass 1 */
        return y * 8;
    y -= p;
    p = (h - 5) / 8 + 1;
    if(y < p)  /* pass 2 */
        return y * 8 + 4;
    y -= p;
    p = (h - 3) / 4 + 1;
    if(y < p)  /* pass 3 */
        return y * 4 + 2;
    y -= p;
    /* pass 4 */
    return y * 2 + 1;
}

/* Decompress image pixels.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table) or parse error. */
static int
read_image_data(gd_GIF * gif, int interlace)
{
    uint8_t sub_len, shift, byte;
    int init_key_size, key_size, table_is_full = 0;
    int frm_off, frm_size, str_len = 0, i, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    Table * table;
    Entry entry = {0};
    size_t start, end;

    f_gif_read(gif, &byte, 1);
    key_size = (int) byte;
    start = f_gif_seek(gif, 0, LV_FS_SEEK_CUR);
    discard_sub_blocks(gif);
    end = f_gif_seek(gif, 0, LV_FS_SEEK_CUR);
    f_gif_seek(gif, start, LV_FS_SEEK_SET);
    clear = 1 << key_size;
    stop = clear + 1;
    table = new_table(key_size);
    key_size++;
    init_key_size = key_size;
    sub_len = shift = 0;
    key = get_key(gif, key_size, &sub_len, &shift, &byte); /* clear code */
    frm_off = 0;
    ret = 0;
    frm_size = gif->fw * gif->fh;
    while(frm_off < frm_size) {
        if(key == clear) {
            key_size = init_key_size;
            table->nentries = (1 << (key_size - 1)) + 2;
            table_is_full = 0;
        }
        else if(!table_is_full) {
            ret = add_entry(&table, str_len + 1, key, entry.suffix);
            if(ret == -1) {
                lv_free(table);
                return -1;
            }
            if(table->nentries == 0x1000) {
                ret = 0;
                table_is_full = 1;
            }
        }
        key = get_key(gif, key_size, &sub_len, &shift, &byte);
        if(key == clear) continue;
        if(key == stop || key == 0x1000) break;
        if(ret == 1) key_size++;
        entry = table->entries[key];
        str_len = entry.length;
	if(frm_off + str_len > frm_size){
		ESP_LOGW(TAG, "LZW table token overflows the frame buffer");
		lv_free(table);
		return -1;
	}
        for(i = 0; i < str_len; i++) {
            p = frm_off + entry.length - 1;
            x = p % gif->fw;
            y = p / gif->fw;
            if(interlace)
                y = interlaced_line_index((int) gif->fh, y);
            gif->frame[(gif->fy + y) * gif->width + gif->fx + x] = entry.suffix;
            if(entry.prefix == 0xFFF)
                break;
            else
                entry = table->entries[entry.prefix];
        }
        frm_off += str_len;
        if(key < table->nentries - 1 && !table_is_full)
            table->entries[table->nentries - 1].suffix = entry.suffix;
    }
    lv_free(table);
    if(key == stop) f_gif_read(gif, &sub_len, 1);  /* Must be zero! */
    f_gif_seek(gif, end, LV_FS_SEEK_SET);
    return 0;
}

#endif

/* Read image.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table) or parse error. */
static int
read_image(gd_GIF * gif)
{
    uint8_t fisrz;
    int interlace;

    /* Image Descriptor. */
    gif->fx = read_num(gif);
    gif->fy = read_num(gif);
    gif->fw = read_num(gif);
    gif->fh = read_num(gif);
    if(gif->fx + (uint32_t)gif->fw > gif->width || gif->fy + (uint32_t)gif->fh > gif->height){
        ESP_LOGW(TAG, "Frame coordinates out of image bounds");
        return -1;
    }
    f_gif_read(gif, &fisrz, 1);
    interlace = fisrz & 0x40;
    /* Ignore Sort Flag. */
    /* Local Color Table? */
    if(fisrz & 0x80) {
        /* Read LCT */
        gif->lct.size = 1 << ((fisrz & 0x07) + 1);
        f_gif_read(gif, gif->lct.colors, 3 * gif->lct.size);
        gif->palette = &gif->lct;
    }
    else
        gif->palette = &gif->gct;
    /* Image Data. */
    return read_image_data(gif, interlace);
}

static void
render_frame_rect(gd_GIF * gif, uint8_t * buffer)
{
    int i = gif->fy * gif->width + gif->fx;
#ifdef GIFDEC_RENDER_FRAME
    GIFDEC_RENDER_FRAME(&buffer[i * 4], gif->fw, gif->fh, gif->width,
                        &gif->frame[i], gif->palette->colors,
                        gif->gce.transparency ? gif->gce.tindex : 0x100);
#else
    int j, k;
    uint8_t index, * color;

    for(j = 0; j < gif->fh; j++) {
        for(k = 0; k < gif->fw; k++) {
            index = gif->frame[(gif->fy + j) * gif->width + gif->fx + k];
            color = &gif->palette->colors[index * 3];
            if(!gif->gce.transparency || index != gif->gce.tindex) {
                buffer[(i + k) * 4 + 0] = *(color + 2);
                buffer[(i + k) * 4 + 1] = *(color + 1);
                buffer[(i + k) * 4 + 2] = *(color + 0);
                buffer[(i + k) * 4 + 3] = 0xFF;
            }
        }
        i += gif->width;
    }
#endif
}

static void
dispose(gd_GIF * gif)
{
    int i;
    uint8_t * bgcolor;
    switch(gif->gce.disposal) {
        case 2: /* Restore to background color. */
            bgcolor = &gif->palette->colors[gif->bgindex * 3];

            uint8_t opa = 0xff;
            if(gif->gce.transparency) opa = 0x00;

            i = gif->fy * gif->width + gif->fx;
#ifdef GIFDEC_FILL_BG
            GIFDEC_FILL_BG(&(gif->canvas[i * 4]), gif->fw, gif->fh, gif->width, bgcolor, opa);
#else
            int j, k;
            for(j = 0; j < gif->fh; j++) {
                for(k = 0; k < gif->fw; k++) {
                    gif->canvas[(i + k) * 4 + 0] = *(bgcolor + 2);
                    gif->canvas[(i + k) * 4 + 1] = *(bgcolor + 1);
                    gif->canvas[(i + k) * 4 + 2] = *(bgcolor + 0);
                    gif->canvas[(i + k) * 4 + 3] = opa;
                }
                i += gif->width;
            }
#endif
            break;
        case 3: /* Restore to previous, i.e., don't update canvas.*/
            break;
        default:
            /* Add frame non-transparent pixels to canvas. */
            render_frame_rect(gif, gif->canvas);
    }
}

/* Return 1 if got a frame; 0 if got GIF trailer; -1 if error. */
int
gd_get_frame(gd_GIF * gif)
{
    char sep;

    dispose(gif);
    f_gif_read(gif, &sep, 1);
    while(sep != ',') {
        if(sep == ';') {
            f_gif_seek(gif, gif->anim_start, LV_FS_SEEK_SET);
            if(gif->loop_count == 1 || gif->loop_count < 0) {
                return 0;
            }
            else if(gif->loop_count > 1) {
                gif->loop_count--;
            }
        }
        else if(sep == '!')
            read_ext(gif);
        else return -1;
        f_gif_read(gif, &sep, 1);
    }
    if(read_image(gif) == -1)
        return -1;
    return 1;
}

void
gd_render_frame(gd_GIF * gif, uint8_t * buffer)
{
    render_frame_rect(gif, buffer);
}

void
gd_rewind(gd_GIF * gif)
{
    gif->loop_count = -1;
    f_gif_seek(gif, gif->anim_start, LV_FS_SEEK_SET);
}

void
gd_close_gif(gd_GIF * gif)
{
    f_gif_close(gif);
    lv_free(gif);
}

static bool f_gif_open(gd_GIF * gif, const void * path, bool is_file)
{
    gif->f_rw_p = 0;
    gif->data = NULL;
    gif->is_file = is_file;

    if(is_file) {
        lv_fs_res_t res = lv_fs_open(&gif->fd, path, LV_FS_MODE_RD);
        if(res != LV_FS_RES_OK) return false;
        else return true;
    }
    else {
        gif->data = path;
        return true;
    }
}

static void f_gif_read(gd_GIF * gif, void * buf, size_t len)
{
    if(gif->is_file) {
        lv_fs_read(&gif->fd, buf, len, NULL);
    }
    else {
        memcpy(buf, &gif->data[gif->f_rw_p], len);
        gif->f_rw_p += len;
    }
}

static int f_gif_seek(gd_GIF * gif, size_t pos, int k)
{
    if(gif->is_file) {
        lv_fs_seek(&gif->fd, pos, k);
        uint32_t x;
        lv_fs_tell(&gif->fd, &x);
        return x;
    }
    else {
        if(k == LV_FS_SEEK_CUR) gif->f_rw_p += pos;
        else if(k == LV_FS_SEEK_SET) gif->f_rw_p = pos;
        return gif->f_rw_p;
    }
}

static void f_gif_close(gd_GIF * gif)
{
    if(gif->is_file) {
        lv_fs_close(&gif->fd);
    }
}

