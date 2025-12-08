#include "esp32_sd_music.h"
#include "board.h"
#include "display.h"
#include "audio_codec.h"
#include "application.h"
#include "sd_card.h"

#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <unordered_set>

#include <esp_log.h>
#include <esp_pthread.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "Esp32SdMusic";

// ================================================================
//  UTILITY HÀM TỰ DO (PHỤC VỤ UTF-8, GỢI Ý)
// ================================================================

// UTF16 → UTF8
static std::string Utf16ToUtf8(const uint8_t* data, int len, bool big_endian)
{
    std::string out;
    out.reserve(len);

    for (int i = 0; i + 1 < len; i += 2) {
        uint16_t ch = big_endian
                      ? (data[i] << 8) | data[i + 1]
                      : (data[i + 1] << 8) | data[i];

        if (ch == 0) break;

        if (ch < 0x80) {
            out.push_back((char)ch);
        } else if (ch < 0x800) {
            out.push_back((char)(0xC0 | (ch >> 6)));
            out.push_back((char)(0x80 | (ch & 0x3F)));
        } else {
            out.push_back((char)(0xE0 | (ch >> 12)));
            out.push_back((char)(0x80 | ((ch >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (ch & 0x3F)));
        }
    }
    return out;
}

// ISO-8859-1 → UTF-8
static std::string Latin1ToUtf8(const uint8_t* data, int len)
{
    std::string out;
    out.reserve(len * 2);

    for (int i = 0; i < len; i++) {
        uint8_t c = data[i];
        if (c < 0x80) {
            out.push_back((char)c);
        } else {
            out.push_back((char)(0xC0 | (c >> 6)));
            out.push_back((char)(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

// Chuẩn hóa về lowercase nhưng chỉ động tới ASCII (giữ nguyên UTF-8 đa byte)
static std::string ToLowerAscii(const std::string& s)
{
    std::string out = s;
    for (char& c : out) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 128) {
            c = static_cast<char>(std::tolower(uc));
        }
    }
    return out;
}

static std::string ExtractDirectory(const std::string& full_path)
{
    size_t pos = full_path.find_last_of('/');
    if (pos == std::string::npos) return std::string();
    return full_path.substr(0, pos);
}

static std::string ExtractBaseNameNoExt(const std::string& name_or_path)
{
    size_t slash = name_or_path.find_last_of('/');
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    size_t dot = name_or_path.find_last_of('.');
    size_t end = (dot == std::string::npos || dot < start) ? name_or_path.size() : dot;
    return name_or_path.substr(start, end - start);
}

static std::string MsToTimeString(int64_t ms)
{
    if (ms <= 0) {
        return "00:00";
    }

    int64_t total_sec = ms / 1000;
    int sec  = static_cast<int>(total_sec % 60);
    int min  = static_cast<int>((total_sec / 60) % 60);
    int hour = static_cast<int>(total_sec / 3600);

    char buf[32];   // <-- CHỈ CẦN DÒNG NÀY!

    if (hour > 0) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, min, sec);
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", min, sec);
    }

    return std::string(buf);
}

// Score cho chế độ gợi ý (tên tương tự + cùng thư mục + tần suất phát)
static int ComputeTrackScoreForBase(const Esp32SdMusic::TrackInfo& base,
                                    const Esp32SdMusic::TrackInfo& cand,
                                    uint32_t cand_play_count)
{
    int score = 0;

    std::string base_dir = ExtractDirectory(base.path);
    std::string cand_dir = ExtractDirectory(cand.path);
    if (!base_dir.empty() && base_dir == cand_dir) {
        score += 3;  // cùng thư mục / thể loại
    }

    std::string base_name = ExtractBaseNameNoExt(base.name.empty() ? base.path : base.name);
    std::string cand_name = ExtractBaseNameNoExt(cand.name.empty() ? cand.path : cand.name);

    base_name = ToLowerAscii(base_name);
    cand_name = ToLowerAscii(cand_name);

    if (!base_name.empty() && !cand_name.empty()) {
        if (cand_name.find(base_name) != std::string::npos ||
            base_name.find(cand_name) != std::string::npos)
        {
            score += 3;  // giống tên / chứa nhau
        } else {
            auto b_space = base_name.find(' ');
            auto c_space = cand_name.find(' ');
            std::string b_first = base_name.substr(0, b_space);
            std::string c_first = cand_name.substr(0, c_space);
            if (!b_first.empty() && b_first == c_first) {
                score += 1; // chung prefix
            }
        }
    }

    if (cand_play_count > 0) {
        score += static_cast<int>(cand_play_count); // ưu tiên bài nghe nhiều
    }

    return score;
}


// ================================================================
//  Đọc ID3v1 (cuối file)
// ================================================================
static void ReadId3v1(const std::string& path,
                      Esp32SdMusic::TrackInfo& info)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return;

    if (fseek(f, -128, SEEK_END) != 0) {
        fclose(f);
        return;
    }

    uint8_t tag[128];
    if (fread(tag, 1, 128, f) != 128) {
        fclose(f);
        return;
    }
    fclose(f);

    if (memcmp(tag, "TAG", 3) != 0) {
        return;
    }

    auto trim = [](const char* p, size_t n) -> std::string {
        std::string s(p, n);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
        return s;
    };

    std::string title   = trim((char*)tag + 3, 30);
    std::string artist  = trim((char*)tag + 33, 30);
    std::string album   = trim((char*)tag + 63, 30);
    std::string year    = trim((char*)tag + 93, 4);
    std::string comment = trim((char*)tag + 97, 28);
    uint8_t     track   = tag[126]; // ID3v1.1

    if (!title.empty()   && info.title.empty())   info.title   = title;
    if (!artist.empty()  && info.artist.empty())  info.artist  = artist;
    if (!album.empty()   && info.album.empty())   info.album   = album;
    if (!year.empty()    && info.year.empty())    info.year    = year;
    if (!comment.empty() && info.comment.empty()) info.comment = comment;

    if (info.track_number == 0 && track != 0) {
        info.track_number = track;
    }

    uint8_t genre_idx = tag[127];
    if (info.genre.empty() && genre_idx != 0xFF) {
        info.genre = std::to_string((int)genre_idx); // nếu muốn map tên genre thì làm thêm
    }
}

// ================================================================
//  Đọc ID3v2 vào TrackInfo (v2.2 / v2.3 / v2.4)
// ================================================================
static void ReadId3v2Full(FILE* f, int version, Esp32SdMusic::TrackInfo& info)
{
    uint8_t hdr[10];
    if (fseek(f, 0, SEEK_SET) != 0) return;
    if (fread(hdr, 1, 10, f) != 10) return;
    if (memcmp(hdr, "ID3", 3) != 0) return;

    int flags = hdr[5];
	(void)flags;   // tránh warning unused, không đổi logic
    uint32_t tag_size =
        ((hdr[6] & 0x7F) << 21) |
        ((hdr[7] & 0x7F) << 14) |
        ((hdr[8] & 0x7F) << 7)  |
         (hdr[9] & 0x7F);

    bool v22 = (version == 2);

    uint32_t pos = 0;
    while (pos + (v22 ? 6 : 10) <= tag_size) {
        uint8_t fh[10];
        int header_len = v22 ? 6 : 10;
        if (fread(fh, 1, header_len, f) != (size_t)header_len) break;
        pos += header_len;

        // ID rỗng → hết frame
        if ((v22 && fh[0] == 0 && fh[1] == 0 && fh[2] == 0) ||
            (!v22 && fh[0] == 0 && fh[1] == 0 && fh[2] == 0 && fh[3] == 0)) {
            break;
        }

        char fid[5] = {0};
        if (v22) {
            fid[0] = fh[0]; fid[1] = fh[1]; fid[2] = fh[2]; fid[3] = 0;
        } else {
            fid[0] = fh[0]; fid[1] = fh[1]; fid[2] = fh[2]; fid[3] = fh[3];
        }

        uint32_t fsize = 0;
        if (v22) {
            fsize = (fh[3] << 16) | (fh[4] << 8) | fh[5];
        } else {
            if (version == 4) {
                fsize =
                    ((fh[4] & 0x7F) << 21) |
                    ((fh[5] & 0x7F) << 14) |
                    ((fh[6] & 0x7F) << 7)  |
                     (fh[7] & 0x7F);
            } else {
                fsize =
                    (fh[4] << 24) | (fh[5] << 16) |
                    (fh[6] << 8)  |  fh[7];
            }
        }

        if (fsize == 0 || pos + fsize > tag_size) {
            fseek(f, tag_size + 10, SEEK_SET);
            break;
        }

        std::vector<uint8_t> frame(fsize);
		if (fread(frame.data(), 1, fsize, f) != fsize) break;
		pos += fsize;

        auto parseTextFrame = [&](const uint8_t* data, uint32_t len) -> std::string {
            if (len == 0) return {};
            int encoding = data[0];
            const uint8_t* text = data + 1;
            int text_len = len - 1;

            if (encoding == 0) {              // ISO-8859-1
                return Latin1ToUtf8(text, text_len);
            } else if (encoding == 1 || encoding == 2) { // UTF-16 / UTF-16BE
                if (text_len < 2) return {};
                bool big = !(text[0] == 0xFF && text[1] == 0xFE);
                return Utf16ToUtf8(text + 2, text_len - 2, big);
            } else if (encoding == 3) {        // UTF-8
                return std::string((char*)text, text_len);
            }
            return {};
        };

        std::string id = std::string(fid);

        // ---- Text frames ----
        if (id == "TIT2" || id == "TT2") {
            std::string v = parseTextFrame(frame.data(), fsize);
            if (!v.empty() && info.title.empty()) info.title = v;
        } else if (id == "TPE1" || id == "TP1") {
            std::string v = parseTextFrame(frame.data(), fsize);
            if (!v.empty() && info.artist.empty()) info.artist = v;
        } else if (id == "TALB" || id == "TAL") {
            std::string v = parseTextFrame(frame.data(), fsize);
            if (!v.empty() && info.album.empty()) info.album = v;
        } else if (id == "TCON" || id == "TCO") {
            std::string v = parseTextFrame(frame.data(), fsize);
            if (!v.empty() && info.genre.empty()) info.genre = v;
        } else if (id == "TYER" || id == "TDRC" || id == "TYE") {
            std::string v = parseTextFrame(frame.data(), fsize);
            if (!v.empty() && info.year.empty()) info.year = v;
        } else if (id == "TRCK" || id == "TRK") {
            std::string v = parseTextFrame(frame.data(), fsize);
            if (!v.empty() && info.track_number == 0) {
                info.track_number = atoi(v.c_str());
            }
        } else if (id == "COMM" || id == "COM") {
            if (fsize > 4) {
                int encoding = frame[0];
                const uint8_t* p = frame.data() + 1;
                if (fsize <= 4) continue;
                // bỏ language 3 byte
                p += 3;
                uint32_t remain = fsize - 4;

                // bỏ short description (null-terminated)
                uint32_t i = 0;
                while (i < remain && p[i] != 0) i++;
                if (i < remain) i++; // skip null
                const uint8_t* text = p + i;
                uint32_t text_len = remain - i;

                std::string v;
                if (encoding == 0) {
                    v = Latin1ToUtf8(text, text_len);
                } else if (encoding == 1 || encoding == 2) {
                    if (text_len < 2) continue;
                    bool big = !(text[0] == 0xFF && text[1] == 0xFE);
                    v = Utf16ToUtf8(text + 2, text_len - 2, big);
                } else if (encoding == 3) {
                    v.assign((char*)text, text_len);
                }

                if (!v.empty() && info.comment.empty()) info.comment = v;
            }
        }
        // ---- Cover art APIC/PIC ----
        else if (id == "APIC" || id == "PIC") {
            long frame_start = ftell(f) - fsize;

            int encoding = frame[0];
			(void)encoding; 
            const uint8_t* p = frame.data() + 1;
            uint32_t len = fsize - 1;

            // mime (null-terminated)
            uint32_t i = 0;
            while (i < len && p[i] != 0) i++;
            std::string mime((char*)p, i);
            if (i < len) i++;
            if (i >= len) continue;

            // picture type
            uint8_t pic_type = p[i];
            (void)pic_type;
            i++;
            if (i >= len) continue;

            // description (null-terminated, encoding-dependent) → bỏ qua
            uint32_t desc_start = i;
			(void)desc_start;
            while (i < len && p[i] != 0) i++;
            if (i < len) i++; // skip null
            if (i >= len) continue;

            uint32_t img_offset_in_frame = i;
            uint32_t img_size = len - img_offset_in_frame;

            if (img_size > 0) {
                info.cover_offset = (uint32_t)(frame_start + img_offset_in_frame);
                info.cover_size   = img_size;
                info.cover_mime   = mime;
            }
        }
        // các frame khác → bỏ qua
    }
}

// ================================================================
//  Đọc đầy đủ ID3 (v2 nếu có, sau đó fallback v1) vào TrackInfo
// ================================================================
static void ReadId3Full(const std::string& path,
                        Esp32SdMusic::TrackInfo& info)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (f) {
        uint8_t hdr[10];
        if (fread(hdr, 1, 10, f) == 10 && memcmp(hdr, "ID3", 3) == 0) {
            int version = hdr[3]; // 2, 3, 4
            ReadId3v2Full(f, version, info);
        }
        fclose(f);
    }

    // Nếu thiếu thông tin, fallback thêm ID3v1
    ReadId3v1(path, info);
}

// ============================================================================
//                         PART 1 / 3
//      CTOR / DTOR / PLAYLIST / THƯ MỤC / ĐẾM BÀI / CHIA TRANG
// ============================================================================

Esp32SdMusic::Esp32SdMusic()
    : root_directory_(),
      playlist_(),
      playlist_mutex_(),
      current_index_(-1),
      play_count_(),
      playback_thread_(),
      stop_requested_(false),
      pause_requested_(false),
      state_(PlayerState::Stopped),
      state_mutex_(),
      state_cv_(),
      shuffle_enabled_(false),
      repeat_mode_(RepeatMode::None),
      current_play_time_ms_(0),
      total_duration_ms_(0),
      final_pcm_data_fft_(nullptr),
      mp3_decoder_(nullptr),
      mp3_decoder_initialized_(false),
      history_mutex_(),
      play_history_indices_()
{
}

Esp32SdMusic::~Esp32SdMusic()
{
    ESP_LOGI(TAG, "Destroying SD music module");

    stop();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        stop_requested_ = true;
        pause_requested_ = false;
        state_cv_.notify_all();
    }

    joinPlaybackThreadWithTimeout();

    cleanupMp3Decoder();

    auto display = Board::GetInstance().GetDisplay();
    if (display && final_pcm_data_fft_) {
        display->ReleaseAudioBuffFFT(final_pcm_data_fft_);
        final_pcm_data_fft_ = nullptr;
    }

    ESP_LOGI(TAG, "SD music module destroyed");
}

void Esp32SdMusic::Initialize(class SdCard* sd_card) {
    sd_card_ = sd_card;
    if (sd_card_ && sd_card_->IsMounted()) {
        root_directory_ = sd_card_->GetMountPoint();
    } else {
        ESP_LOGW(TAG, "SD card not mounted yet — will retry later");
    }
    // InitializeMp3Decoder();
}

// Helper gom code join/detach thread
void Esp32SdMusic::joinPlaybackThreadWithTimeout()
{
    if (!playback_thread_.joinable()) return;

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(120);

    while (playback_thread_.joinable() &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (playback_thread_.joinable()) {
        ESP_LOGE(TAG, "Thread stuck → force detach()");
        playback_thread_.detach();
    }
}

// Playlist loading
bool Esp32SdMusic::loadTrackList()
{
    std::vector<TrackInfo> list;

    if (root_directory_.empty()) {
        return false;
    }

    ESP_LOGI(TAG, "Scanning SD card: %s", root_directory_.c_str());

    scanDirectoryRecursive(root_directory_, list, id3_cache_);

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        playlist_.swap(list);
        current_index_ = playlist_.empty() ? -1 : 0;
        play_count_.assign(playlist_.size(), 0);
    }

    {
        std::lock_guard<std::mutex> hlock(history_mutex_);
        play_history_indices_.clear();
    }

    ESP_LOGI(TAG, "Found %u tracks", (unsigned)playlist_.size());
    return !playlist_.empty();
}

size_t Esp32SdMusic::getTotalTracks() const
{
    std::lock_guard<std::mutex> lock(playlist_mutex_);
    return playlist_.size();
}

std::vector<Esp32SdMusic::TrackInfo> Esp32SdMusic::listTracks() const
{
    std::lock_guard<std::mutex> lock(playlist_mutex_);
    return playlist_;
}

Esp32SdMusic::TrackInfo Esp32SdMusic::getTrackInfo(int index) const
{
    std::lock_guard<std::mutex> lock(playlist_mutex_);
    if (index < 0 || index >= (int)playlist_.size()) return {};
    return playlist_[index];
}

// Gom code build path + resolve FAT short / case-insensitive
bool Esp32SdMusic::resolveDirectoryRelative(const std::string& relative_dir,
                                            std::string& out_full)
{
    if (sd_card_ == nullptr || !sd_card_->IsMounted()) {
        ESP_LOGE(TAG, "resolveDirectoryRelative: SD not mounted");
        return false;
    }

    std::string mount = sd_card_->GetMountPoint();
    std::string full;

    if (relative_dir.empty() || relative_dir == "/") {
        full = mount;
    } else if (relative_dir[0] == '/') {
        full = mount + relative_dir;
    } else {
        full = mount + "/" + relative_dir;
    }

    full = resolveLongName(full);
    full = resolveCaseInsensitiveDir(full);

    struct stat st{};
    if (stat(full.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "Invalid directory: %s", full.c_str());
        return false;
    }

    out_full = full;
    return true;
}

// SET DIRECTORY
bool Esp32SdMusic::setDirectory(const std::string& relative_dir)
{
    std::string full;
    if (!resolveDirectoryRelative(relative_dir, full)) {
        ESP_LOGE(TAG, "setDirectory: cannot resolve %s", relative_dir.c_str());
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        root_directory_ = full;
    }

    ESP_LOGI(TAG, "Directory selected: %s", full.c_str());
    return loadTrackList();
}

// PLAY DIRECTORY
bool Esp32SdMusic::playDirectory(const std::string& relative_dir)
{
    ESP_LOGI(TAG, "Request to play directory: %s", relative_dir.c_str());
    if (!setDirectory(relative_dir)) {
        ESP_LOGE(TAG, "playDirectory: cannot set directory: %s", relative_dir.c_str());
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (playlist_.empty()) {
            ESP_LOGE(TAG, "playDirectory: directory is empty: %s", relative_dir.c_str());
            return false;
        }
        current_index_ = 0;
        ESP_LOGI(TAG, "playDirectory: start track #0: %s",
                 playlist_[0].name.c_str());
    }
    return play();
}

// Tìm index theo keyword (tên hoặc path)
int Esp32SdMusic::findTrackIndexByKeyword(const std::string& keyword) const
{
    if (keyword.empty()) return -1;

    std::string kw = ToLowerAscii(keyword);

    std::lock_guard<std::mutex> lock(playlist_mutex_);
    for (int i = 0; i < (int)playlist_.size(); ++i) {
        std::string name = ToLowerAscii(playlist_[i].name);
        std::string path = ToLowerAscii(playlist_[i].path);

        if (name.find(kw) != std::string::npos ||
            path.find(kw) != std::string::npos) {
            return i;
        }
    }
    return -1;
}

// PLAY BY NAME
bool Esp32SdMusic::playByName(const std::string& keyword)
{
    if (keyword.empty()) {
        ESP_LOGW(TAG, "playByName(): empty keyword");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (playlist_.empty()) {
            ESP_LOGW(TAG, "playByName(): playlist empty — reloading");
        }
    }
    if (playlist_.empty()) {
        if (!loadTrackList()) {
            ESP_LOGE(TAG, "playByName(): Cannot load playlist");
            return false;
        }
    }

    int found_index = findTrackIndexByKeyword(keyword);
    if (found_index < 0) {
        ESP_LOGW(TAG, "playByName(): no match for '%s'", keyword.c_str());
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (found_index < 0 || found_index >= (int)playlist_.size()) {
            return false;
        }
        current_index_ = found_index;
        ESP_LOGI(TAG, "playByName(): matched track #%d → %s",
                 found_index, playlist_[found_index].name.c_str());
    }

    return play();
}

std::string Esp32SdMusic::getCurrentTrack() const
{
    std::lock_guard<std::mutex> lock(playlist_mutex_);
    if (current_index_ < 0 || current_index_ >= (int)playlist_.size()) return "";
    return playlist_[current_index_].name;
}

std::string Esp32SdMusic::getCurrentTrackPath() const
{
    std::lock_guard<std::mutex> lock(playlist_mutex_);
    if (current_index_ < 0 || current_index_ >= (int)playlist_.size()) return "";
    return playlist_[current_index_].path;
}

std::vector<std::string> Esp32SdMusic::listDirectories() const
{
    std::vector<std::string> dirs;

    // Duyệt thư mục bằng VFS (/sdcard/...) thay vì FatFs f_opendir
    DIR* d = opendir(root_directory_.c_str());
    if (!d) {
        ESP_LOGE(TAG, "Cannot open directory: %s", root_directory_.c_str());
        return dirs;
    }

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;

        if (name == "." || name == "..")
            continue;

        std::string full = root_directory_ + "/" + name;

        struct stat st{};
        if (stat(full.c_str(), &st) != 0)
            continue;

        // Nếu là thư mục → add vào list
        if (S_ISDIR(st.st_mode)) {
            dirs.push_back(name);   // name là UTF-8 (kể cả tiếng Việt)
        }
    }

    closedir(d);
    return dirs;
}

std::vector<Esp32SdMusic::TrackInfo>
Esp32SdMusic::searchTracks(const std::string& keyword) const
{
    std::vector<TrackInfo> results;
    if (keyword.empty()) return results;

    std::string kw = ToLowerAscii(keyword);

    std::lock_guard<std::mutex> lock(playlist_mutex_);
    for (const auto& t : playlist_) {
        std::string name = ToLowerAscii(t.name);
        std::string path = ToLowerAscii(t.path);
        if (name.find(kw) != std::string::npos ||
            path.find(kw) != std::string::npos) {
            results.push_back(t);
        }
    }
    return results;
}

std::vector<std::string> Esp32SdMusic::listGenres() const
{
    std::vector<std::string> genres;
    std::unordered_set<std::string> uniq;

    std::lock_guard<std::mutex> lock(playlist_mutex_);
    for (const auto &t : playlist_) {
        if (t.genre.empty()) continue;
        if (uniq.insert(t.genre).second) {
            genres.push_back(t.genre);
        }
    }

	std::sort(genres.begin(), genres.end(),
		  [](const std::string& a, const std::string& b) {
			  return ToLowerAscii(a) < ToLowerAscii(b);
		  });

    return genres;
}

std::string Esp32SdMusic::resolveCaseInsensitiveDir(const std::string& path)
{
    // Tách parent + tên thư mục con
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return path;

    std::string parent = path.substr(0, pos);
    std::string name   = path.substr(pos + 1);

    // Duyệt parent bằng VFS
    DIR* d = opendir(parent.c_str());
    if (!d) {
        return path;
    }

    // Lowercase ASCII (giữ nguyên UTF-8 đa byte) – đã có sẵn ToLowerAscii
    std::string lowerName = ToLowerAscii(name);

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string entry = ent->d_name;

        if (entry == "." || entry == "..")
            continue;

        std::string lowerEntry = ToLowerAscii(entry);

        // So sánh không phân biệt hoa/thường (ASCII)
        if (lowerEntry == lowerName) {
            std::string full = parent + "/" + entry;

            struct stat st{};
            if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                closedir(d);
                return full;
            }
        }
    }

    closedir(d);
    return path;
}

bool Esp32SdMusic::setTrack(int index)
{
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (index < 0 || index >= (int)playlist_.size()) {
            ESP_LOGE(TAG, "setTrack: index %d out of range", index);
            return false;
        }
        current_index_ = index;
        ESP_LOGI(TAG, "Switching to track #%d: %s",
                 index, playlist_[index].name.c_str());
    }
    return play();
}

void Esp32SdMusic::scanDirectoryRecursive(
    const std::string& dir,
    std::vector<TrackInfo>& out,
    std::unordered_map<std::string, TrackInfo>& cache)
{
    // Duyệt thư mục bằng VFS (opendir) với đường dẫn /sdcard/...
    DIR* d = opendir(dir.c_str());
    if (!d) {
        ESP_LOGE(TAG, "Cannot open directory: %s", dir.c_str());
        return;
    }

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name_utf8 = ent->d_name;

        if (name_utf8 == "." || name_utf8 == "..")
            continue;

        std::string full = dir + "/" + name_utf8;

        // Lấy thông tin file/dir
        struct stat st{};
        if (stat(full.c_str(), &st) != 0) {
            continue;
        }

        // Nếu là thư mục → đệ quy
        if (S_ISDIR(st.st_mode)) {
            scanDirectoryRecursive(full, out, cache);
            continue;
        }

        // Chỉ xử lý file .mp3
        std::string low = ToLowerAscii(name_utf8);
        if (low.size() <= 4 || low.substr(low.size() - 4) != ".mp3")
            continue;

        TrackInfo t;

        t.path      = full;          // path dạng /sdcard/...
        t.file_size = st.st_size;
        t.mtime     = st.st_mtime;   // dùng mtime của stat để cache

        // Kiểm tra cache ID3
        auto it = cache.find(full);
        bool need_rescan = true;

        if (it != cache.end()) {
            const TrackInfo& old = it->second;

            if (old.file_size == t.file_size &&
                old.mtime     == t.mtime)
            {
                t = old;
                need_rescan = false;
            }
        }

        if (need_rescan) {
            ReadId3Full(full, t);
        }

        // Tên hiển thị ưu tiên Title, fallback tên file
        if (!t.title.empty())
            t.name = t.title;
        else
            t.name = name_utf8;

        cache[t.path] = t;
        out.push_back(std::move(t));
    }

    closedir(d);
}

std::string Esp32SdMusic::resolveLongName(const std::string& path)
{
    // Đơn giản nhất: không xử lý 8.3, trả nguyên đường dẫn
    return path;
}

int Esp32SdMusic::findNextTrackIndex(int start, int direction)
{
    if (playlist_.empty()) return -1;
    int count = static_cast<int>(playlist_.size());
    if (start < 0 || start >= count)
        return 0;
    int result = (start + direction + count) % count;
    return result;
}

size_t Esp32SdMusic::countTracksInDirectory(const std::string& relative_dir)
{
    std::string full;
    if (!resolveDirectoryRelative(relative_dir, full)) {
        return 0;
    }
    std::vector<TrackInfo> tmp;
    // Dùng luôn id3_cache_ để không phải parse lại file cũ
    scanDirectoryRecursive(full, tmp, id3_cache_);
    return tmp.size();
}

// Đếm bài trong playlist/thư mục hiện tại
size_t Esp32SdMusic::countTracksInCurrentDirectory() const
{
    std::lock_guard<std::mutex> lock(playlist_mutex_);
    return playlist_.size();
}

// Chia trang danh sách bài hát
std::vector<Esp32SdMusic::TrackInfo>
Esp32SdMusic::listTracksPage(size_t page_index, size_t page_size) const
{
    std::vector<TrackInfo> result;
    if (page_size == 0) return result;

    std::lock_guard<std::mutex> lock(playlist_mutex_);
    if (playlist_.empty()) return result;

    size_t start = page_index * page_size;
    if (start >= playlist_.size()) return result;

    size_t end = std::min(start + page_size, playlist_.size());
    result.reserve(end - start);
    for (size_t i = start; i < end; ++i) {
        result.push_back(playlist_[i]);
    }
    return result;
}

// ============================================================================
//                         PART 2 / 3
//      SHUFFLE / REPEAT / PLAY-PAUSE-STOP / THREAD / DECODE
// ============================================================================

void Esp32SdMusic::shuffle(bool enabled)
{
    shuffle_enabled_ = enabled;
    ESP_LOGI(TAG, "Shuffle: %s", enabled ? "ON" : "OFF");
}

void Esp32SdMusic::repeat(RepeatMode mode)
{
    repeat_mode_ = mode;
    const char* mode_str =
        (mode == RepeatMode::None)      ? "None" :
        (mode == RepeatMode::RepeatOne) ? "RepeatOne" : "RepeatAll";
    ESP_LOGI(TAG, "Repeat mode = %s", mode_str);
}

bool Esp32SdMusic::play()
{
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);

        if (playlist_.empty()) {
            ESP_LOGW(TAG, "Playlist empty — reloading");
            loadTrackList();
            if (playlist_.empty()) {
                ESP_LOGE(TAG, "No MP3 files found on SD");
                return false;
            }
        }

        if (current_index_ < 0)
            current_index_ = 0;
    }

    if (state_.load() == PlayerState::Paused) {
        ESP_LOGI(TAG, "Resuming playback");
        pause_requested_ = false;
        state_.store(PlayerState::Playing);
        state_cv_.notify_all();
        return true;
    }

    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        stop_requested_ = true;
        pause_requested_ = false;
        state_cv_.notify_all();
    }

    joinPlaybackThreadWithTimeout();

    auto& app = Application::GetInstance();
    app.StopListening();
    app.GetAudioService().EnableWakeWordDetection(false);
    app.SetDeviceState(kDeviceStateSpeaking);

    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        stop_requested_ = false;
        pause_requested_ = false;
        state_.store(PlayerState::Preparing);
    }

    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.stack_size = 1024 * 3;
    cfg.prio = 5;
    cfg.thread_name = "sd_music_play";
    esp_pthread_set_cfg(&cfg);

    ESP_LOGI(TAG, "Starting playback thread");
    playback_thread_ = std::thread(&Esp32SdMusic::playbackThreadFunc, this);

    return true;
}

void Esp32SdMusic::pause()
{
    if (state_.load() == PlayerState::Playing) {
        ESP_LOGI(TAG, "Pausing playback");
        pause_requested_ = true;
    }
}

void Esp32SdMusic::stop()
{
    PlayerState st = state_.load();

    if (st == PlayerState::Stopped ||
        st == PlayerState::Error ||
        st == PlayerState::Preparing) {
        ESP_LOGW(TAG, "stop(): No SD music in progress to stop");
        return;
    }

    ESP_LOGI(TAG, "Stopping SD music playback");

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        stop_requested_ = true;
        pause_requested_ = false;
        state_cv_.notify_all();
    }

    joinPlaybackThreadWithTimeout();

    state_.store(PlayerState::Stopped);
    current_play_time_ms_ = 0;

    ESP_LOGI(TAG, "SD music stopped successfully");
}

bool Esp32SdMusic::next()
{
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (playlist_.empty()) return false;
        if (shuffle_enabled_) {
            if (playlist_.size() > 1) {
                int new_i;
                do {
                    new_i = rand() % playlist_.size();
                } while (new_i == current_index_);
                current_index_ = new_i;
            }
        } else {
            current_index_ = findNextTrackIndex(current_index_, +1);
        }
        ESP_LOGI(TAG, "Next track → #%d: %s",
                 current_index_, playlist_[current_index_].name.c_str());
    }
    return play();
}

bool Esp32SdMusic::prev()
{
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (playlist_.empty()) return false;
        if (shuffle_enabled_) {
            if (playlist_.size() > 1) {
                int new_i;
                do {
                    new_i = rand() % playlist_.size();
                } while (new_i == current_index_);
                current_index_ = new_i;
            }
        } else {
            current_index_ = findNextTrackIndex(current_index_, -1);
        }
        ESP_LOGI(TAG, "Previous track → #%d: %s",
                 current_index_, playlist_[current_index_].name.c_str());
    }
    return play();
}

void Esp32SdMusic::recordPlayHistory(int index)
{
    if (index < 0) return;

    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        play_history_indices_.push_back(index);
        const size_t kMaxHistory = 200;
        if (play_history_indices_.size() > kMaxHistory) {
            size_t remove_count = play_history_indices_.size() - kMaxHistory;
            play_history_indices_.erase(
                play_history_indices_.begin(),
                play_history_indices_.begin() + remove_count);
        }
    }

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (index >= 0 && index < (int)play_count_.size()) {
            ++play_count_[index];
        }
    }
}

// PLAYBACK THREAD
void Esp32SdMusic::playbackThreadFunc()
{
    TrackInfo track;
    int play_index = -1;
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);

        if (current_index_ < 0 || current_index_ >= (int)playlist_.size()) {
            ESP_LOGE(TAG, "Invalid current track index");
            state_.store(PlayerState::Error);
            return;
        }

        track = playlist_[current_index_];
        play_index = current_index_;
    }

    recordPlayHistory(play_index);

    state_.store(PlayerState::Playing);
    ESP_LOGI(TAG, "Playback thread start: %s", track.path.c_str());
    current_play_time_ms_ = 0;
    total_duration_ms_ = 0;

    auto display = Board::GetInstance().GetDisplay();
	if (display) {
		// Ưu tiên title từ ID3, nếu không có thì dùng name
		std::string title  = !track.title.empty() ? track.title : track.name;
		std::string artist = track.artist;   // ← lấy từ ID3

		std::string line;
		if (!artist.empty()) {
			// Ví dụ: "Sơn Tùng M-TP - Chúng Ta Của Hiện Tại"
			line = artist + " - " + title;
		} else {
			line = title;
		}

		display->SetMusicInfo(line.c_str());
		display->StartFFT();
	}

    InitializeMp3Decoder();
    mp3_frame_info_ = {};

    bool ok = decodeAndPlayFile(track);
    cleanupMp3Decoder();

    if (display) {
        display->StopFFT();
        if (final_pcm_data_fft_) {
            display->ReleaseAudioBuffFFT(final_pcm_data_fft_);
            final_pcm_data_fft_ = nullptr;
        }
    }

    resetSampleRate();

    if (stop_requested_) {
        state_.store(PlayerState::Stopped);
        return;
    }

    if (!ok) {
        ESP_LOGW(TAG, "Playback error, stopping");
        state_.store(PlayerState::Error);
        return;
    }

    ESP_LOGI(TAG, "Playback finished normally: %s", track.name.c_str());
	
	// Ưu tiên chuyển bài theo genre nếu đang bật
	if (!genre_playlist_.empty()) {
		if (playNextGenre()) return;
	}

    int next_index = -1;

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);

        if (playlist_.empty()) {
            state_.store(PlayerState::Stopped);
            return;
        }

        switch (repeat_mode_) {
            case RepeatMode::RepeatOne:
                ESP_LOGI(TAG, "[RepeatOne] → replay same track");
                next_index = current_index_;
                break;

            case RepeatMode::RepeatAll:
                ESP_LOGI(TAG, "[RepeatAll] → next");
                if (shuffle_enabled_ && playlist_.size() > 1) {
                    int new_i;
                    do {
                        new_i = rand() % playlist_.size();
                    } while (new_i == current_index_);
                    next_index = new_i;
                } else {
                    next_index = findNextTrackIndex(current_index_, +1);
                }
                break;

            case RepeatMode::None:
            default:
                ESP_LOGI(TAG, "[No repeat] → stop");

                if (current_index_ == (int)playlist_.size() - 1) {
                    state_.store(PlayerState::Stopped);
                    return;
                }

                next_index = findNextTrackIndex(current_index_, +1);
                break;
        }
    }

    if (next_index >= 0) {
        current_index_ = next_index;
        play();
    }
}

// DECODE & PLAY FILE
bool Esp32SdMusic::decodeAndPlayFile(const TrackInfo& track)
{
    if (!mp3_decoder_initialized_ && !InitializeMp3Decoder()) {
        state_.store(PlayerState::Error);
        return false;
    }

    FILE* fp = fopen(track.path.c_str(), "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Cannot open MP3 file: %s", track.path.c_str());
        state_.store(PlayerState::Error);
        return false;
    }

    struct stat st{};
    int64_t file_size = 0;
    if (stat(track.path.c_str(), &st) == 0) {
        file_size = st.st_size;
    }

    auto display = Board::GetInstance().GetDisplay();
    auto codec   = Board::GetInstance().GetAudioCodec();
    auto& app    = Application::GetInstance();

    if (!codec || !codec->output_enabled()) {
        fclose(fp);
        state_.store(PlayerState::Error);
        return false;
    }

    const int INPUT_BUF = 8192;

    uint8_t* input = (uint8_t*) heap_caps_malloc(
        INPUT_BUF, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!input) {
        ESP_LOGE(TAG, "Cannot allocate input buffer");
        fclose(fp);
        return false;
    }

    int16_t* pcm = new int16_t[2304];
    if (!pcm) {
        ESP_LOGE(TAG, "Cannot allocate PCM buffer");
        heap_caps_free(input);
        fclose(fp);
        return false;
    }

    int bytes_left = 0;
    uint8_t* read_ptr = input;
    bool id3_done = false;

    current_play_time_ms_ = 0;
    total_duration_ms_ = 0;

    state_.store(PlayerState::Playing);

    int total_bytes_played = 0;

    while (true) {
        if (stop_requested_) break;

        if (pause_requested_) {
            {
                std::unique_lock<std::mutex> lk(state_mutex_);
                state_.store(PlayerState::Paused);
                state_cv_.wait(lk, [this]() {
                    return (!pause_requested_) || stop_requested_;
                });
            }

            if (stop_requested_) break;
            state_.store(PlayerState::Playing);
        }

        {
            DeviceState current_state = app.GetDeviceState();

            if (current_state == kDeviceStateListening ||
                current_state == kDeviceStateSpeaking) {
                app.ToggleChatState();
                vTaskDelay(pdMS_TO_TICKS(300));
                continue;
            } else if (current_state != kDeviceStateIdle) {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
        }

        if (bytes_left < 1024) {
            if (bytes_left > 0 && read_ptr != input) {
                memmove(input, read_ptr, bytes_left);
            }

            size_t space = INPUT_BUF - bytes_left;
            size_t read_bytes = fread(input + bytes_left, 1, space, fp);
            if (stop_requested_) break;

            bytes_left += read_bytes;
            read_ptr = input;

            if (!id3_done && bytes_left >= 10) {
                size_t skip = SkipId3Tag(read_ptr, bytes_left);
                if (skip > 0 && skip <= (size_t)bytes_left) {
                    read_ptr += skip;
                    bytes_left -= skip;
                    ESP_LOGI(TAG, "ID3 tag skipped (%u bytes)", (unsigned)skip);
                }
                id3_done = true;
            }

            if (read_bytes == 0 && bytes_left == 0) {
                ESP_LOGI(TAG, "EOF reached");
                break;
            }
        }

        int off = MP3FindSyncWord(read_ptr, bytes_left);
        if (off < 0) {
            bytes_left = 0;
            continue;
        }

        if (off > 0) {
            read_ptr += off;
            bytes_left -= off;
        }

        int ret = MP3Decode(mp3_decoder_, &read_ptr, &bytes_left, pcm, 0);
        if (stop_requested_) break;

        if (ret != 0) {
            if (bytes_left > 1) {
                read_ptr++;
                bytes_left--;
            } else {
                bytes_left = 0;
            }
            continue;
        }

        MP3GetLastFrameInfo(mp3_decoder_, &mp3_frame_info_);
        if (mp3_frame_info_.samprate == 0 ||
            mp3_frame_info_.nChans == 0) {
            continue;
        }

        if (codec->output_sample_rate() != mp3_frame_info_.samprate) {
            ESP_LOGI(TAG, "Switch sample rate → %d Hz", mp3_frame_info_.samprate);
            codec->SetOutputSampleRate(mp3_frame_info_.samprate);
        }

        if (!codec->output_enabled()) {
            ESP_LOGW(TAG, "Audio output disabled – re-enabling.");
            codec->EnableOutput(true);
        }

        int frame_ms =
            (mp3_frame_info_.outputSamps * 1000) /
            (mp3_frame_info_.samprate * mp3_frame_info_.nChans);

        current_play_time_ms_ += frame_ms;

        if (total_duration_ms_.load() == 0 &&
			file_size > 0 &&
			mp3_frame_info_.bitrate > 0) {

			total_duration_ms_ =
				(file_size * 8LL * 1000LL) / mp3_frame_info_.bitrate;

			// Cập nhật duration/bitrate vào TrackInfo + cache
			{
				std::lock_guard<std::mutex> lock(playlist_mutex_);
				if (current_index_ >= 0 &&
					current_index_ < (int)playlist_.size()) {
					auto& ti = playlist_[current_index_];
					ti.duration_ms  = (int)total_duration_ms_.load();
					ti.bitrate_kbps = mp3_frame_info_.bitrate / 1000;

					auto it = id3_cache_.find(ti.path);
					if (it != id3_cache_.end()) {
						it->second.duration_ms  = ti.duration_ms;
						it->second.bitrate_kbps = ti.bitrate_kbps;
					}
				}
			}
		}

        int16_t* final_pcm = pcm;
        int final_samples = mp3_frame_info_.outputSamps;

        if (mp3_frame_info_.nChans == 2)
        {
            int mono_samples = final_samples / 2;
            for (int i = 0; i < mono_samples; i++) {
                int L = pcm[2 * i];
                int R = pcm[2 * i + 1];
                pcm[i] = (L + R) / 2;
            }
            final_pcm = pcm;
            final_samples = mono_samples;
        }

        AudioStreamPacket pkt;
        pkt.sample_rate = mp3_frame_info_.samprate;

        int real_frame_ms =
            (mp3_frame_info_.outputSamps * 1000) /
            (mp3_frame_info_.samprate * mp3_frame_info_.nChans);
        pkt.frame_duration = real_frame_ms;
        pkt.timestamp = 0;

        size_t pcm_bytes = final_samples * sizeof(int16_t);
        pkt.payload.resize(pcm_bytes);
        memcpy(pkt.payload.data(), final_pcm, pcm_bytes);

        app.AddAudioData(std::move(pkt));

        if (display) {
            final_pcm_data_fft_ = display->MakeAudioBuffFFT(pcm_bytes);
            display->FeedAudioDataFFT(final_pcm, pcm_bytes);
        }

        total_bytes_played += pcm_bytes;
        (void)total_bytes_played; // giữ biến nhưng không spam log
    }

    delete[] pcm;
    heap_caps_free(input);
    fclose(fp);

    return !stop_requested_;
}

// ============================================================================
//                         PART 3 / 3
//      DECODER UTIL / STATE / PROGRESS / GỢI Ý BÀI HÁT
// ============================================================================

bool Esp32SdMusic::InitializeMp3Decoder()
{
    if (mp3_decoder_initialized_) {
        ESP_LOGW(TAG, "MP3 decoder already initialized");
        return true;
    }

    mp3_decoder_ = MP3InitDecoder();
    if (!mp3_decoder_) {
        ESP_LOGE(TAG, "Failed to init MP3 decoder");
        return false;
    }

    mp3_decoder_initialized_ = true;
    ESP_LOGI(TAG, "MP3 decoder initialized (offline SD)");
    return true;
}

void Esp32SdMusic::cleanupMp3Decoder()
{
    if (mp3_decoder_) {
        MP3FreeDecoder(mp3_decoder_);
        mp3_decoder_ = nullptr;
    }
    mp3_decoder_initialized_ = false;
}

size_t Esp32SdMusic::SkipId3Tag(uint8_t* data, size_t size)
{
    if (!data || size < 10) return 0;
    if (memcmp(data, "ID3", 3) != 0) return 0;

    uint32_t tag_sz =
        ((data[6] & 0x7F) << 21) |
        ((data[7] & 0x7F) << 14) |
        ((data[8] & 0x7F) << 7)  |
         (data[9] & 0x7F);

    size_t total = 10 + tag_sz;
    if (total > size) total = size;

    ESP_LOGI(TAG, "Skip ID3v2 tag: %u bytes", (unsigned)total);
    return total;
}

void Esp32SdMusic::resetSampleRate()
{
    auto codec = Board::GetInstance().GetAudioCodec();
    if (!codec) return;

    int orig = codec->original_output_sample_rate();
    if (orig <= 0) return;

    int cur = codec->output_sample_rate();
    if (cur != orig) {
        ESP_LOGI(TAG, "Reset sample rate: %d → %d", cur, orig);
        codec->SetOutputSampleRate(-1);
    }
}

Esp32SdMusic::TrackProgress Esp32SdMusic::updateProgress() const
{
    TrackProgress p;
    p.position_ms = current_play_time_ms_.load();
    p.duration_ms = total_duration_ms_.load();
    return p;
}

int16_t* Esp32SdMusic::getFFTData() const
{
    return final_pcm_data_fft_;
}

Esp32SdMusic::PlayerState Esp32SdMusic::getState() const
{
    return state_.load();
}

int Esp32SdMusic::getBitrate() const
{
    // mp3_frame_info_.bitrate thường là kbps; nếu chưa decode sẽ = 0
    int br = mp3_frame_info_.bitrate;
    if (br < 0) br = 0;
    return br;
}

int64_t Esp32SdMusic::getDurationMs() const
{
    // Thời lượng tổng đang được tính trong luồng giải mã
    return total_duration_ms_.load();
}

int64_t Esp32SdMusic::getCurrentPositionMs() const
{
    // Vị trí hiện tại đã phát (ms)
    return current_play_time_ms_.load();
}

std::string Esp32SdMusic::getDurationString() const
{
    return MsToTimeString(total_duration_ms_.load());
}

std::string Esp32SdMusic::getCurrentTimeString() const
{
    return MsToTimeString(current_play_time_ms_.load());
}

// Gợi ý bài tiếp theo dựa trên lịch sử phát
std::vector<Esp32SdMusic::TrackInfo>
Esp32SdMusic::suggestNextTracks(size_t max_results)
{
    std::vector<TrackInfo> results;
    if (max_results == 0) return results;

    int base_index = -1;
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        if (!play_history_indices_.empty()) {
            base_index = play_history_indices_.back();
        }
    }

    std::vector<TrackInfo> playlist_copy;
    std::vector<uint32_t> count_copy;
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (playlist_.empty()) return results;
        playlist_copy = playlist_;
        count_copy = play_count_;
    }

    if (base_index < 0 || base_index >= (int)playlist_copy.size()) {
        size_t limit = std::min(max_results, playlist_copy.size());
        results.assign(playlist_copy.begin(),
                       playlist_copy.begin() + limit);
        return results;
    }

    TrackInfo base = playlist_copy[base_index];

    struct Scored { int index; int score; };
    std::vector<Scored> scored;
    int n = static_cast<int>(playlist_copy.size());
    scored.reserve(std::max(0, n - 1));

    for (int i = 0; i < n; ++i) {
        if (i == base_index) continue;
        uint32_t pc = (i < (int)count_copy.size()) ? count_copy[i] : 0;
        int s = ComputeTrackScoreForBase(base, playlist_copy[i], pc);
        scored.push_back({i, s});
    }

    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.index < b.index;
              });

    size_t limit = std::min(max_results, scored.size());
    results.reserve(limit);
    for (size_t i = 0; i < limit; ++i) {
        results.push_back(playlist_copy[scored[i].index]);
    }

    return results;
}

// Gợi ý bài giống bài X
std::vector<Esp32SdMusic::TrackInfo>
Esp32SdMusic::suggestSimilarTo(const std::string& name_or_path,
                               size_t max_results)
{
    std::vector<TrackInfo> results;
    if (max_results == 0) return results;

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (playlist_.empty()) {
            ESP_LOGW(TAG, "suggestSimilarTo(): playlist empty — reloading");
        }
    }
    if (playlist_.empty()) {
        if (!loadTrackList()) {
            ESP_LOGE(TAG, "suggestSimilarTo(): cannot load playlist");
            return results;
        }
    }

    int base_index = findTrackIndexByKeyword(name_or_path);
    if (base_index < 0) {
        return suggestNextTracks(max_results);
    }

    std::vector<TrackInfo> playlist_copy;
    std::vector<uint32_t> count_copy;
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        playlist_copy = playlist_;
        count_copy = play_count_;
    }

    if (playlist_copy.empty() ||
        base_index < 0 ||
        base_index >= (int)playlist_copy.size()) {
        return results;
    }

    TrackInfo base = playlist_copy[base_index];

    struct Scored { int index; int score; };
    std::vector<Scored> scored;
    int n = static_cast<int>(playlist_copy.size());
    scored.reserve(std::max(0, n - 1));

    for (int i = 0; i < n; ++i) {
        if (i == base_index) continue;
        uint32_t pc = (i < (int)count_copy.size()) ? count_copy[i] : 0;
        int s = ComputeTrackScoreForBase(base, playlist_copy[i], pc);
        scored.push_back({i, s});
    }

    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.index < b.index;
              });

    size_t limit = std::min(max_results, scored.size());
    results.reserve(limit);
    for (size_t i = 0; i < limit; ++i) {
        results.push_back(playlist_copy[scored[i].index]);
    }

    return results;
}

// Tạo danh sách bài theo thể loại (genre)
bool Esp32SdMusic::buildGenrePlaylist(const std::string& genre)
{
    std::string kw = ToLowerAscii(genre);
    if (kw.empty()) return false;

    std::vector<int> indices;

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        for (int i = 0; i < (int)playlist_.size(); ++i) {
            std::string g = ToLowerAscii(playlist_[i].genre);
            if (!g.empty() && g.find(kw) != std::string::npos) {
                indices.push_back(i);
            }
        }
    }

    if (indices.empty()) {
        ESP_LOGW(TAG, "No tracks found with genre '%s'", genre.c_str());
        return false;
    }

    genre_playlist_ = indices;
    genre_current_key_ = genre;
    genre_current_pos_ = 0;

    ESP_LOGI(TAG, "Genre playlist built for '%s' (%d tracks)",
             genre.c_str(), (int)indices.size());
    return true;
}

// Phát bài thứ pos trong genre playlist
bool Esp32SdMusic::playGenreIndex(int pos)
{
    if (genre_playlist_.empty())
        return false;

    if (pos < 0 || pos >= (int)genre_playlist_.size())
        return false;

    int track_index = genre_playlist_[pos];

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (track_index < 0 || track_index >= (int)playlist_.size())
            return false;

        current_index_ = track_index;
    }

    genre_current_pos_ = pos;

    ESP_LOGI(TAG, "Play genre-track [%d/%d] → index %d (%s)",
             pos + 1, (int)genre_playlist_.size(),
             track_index,
             playlist_[track_index].name.c_str());

    return play();
}

// Phát bài kế tiếp trong danh sách thể loại
bool Esp32SdMusic::playNextGenre()
{
    if (genre_playlist_.empty())
        return false;

    int next_pos = genre_current_pos_ + 1;

    if (next_pos >= (int)genre_playlist_.size()) {
        ESP_LOGI(TAG, "End of genre playlist '%s'", genre_current_key_.c_str());
        return false;
    }

    genre_current_pos_ = next_pos;
    int track_index = genre_playlist_[next_pos];

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (track_index < 0 || track_index >= (int)playlist_.size())
            return false;

        current_index_ = track_index;
    }

    ESP_LOGI(TAG, "Next genre track → pos=%d → index=%d (%s)",
             next_pos,
             track_index,
             playlist_[track_index].name.c_str());

    return play();
}
