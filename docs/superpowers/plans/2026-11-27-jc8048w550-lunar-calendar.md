# JC8048W550 Lunar Calendar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On the JC8048W550 idle screen, replace the `HH:MM` clock with the solar date and the current-date line with the full Vietnamese lunar date plus a line for today's holiday/giỗ; keep weather and the GitHub list untouched.

**Architecture:** A pure C++ lunar-calendar module (Hồ Ngọc Đức algorithm, UTC+7) and a pure event table feed the existing `IdleDashboard` summary card through `RefreshClock()`. Text needs Vietnamese glyphs that the assets font lacks, so two Noto Sans LVGL fonts (20 px and 28 px, Latin + Vietnamese ranges) are compiled into the board directory (the board CMake globs `*.c`/`*.cc`, so no CMake change).

**Tech Stack:** ESP-IDF 6.1 (`N:\Espressif\frameworks\esp-idf-v6.1`, `IDF_TOOLS_PATH=N:\Espressif`), LVGL 9, C++20, `lv_font_conv` (npm) for font generation.

**Spec:** `docs/superpowers/specs/2026-11-27-jc8048w550-lunar-calendar-design.md`

## Global Constraints

- All changes stay in `main/boards/guition/jc8048w550/` (plus these docs). No change to core, protocol, Kconfig, CMake, pins, board identity.
- Weather (today + 7 days), city, GitHub list and detail overlay are unchanged.
- No new NVS keys, no new network calls, no personal giỗ list.
- Time zone for lunar computation is UTC+7; the date fed in is the device's local date (`localtime_r`). Clock not synced (`tm_year < 2025 - 1900`) must never show a date.
- Vietnamese text must be real UTF-8 with diacritics; no unaccented fallback.
- Format touched C/C++ files with the repo `.clang-format` (`clang-format -i <files>`); do not reformat the generated font `.c` files.
- Pure modules (`lunar_calendar`, `vn_events`) include no LVGL, no ESP headers.
- Build with `idf.py build` + `app-flash` (not `scripts/build.py`) to keep local `sdkconfig` and custom assets (see memory: `jc8048w550-display-findings`). Flash port is COM5.
- No host C++ compiler exists on this machine (no `g++` on PATH), so tests follow the board's existing pattern: an on-device self-test (`dashboard_model_selftest.cc`) read from the serial log.

## Review Focus

1. Clock not yet synced (year < 2025): dashboard shows `--` placeholders, never a 1970 date or a lunar date for it. (Task 4)
2. Year rollover and Tết eve: 31/12 solar and the last day of lunar month 12 (day 29 or 30) must convert correctly, and the last day gets "Giao thừa". (Tasks 1, 2)
3. Leap months: `Nhuận` is shown, and mùng 1/rằm/lunar festivals do not fire inside a leap month. (Tasks 1, 2, 4)
4. Longest line ("Ngày Quý Hợi · Tháng Giáp Tuất · Tiết Đại tuyết", and two events on one day) must not overflow or push the weather block off the card or the fifth GitHub row off screen. (Task 4)
5. Every character in the lunar/weekday/event strings exists in the compiled fonts (a missing glyph silently renders blank, as `ỵ` in Tỵ would in the assets font). (Task 3)

---

### Task 1: Lunar calendar module

**Files:**
- Create: `main/boards/guition/jc8048w550/lunar_calendar.h`
- Create: `main/boards/guition/jc8048w550/lunar_calendar.cc`
- Create: `main/boards/guition/jc8048w550/lunar_selftest.cc`

**Interfaces:**
- Produces (namespace `lunar`):
  - `struct LunarDate { int day; int month; int year; bool leap; };`
  - `int JulianDay(int day, int month, int year);` (Gregorian civil date → JDN)
  - `LunarDate SolarToLunar(int day, int month, int year);` (UTC+7)
  - `std::string YearCanChi(int lunar_year);` e.g. `"Bính Ngọ"`
  - `std::string MonthCanChi(const LunarDate& date);` (month 1 = Dần)
  - `std::string DayCanChi(int jdn);`
  - `const char* SolarTerm(int day, int month, int year);` one of the 24 names, current 15° term
  - `bool RunLunarSelfTest();` (in `lunar_selftest.cc`, declared in `lunar_calendar.h`; logs and returns true when all checks pass)

- [ ] **Step 1: Write the failing self-test** in `lunar_selftest.cc`, same shape as `dashboard_model_selftest.cc` (`Check`/`CHECK` macro, `ESP_LOGE` on failure, `ESP_LOGI` "lunar selftest OK"). Assertions (each `CHECK`):
  - `SolarToLunar(17,2,2026)` == `{1,1,2026,false}`; `SolarToLunar(29,1,2025)` == `{1,1,2025,false}`
  - `SolarToLunar(25,7,2025)` == `{1,6,2025,true}`; `SolarToLunar(8,8,2025)` == `{15,6,2025,true}`
  - `SolarToLunar(1,1,2000)` == `{25,11,1999,false}`
  - `SolarToLunar(25,9,2026)` == `{15,8,2026,false}`; `SolarToLunar(26,4,2026)` == `{10,3,2026,false}`
  - `SolarToLunar(16,2,2026).month == 12 && .day >= 29` (Tết eve); `SolarToLunar(31,12,2025)` has `year == 2025`
  - `YearCanChi(2025) == "Ất Tỵ"`, `YearCanChi(2026) == "Bính Ngọ"`, `YearCanChi(1999) == "Kỷ Mão"`
  - `DayCanChi(JulianDay(1,1,2000)) == "Mậu Ngọ"`
  - `MonthCanChi({1,1,2026,false}) == "Canh Dần"`
  - `std::string(SolarTerm(15,7,2026)) == "Tiểu thử"`; `std::string(SolarTerm(22,12,2026)) == "Đông chí"`
- [ ] **Step 2: Run to verify it fails.** `idf.py build` from the IDF 6.1 shell: Expected: compile error, `lunar_calendar.h` not found / functions undefined.
- [ ] **Step 3: Implement** `lunar_calendar.{h,cc}` with the signatures above. Use the Hồ Ngọc Đức algorithm: new-moon day via the `k`-th lunation and `NewMoon`, `SunLongitude` at local midnight (time zone 7), leap-month detection by "month 11 + 13 lunations has a month with no major term". Day Can = `(jdn + 9) % 10`, Chi = `(jdn + 1) % 12`; year Can = `(year + 6) % 10`, Chi = `(year + 8) % 12`; month Can = `(year * 12 + month + 3) % 10`, Chi = `(month + 1) % 12`. `SolarTerm` indexes `floor(sunLongitude / 15°)`, with 0 = Xuân phân, then Thanh minh, Cốc vũ, Lập hạ, Tiểu mãn, Mang chủng, Hạ chí, Tiểu thử, Đại thử, Lập thu, Xử thử, Bạch lộ, Thu phân, Hàn lộ, Sương giáng, Lập đông, Tiểu tuyết, Đại tuyết, Đông chí, Tiểu hàn, Đại hàn, Lập xuân, Vũ thủy, Kinh trập. Can: Giáp Ất Bính Đinh Mậu Kỷ Canh Tân Nhâm Quý. Chi: Tý Sửu Dần Mão Thìn Tỵ Ngọ Mùi Thân Dậu Tuất Hợi. Use `double` math only inside these functions (no allocation in the hot path beyond the returned `std::string`).
- [ ] **Step 4: Temporarily call `lunar::RunLunarSelfTest()` once at the top of `JC8048W550Board()` constructor body in `jc8048w550_board.cc`** (do not commit this line), `idf.py build`, then `idf.py -p COM5 app-flash`, read the serial log (e.g. `python -m serial.tools.miniterm COM5 115200` for ~15 s after reset). Expected: `lunar selftest OK`, no `FAILED:` lines. If a value fails, fix the algorithm, not the expectation, unless the expectation contradicts an authoritative Vietnamese calendar.
- [ ] **Step 5: Remove the temporary call, run `clang-format -i` on the three new files, commit** `git add main/boards/guition/jc8048w550/lunar_*` / `git commit -m "feat(boards): JC8048W550 Vietnamese lunar calendar module"`.

---

### Task 2: Holiday / giỗ events

**Files:**
- Create: `main/boards/guition/jc8048w550/vn_events.h`
- Create: `main/boards/guition/jc8048w550/vn_events.cc`
- Modify: `main/boards/guition/jc8048w550/lunar_selftest.cc` (add event checks to `RunLunarSelfTest`)

**Interfaces:**
- Consumes: `lunar::SolarToLunar`, `lunar::LunarDate` (Task 1).
- Produces: `std::vector<std::string> dashboard::EventsFor(int day, int month, int year);` — names for that solar date, most specific first; empty on an ordinary day. Never fires lunar events when `leap` is true.

- [ ] **Step 1: Add failing checks** to the self-test (helper `Has(events, "name")`):
  - `EventsFor(17,2,2026)` has `"Tết Nguyên Đán"`; `EventsFor(16,2,2026)` has `"Giao thừa"`
  - `EventsFor(26,4,2026)` has `"Giỗ Tổ Hùng Vương"`; `EventsFor(25,9,2026)` has `"Tết Trung Thu"` and does not have `"Rằm"`
  - `EventsFor(6,9,2025)` has `"Vu Lan"`
  - `EventsFor(8,8,2025)` is empty (15th of leap month 6: no "Rằm")
  - `EventsFor(30,4,2026)` has `"Giải phóng miền Nam"`; `EventsFor(2,9,2026)` has `"Quốc khánh"`; `EventsFor(1,1,2026)` has `"Tết Dương lịch"`
  - one plain `1st` and one plain `15th` lunar day in a non-leap month return exactly `"Mùng 1"` / `"Rằm"` (pick dates from `SolarToLunar` rather than hardcoding: loop days of 2026 and check the count of `"Mùng 1"` days is 12 or 13 and of `"Rằm"` days at least 8 once festivals replace some)
- [ ] **Step 2: Run to verify it fails** (`idf.py build`): Expected: undefined `EventsFor`.
- [ ] **Step 3: Implement `EventsFor`** in `vn_events.cc` with a constant table: lunar (day, month, name): 1/1 Tết Nguyên Đán, 15/1 Tết Nguyên Tiêu, 10/3 Giỗ Tổ Hùng Vương, 15/4 Phật Đản, 5/5 Tết Đoan Ngọ, 15/7 Vu Lan, 15/8 Tết Trung Thu, 23/12 Ông Công Ông Táo; solar: 1/1 Tết Dương lịch, 30/4 Giải phóng miền Nam, 1/5 Quốc tế Lao động, 2/9 Quốc khánh. "Giao thừa" = the next solar day converts to lunar 1/1. Generic "Mùng 1" and "Rằm" (lunar day 1 / 15) are added only when no named lunar event matched that day. Skip all lunar rules if `leap`.
- [ ] **Step 4: Build, flash, read serial log** exactly as Task 1 Step 4 (temporary call again, not committed). Expected: `lunar selftest OK`.
- [ ] **Step 5: Format, commit** `feat(boards): JC8048W550 Vietnamese holiday and giỗ events`.

---

### Task 3: Vietnamese fonts

**Files:**
- Create: `main/boards/guition/jc8048w550/font_vi_20.c`, `font_vi_28.c` (generated)
- Create: `main/boards/guition/jc8048w550/vi_fonts.h`
- Modify: `main/boards/guition/jc8048w550/README.md` (font regeneration command)

**Interfaces:**
- Produces: `extern "C"` declarations `extern const lv_font_t font_vi_20; extern const lv_font_t font_vi_28;` in `vi_fonts.h`.

- [ ] **Step 1: Define the coverage check first.** In the scratchpad create `check_glyphs.py` taking the generated `.c` files and the Vietnamese strings; it must read the `unicode_list`/cmap ranges (or simply re-derive from the `--range` list passed to the generator) and fail if any character in the names of Task 1/Task 2 (all can, chi, solar terms, event names, `Thứ Hai … Chủ Nhật`, `Tháng`, `Ngày`, `Tiết`, `Nhuận`, `Âm lịch`, `năm`) is outside the generated range. Run it now against the planned range list: Expected FAIL (no fonts yet).
- [ ] **Step 2: Generate the fonts.** Download Noto Sans Regular (OFL) from `https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/full/ttf/NotoSans-Regular.ttf` into the scratchpad (not the repo). Run for sizes 20 and 28: `npx --yes lv_font_conv --font NotoSans-Regular.ttf --size <N> --bpp 4 --format lvgl --lv-include lvgl.h --lv-font-name font_vi_<N> -r 0x20-0x7E,0xC0-0xFD,0x102-0x103,0x110-0x111,0x128-0x129,0x168-0x169,0x1A0-0x1A1,0x1AF-0x1B0,0x1EA0-0x1EF9,0xB7 -o font_vi_<N>.c` (pin the lv_font_conv version used in the README). Put the output in the board dir and `vi_fonts.h` beside it.
- [ ] **Step 2b:** Run `check_glyphs.py`. Expected: PASS.
- [ ] **Step 3: Build** (`idf.py build`). Expected: success; note the app size increase (about 60–120 KB for both fonts) and confirm the app partition still has headroom in the `idf.py size` summary.
- [ ] **Step 4: Document** in the board README: license (Noto Sans, OFL), the exact generator command and version, and that the files are generated.
- [ ] **Step 5: Commit** `feat(boards): JC8048W550 Vietnamese Noto Sans fonts`.

---

### Task 4: Dashboard UI

**Files:**
- Modify: `main/boards/guition/jc8048w550/idle_dashboard.h`, `idle_dashboard.cc`
- Modify: `main/boards/guition/jc8048w550/jc8048w550_board.cc:329-331` (the `DashboardFonts` initializer)

**Interfaces:**
- Consumes: `lunar::*` (Task 1), `dashboard::EventsFor` (Task 2), `font_vi_20`, `font_vi_28` (Task 3).
- Produces: `DashboardFonts` fields `small, normal, temp, solar, lunar, icon, text` (`clock` is replaced by `solar`; `lunar` is new). `IdleDashboard::RefreshClock()` keeps its name and 10 s timer and now sets: `solar_` ("Thứ Sáu, 27/11/2026", `fonts_.solar`), `lunar_line1_` ("Âm lịch: 18/10 năm Bính Ngọ", with "Nhuận" after the month for leap months), `lunar_line2_` ("Ngày Mậu Ngọ · Tháng Canh Dần · Tiết Tiểu thử"), `event_` (kWarning colour, events joined with " · ", hidden when none).

- [ ] **Step 1: Define the checks** (UI cannot be unit-tested here), to run on the board in Step 5: (a) all five GitHub rows visible with a loaded weather card; (b) event line hidden on an ordinary day, shown on 1st/15th lunar; (c) placeholders `--` when the clock is not synced; (d) the longest line fits in the left column; (e) weather block and city line unchanged.
- [ ] **Step 2: Change `DashboardFonts`** in `idle_dashboard.h` and the initializer in `jc8048w550_board.cc` to `{&lv_font_montserrat_16, &lv_font_montserrat_20, &lv_font_montserrat_28, &font_vi_28, &font_vi_20, theme->icon_font()->font(), nullptr}` (field order as declared), include `vi_fonts.h`. Update the `IdleDashboard` class comment (clock → calendar).
- [ ] **Step 3: Rebuild the left column** in `BuildSummaryCard` (`idle_dashboard.cc:151-163`): replace `clock_` and `date_` by `solar_`, `lunar_line1_`, `lunar_line2_`, `event_` (columns of `Label` with `fonts_.solar` / `fonts_.lunar`); keep `city_icon_`/`city_` row below them. Give the lines a bounded width (the card width minus the weather block, never pushing `right`) with `LV_LABEL_LONG_MODE_CLIP` on the two lunar lines and `LV_LABEL_LONG_MODE_WRAP` limited to the event line. Budget: the top row grows by about 40 px when the event line is hidden; if the fifth repo row falls off screen, merge `lunar_line2_` content into `lunar_line1_` at `fonts_.small`-sized spacing rather than shrinking the repo rows.
- [ ] **Step 4: Rewrite `RefreshClock()`** (`idle_dashboard.cc:398-412`): keep the `local.tm_year < 2025 - 1900` guard → set `solar_` to `"--/--/----"` and the other three labels empty/hidden. Otherwise compute with `lunar::SolarToLunar(local.tm_mday, local.tm_mon + 1, local.tm_year + 1900)`, format with `snprintf`, call `SetText` (existing helper that skips unchanged text) and `SetHidden(event_, events.empty())`. Weekday names: `Chủ Nhật, Thứ Hai … Thứ Bảy` indexed by `tm_wday`. Remove the now-unused `strftime` `%H:%M` code and the unused `kClock` references.
- [ ] **Step 5: Build, flash, check on the panel** (checks a–e from Step 1). Also verify one leap-month date and one event day by temporarily setting the date with `settimeofday` in a throwaway build (do not commit), or by waiting for a real 1st/15th. Expected: matches an independent Vietnamese calendar for that date.
- [ ] **Step 6: Format touched files, commit** `feat(boards): JC8048W550 solar and lunar date card with holidays`.

---

### Task 5: Docs and final verification

**Files:**
- Modify: `main/boards/guition/jc8048w550/README.md` (idle-dashboard section: calendar card, event list, font note, timezone note)
- Modify: `docs/superpowers/specs/2026-11-27-jc8048w550-lunar-calendar-design.md` only if a spec statement became untrue.

- [ ] **Step 1: Update the README** idle-screen section: describe the solar date, lunar lines, event line, the event table, that lunar data is computed offline for the device's local date, and that text uses the embedded Vietnamese fonts instead of the assets font.
- [ ] **Step 2: Full build from the IDF 6.1 shell** `idf.py build`; Expected: success, no new warnings from the touched files.
- [ ] **Step 3: Run the existing Python tests** `python3 -m unittest discover -s scripts/tests -v`; Expected: same result as before the change (nothing here touches them).
- [ ] **Step 4: Hardware smoke on COM5:** boot, wait for NTP, confirm date lines, weather still updates, GitHub list taps still open the detail card, chat view still takes over when talking. Report what was verified on hardware and what was not (a successful build is not hardware validation).
- [ ] **Step 5: Commit** `docs(boards): JC8048W550 lunar calendar card`.
