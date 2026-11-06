#include "weather_ui.h"
#include "board.h" 
#include "wifi_station.h"
#include <esp_log.h>
#include <time.h>
#include <cstdio>

// --- 1. KHAI BÁO FONTS ---
LV_FONT_DECLARE(font_awesome_30_4);
LV_FONT_DECLARE(font_awesome_20_4);     
LV_FONT_DECLARE(lv_font_montserrat_20); 
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(font_puhui_20_4);       
LV_FONT_DECLARE(lv_font_ds_digitb_48);  

// --- 2. CẤU HÌNH MÀU SẮC ---
#define COLOR_BG            lv_color_hex(0x000000)
#define COLOR_NEON_GREEN    lv_color_hex(0x39FF14)
#define COLOR_ORANGE        lv_color_hex(0xFFA500)
#define COLOR_WHITE         lv_color_hex(0xFFFFFF)
#define COLOR_GRAY          lv_color_hex(0xAAAAAA)

// --- 3. BIẾN KÍCH THƯỚC ĐỘNG (RESPONSIVE) ---
static int s_card_w = 0;
static int s_card_h = 0;
#define FLIP_DURATION   150

static lv_obj_t *fc_cards[4];
static lv_obj_t *fc_labels[4];
static lv_obj_t *fc_labels2[4];
static lv_obj_t *fc_labels3[4];
static int fc_last_min = -1;
static int fc_last_digits[4] = {-1, -1, -1, -1};
static int fc_pending_digits[4];

static const char *weekday_str[] = {
    "Chủ nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư", "Thứ Năm", "Thứ Sáu", "Thứ Bảy"
};

static void flip_scale_y_cb(void *var, int32_t val) {
    lv_obj_set_style_transform_scale_y((lv_obj_t *)var, val, 0);
}

static void flip_phase1_done(lv_anim_t *a) {
    int idx = (int)(intptr_t)lv_anim_get_user_data(a);
    if (idx < 0 || idx > 3) return;

    char buf[2] = { (char)('0' + fc_pending_digits[idx]), '\0' };
    lv_label_set_text(fc_labels[idx], buf);
    lv_label_set_text(fc_labels2[idx], buf);
    lv_label_set_text(fc_labels3[idx], buf);
    fc_last_digits[idx] = fc_pending_digits[idx];

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, fc_cards[idx]);
    lv_anim_set_values(&a2, 0, 256);
    lv_anim_set_duration(&a2, FLIP_DURATION);
    lv_anim_set_exec_cb(&a2, flip_scale_y_cb);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_start(&a2);
}

static void flip_digit(int idx, int new_digit) {
    fc_pending_digits[idx] = new_digit;
    lv_obj_set_style_transform_pivot_x(fc_cards[idx], s_card_w / 2, 0);
    lv_obj_set_style_transform_pivot_y(fc_cards[idx], s_card_h / 2, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fc_cards[idx]);
    lv_anim_set_values(&a, 256, 0);
    lv_anim_set_duration(&a, FLIP_DURATION);
    lv_anim_set_exec_cb(&a, flip_scale_y_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_user_data(&a, (void *)(intptr_t)idx);
    lv_anim_set_completed_cb(&a, flip_phase1_done);
    lv_anim_start(&a);
}

static void create_card(lv_obj_t *parent, int idx, const lv_font_t *font, int zoom_val, int radius) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, s_card_w, s_card_h);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, radius, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *wrap = lv_obj_create(card);
    lv_obj_set_size(wrap, s_card_w, s_card_h);
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrap, 0, 0);
    lv_obj_set_style_pad_all(wrap, 0, 0);
    lv_obj_center(wrap);
    
    lv_obj_set_style_transform_pivot_x(wrap, s_card_w / 2, 0);
    lv_obj_set_style_transform_pivot_y(wrap, s_card_h / 2, 0);
    lv_obj_set_style_transform_zoom(wrap, zoom_val, 0);

    // 1. LỚP BÓNG TỐI Ở DƯỚI (Đã thêm số 0 ở cuối)
    lv_obj_t *lbl2 = lv_label_create(wrap);
    lv_obj_set_style_text_font(lbl2, font, 0);
    lv_obj_set_style_text_color(lbl2, lv_color_hex(0x444444), 0); 
    lv_label_set_text(lbl2, "0");
    lv_obj_align(lbl2, LV_ALIGN_CENTER, 2, 2);

    // 2. LỚP BÓNG SÁNG Ở DƯỚI (Đã thêm số 0 ở cuối)
    lv_obj_t *lbl3 = lv_label_create(wrap);
    lv_obj_set_style_text_font(lbl3, font, 0);
    lv_obj_set_style_text_color(lbl3, lv_color_hex(0x777777), 0); 
    lv_label_set_text(lbl3, "0");
    lv_obj_align(lbl3, LV_ALIGN_CENTER, -1, -1);

    // 3. LỚP CHỮ TRẮNG Ở TRÊN CÙNG
    lv_obj_t *lbl = lv_label_create(wrap);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, COLOR_WHITE, 0);
    lv_label_set_text(lbl, "0");
    lv_obj_center(lbl);

    // 4. ĐƯỜNG CẮT NGANG THẺ LẬT
    lv_obj_t *line = lv_obj_create(wrap);
    lv_obj_set_size(line, s_card_w, (s_card_h * 0.02 > 2) ? (s_card_h * 0.02) : 2); 
    lv_obj_set_style_bg_color(line, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, 0);

    fc_cards[idx] = card; 
    fc_labels[idx] = lbl;
    fc_labels2[idx] = lbl2;
    fc_labels3[idx] = lbl3;
}
// --------------------------------------------------------

WeatherUI::WeatherUI() 
    : container_(nullptr), 
      screen_width_(0), 
      screen_height_(0), 
      label_wifi_icon_(nullptr),
      label_wifi_text_(nullptr),
      label_bat_icon_(nullptr),
      label_bat_text_(nullptr),
      label_full_date_(nullptr),
      label_location_(nullptr),
      group_weather_(nullptr),
      label_main_temp_(nullptr),
      label_main_icon_(nullptr)
{
    fc_last_min = -1;
    for(int i=0; i<4; i++) fc_last_digits[i] = -1;
}

WeatherUI::~WeatherUI() {
    if (container_) {
        lv_obj_del(container_);
        container_ = nullptr;
    }
}

const char* WeatherUI::GetWeatherIcon(const std::string& code) {
    if (code == "01d" || code == "01n") return "\uf185"; 
    if (code == "02d" || code == "02n") return "\uf6c4"; 
    if (code == "03d" || code == "03n") return "\uf0c2"; 
    if (code == "04d" || code == "04n") return "\uf0c2"; 
    if (code == "09d" || code == "09n") return "\uf740"; 
    if (code == "10d" || code == "10n") return "\uf743"; 
    if (code == "11d" || code == "11n") return "\uf0e7"; 
    return "\uf0c2"; 
}

void WeatherUI::SetupIdleUI(lv_obj_t* parent, int screen_width, int screen_height) {
    screen_width_ = screen_width;
    screen_height_ = screen_height;

    // --- TÍNH TOÁN KÍCH THƯỚC (RESPONSIVE) ---
    s_card_w = (int)(screen_width_ * 0.20); 
    s_card_h = (int)(screen_height_ * 0.35); 
    
    int card_radius = (int)(screen_width_ * 0.03);
    int card_gap    = (int)(screen_width_ * 0.02);

    int zoom_digit  = (int)((s_card_h * 0.75f / 48.0f) * 256.0f);

    // 1. Container Chính
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, screen_width_, screen_height_);
    lv_obj_center(container_);
    lv_obj_set_style_bg_color(container_, COLOR_BG, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0); 
    lv_obj_set_style_pad_all(container_, 0, 0); 
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(container_, 8, 0);

    // --- 2. HEADER (PIN, WIFI) ---
    lv_obj_t* row_header = lv_obj_create(container_);
    lv_obj_set_width(row_header, LV_PCT(80)); 
    lv_obj_set_height(row_header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_header, 0, 0);
    lv_obj_set_style_pad_all(row_header, 0, 0);
    lv_obj_set_style_margin_top(row_header, 6, 0);
    lv_obj_set_style_margin_bottom(row_header, 10, 0);
    lv_obj_set_scrollbar_mode(row_header, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(row_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Cụm Pin
    lv_obj_t* box_bat = lv_obj_create(row_header);
    lv_obj_set_size(box_bat, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box_bat, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_bat, 0, 0);
    lv_obj_set_style_pad_all(box_bat, 0, 0);
    lv_obj_set_flex_flow(box_bat, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box_bat, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(box_bat, 5, 0);

    label_bat_icon_ = lv_label_create(box_bat);
    lv_obj_set_style_text_font(label_bat_icon_, &font_awesome_20_4, 0);
    lv_obj_set_style_text_color(label_bat_icon_, COLOR_NEON_GREEN, 0);
    lv_label_set_text(label_bat_icon_, "\xef\x89\x80"); 

    label_bat_text_ = lv_label_create(box_bat);
    lv_obj_set_style_text_font(label_bat_text_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_bat_text_, COLOR_WHITE, 0);
    lv_label_set_text(label_bat_text_, "--%");

    // Cụm Wifi
    lv_obj_t* box_wifi = lv_obj_create(row_header);
    lv_obj_set_size(box_wifi, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box_wifi, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_wifi, 0, 0);
    lv_obj_set_style_pad_all(box_wifi, 0, 0);
    lv_obj_set_flex_flow(box_wifi, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box_wifi, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(box_wifi, 5, 0);

    label_wifi_text_ = lv_label_create(box_wifi);
    lv_obj_set_style_text_font(label_wifi_text_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_wifi_text_, COLOR_WHITE, 0);
    lv_label_set_text(label_wifi_text_, "--");

    label_wifi_icon_ = lv_label_create(box_wifi);
    lv_obj_set_style_text_font(label_wifi_icon_, &font_awesome_20_4, 0);
    lv_obj_set_style_text_color(label_wifi_icon_, COLOR_WHITE, 0);
    lv_label_set_text(label_wifi_icon_, "\xef\x87\xab"); 

    // --- 3. KHỐI ĐỒNG HỒ LẬT ---
    lv_obj_t *clock_row = lv_obj_create(container_);
    lv_obj_set_size(clock_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(clock_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_row, 0, 0);
    lv_obj_set_style_pad_all(clock_row, 0, 0);
    lv_obj_set_flex_flow(clock_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(clock_row, card_gap, 0);
    lv_obj_set_scrollbar_mode(clock_row, LV_SCROLLBAR_MODE_OFF);

    create_card(clock_row, 0, &lv_font_ds_digitb_48, zoom_digit, card_radius);
    create_card(clock_row, 1, &lv_font_ds_digitb_48, zoom_digit, card_radius);

    lv_obj_t *colon_wrap = lv_obj_create(clock_row);
    lv_obj_set_size(colon_wrap, s_card_w / 3, s_card_h);
    lv_obj_set_style_bg_opa(colon_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(colon_wrap, 0, 0);
    lv_obj_set_style_pad_all(colon_wrap, 0, 0);
    lv_obj_set_style_transform_pivot_x(colon_wrap, (s_card_w / 3) / 2, 0);
    lv_obj_set_style_transform_pivot_y(colon_wrap, s_card_h / 2, 0);
    lv_obj_set_style_transform_zoom(colon_wrap, zoom_digit, 0);

    lv_obj_t *colon = lv_label_create(colon_wrap);
    lv_obj_set_style_text_font(colon, &lv_font_ds_digitb_48, 0);
    lv_obj_set_style_text_color(colon, lv_color_hex(0xCCCCCC), 0);
    lv_label_set_text(colon, ":");
    lv_obj_center(colon);

    create_card(clock_row, 2, &lv_font_ds_digitb_48, zoom_digit, card_radius);
    create_card(clock_row, 3, &lv_font_ds_digitb_48, zoom_digit, card_radius);

    // --- 4. NGÀY THÁNG ---
    label_full_date_ = lv_label_create(container_);
    lv_obj_set_style_text_font(label_full_date_, &font_puhui_20_4, 0);
    lv_obj_set_style_text_color(label_full_date_, COLOR_GRAY, 0);
    lv_label_set_text(label_full_date_, "--, --/--/----");

    // --- 5. VỊ TRÍ ---
    label_location_ = lv_label_create(container_);
    lv_obj_set_style_text_font(label_location_, &font_puhui_20_4, 0);
    lv_obj_set_style_text_color(label_location_, COLOR_WHITE, 0);
    lv_label_set_text(label_location_, "\xef\x81\x81 Đang cập nhật..."); 

    // --- 6. KHỐI THỜI TIẾT ---
    group_weather_ = lv_obj_create(container_); 
    lv_obj_set_size(group_weather_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(group_weather_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(group_weather_, 0, 0);
    lv_obj_set_style_pad_all(group_weather_, 0, 0);
    
    lv_obj_set_flex_flow(group_weather_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(group_weather_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(group_weather_, 20, 0); 

    label_main_icon_ = lv_label_create(group_weather_);
    lv_obj_set_style_text_font(label_main_icon_, &font_awesome_30_4, 0);
    lv_obj_set_style_text_color(label_main_icon_, COLOR_NEON_GREEN, 0);
    lv_label_set_text(label_main_icon_, "\uf185");

    label_main_temp_ = lv_label_create(group_weather_);
    lv_obj_set_style_text_font(label_main_temp_, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_main_temp_, COLOR_ORANGE, 0);
    lv_label_set_text(label_main_temp_, "--°C");
}

void WeatherUI::ShowIdleCard(const IdleCardInfo& info) {
    if (!container_) return;

    // --- CẬP NHẬT HEADER (WIFI, PIN) ---
    char bbuf[16];
    snprintf(bbuf, sizeof(bbuf), "%d%%", info.battery_level);
    if(label_bat_text_) lv_label_set_text(label_bat_text_, bbuf);
    if (label_wifi_icon_) lv_label_set_text(label_wifi_icon_, info.network_icon.c_str());

    char wbuf[16];
    snprintf(wbuf, sizeof(wbuf), "%d", info.rssi);
    if(label_wifi_text_) lv_label_set_text(label_wifi_text_, wbuf);
    if (label_bat_icon_) lv_label_set_text(label_bat_icon_, info.battery_icon.c_str());

    // --- CẬP NHẬT ĐỒNG HỒ LẬT ---
    time_t now;
    struct tm ti;
    time(&now);
    localtime_r(&now, &ti);

    int cur_min = ti.tm_hour * 60 + ti.tm_min;
    if (cur_min != fc_last_min) {
        fc_last_min = cur_min;
        int digits[4];
        digits[0] = ti.tm_hour / 10;
        digits[1] = ti.tm_hour % 10;
        digits[2] = ti.tm_min  / 10;
        digits[3] = ti.tm_min  % 10;

        for (int i = 0; i < 4; i++) {
            if (digits[i] != fc_last_digits[i]) {
                flip_digit(i, digits[i]);
            }
        }
    }

    // --- CẬP NHẬT NGÀY THÁNG ---
    if (label_full_date_) {
        char date_buf[64];
        snprintf(date_buf, sizeof(date_buf), "%s, %02d/%02d/%04d",
                 (ti.tm_wday >= 0 && ti.tm_wday <= 6) ? weekday_str[ti.tm_wday] : "",
                 ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
        lv_label_set_text(label_full_date_, date_buf);
    }

    // --- CẬP NHẬT VỊ TRÍ TỪ OPENWEATHERMAP ---
    if (label_location_ && !info.city.empty()) {
        char loc_buf[64];
        snprintf(loc_buf, sizeof(loc_buf), "\xef\x81\x81 %s", info.city.c_str());
        lv_label_set_text(label_location_, loc_buf);
    }

    // --- CẬP NHẬT THỜI TIẾT ---
    if (label_main_temp_) lv_label_set_text(label_main_temp_, info.temperature_text.c_str());
    if (info.icon && label_main_icon_) lv_label_set_text(label_main_icon_, info.icon);

    lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

void WeatherUI::HideIdleCard() {
    if (container_) lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
}