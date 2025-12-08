#ifndef ESP32_SD_MUSIC_H
#define ESP32_SD_MUSIC_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <unordered_map>

extern "C" {
#include "mp3dec.h"
}

class Esp32SdMusic {
public:
    // ============================================================
    // 1) ENUM — State Machine
    // ============================================================
    enum class PlayerState {
        Stopped = 0,   // Không phát
        Preparing,     // Đang chuẩn bị phát
        Playing,       // Đang phát
        Paused,        // Tạm dừng
        Error          // Lỗi
    };

    // ============================================================
    // 2) ENUM — Repeat modes
    // ============================================================
    enum class RepeatMode {
        None = 0,      // Không lặp
        RepeatOne,     // Lặp 1 bài
        RepeatAll      // Lặp toàn playlist
    };

    // ============================================================
    // 3) Struct TrackInfo — dữ liệu một bài
    // ============================================================
    struct TrackInfo {
		// ============================================
		// Hiển thị
		// ============================================
		std::string name;   // Tên hiển thị ưu tiên ID3
		std::string path;   // Đường dẫn tuyệt đối

		// ============================================
		// ID3 TEXT TAGS (ID3v1 + ID3v2)
		// ============================================
		std::string title;   
		std::string artist;
		std::string album;
		std::string genre;
		std::string comment;
		std::string year;
		int         track_number = 0;

		// ============================================
		// AUDIO INFO (tự cập nhật khi decode)
		// ============================================
		int duration_ms  = 0;
		int bitrate_kbps = 0;

		// ============================================
		// METADATA CACHE (dùng để xác định file thay đổi)
		// ============================================
		size_t file_size = 0;
		time_t mtime     = 0;

		// ============================================
		// COVER ART (metadata offset, không load ảnh)
		// ============================================
		uint32_t cover_offset = 0;
		uint32_t cover_size   = 0;
		std::string cover_mime;
	};

    // ============================================================
    // 4) Struct Progress — dành cho UI
    // ============================================================
    struct TrackProgress {
        int64_t position_ms = 0;
        int64_t duration_ms = 0;
    };

public:
    // ============================================================
    // 5) Constructor / Destructor
    // ============================================================
    Esp32SdMusic();
    ~Esp32SdMusic();

    void Initialize(class SdCard* sd_card);

    // ============================================================
    // 6) Playlist API cơ bản
    // ============================================================
    bool loadTrackList();                          // Quét SD / thư mục hiện tại
    size_t getTotalTracks() const;                 // Tổng số bài trong playlist hiện tại
    std::vector<TrackInfo> listTracks() const;     // Toàn bộ playlist

    bool setDirectory(const std::string& relative_dir);  // Chọn thư mục làm root
    bool playDirectory(const std::string& relative_dir); // Chọn + phát từ thư mục

    bool playByName(const std::string& keyword);   // Phát bài theo tên / từ khóa
    TrackInfo getTrackInfo(int index) const;       // Lấy thông tin bài theo index
    bool setTrack(int index);                      // Chọn bài theo index rồi play

    std::string getCurrentTrack() const;           // Tên bài hiện tại
    std::string getCurrentTrackPath() const;       // Đường dẫn tuyệt đối

    std::vector<std::string> listDirectories() const;         // Liệt kê thư mục con
    std::vector<TrackInfo> searchTracks(const std::string& keyword) const; // Tìm kiếm trong playlist

    // FAT short-name + case-insensitive dir resolver
    std::string resolveLongName(const std::string& path);
    std::string resolveCaseInsensitiveDir(const std::string& path);

    // Đếm số bài .mp3 trong thư mục bất kỳ (tên tương đối từ mount point)
    size_t countTracksInDirectory(const std::string& relative_dir);

    // Đếm số bài trong playlist hiện tại (thư mục hiện tại)
    size_t countTracksInCurrentDirectory() const;

    // Liệt kê theo trang: page_index = 0 → trang 1, mỗi trang mặc định 10 bài
    std::vector<TrackInfo> listTracksPage(size_t page_index,
                                          size_t page_size = 10) const;

    // ============================================================
    // 7) Playback API
    // ============================================================
    bool play();       // Nếu đang Paused → resume, nếu không → start thread mới
    void pause();      // Tạm dừng nhưng không reset
    void stop();       // Dừng hoàn toàn, reset progress

    bool next();       // Bài kế tiếp (hỗ trợ shuffle/repeat)
    bool prev();       // Bài trước đó

    // ============================================================
    // 8) Playback Settings
    // ============================================================
    void shuffle(bool enabled);         // Bật/tắt shuffle
    void repeat(RepeatMode mode);       // Repeat none/one/all

    // ============================================================
    // 9) Query state / FFT
    // ============================================================
    PlayerState getState() const;
    TrackProgress updateProgress() const;
    int16_t* getFFTData() const;

    // --- Helper cho UI: bitrate + thời gian ---
    // Dùng cho LcdDisplay (music UI) để vẽ thanh tiến trình + text
    int getBitrate() const;                // kbps (0 nếu chưa đọc được)
    int64_t getDurationMs() const;         // tổng thời lượng hiện tại (ms)
    int64_t getCurrentPositionMs() const;  // vị trí đang phát (ms)
    std::string getDurationString() const; // "mm:ss" hoặc "hh:mm:ss"
    std::string getCurrentTimeString() const;

    // ============================================================
    // 10) Chế độ gợi ý bài hát
    // ============================================================
    // Gợi ý vài bài kế tiếp dựa trên lịch sử phát + thư mục + tần suất
    std::vector<TrackInfo> suggestNextTracks(size_t max_results = 5);

    // Gợi ý bài giống bài X (theo tên hoặc path keyword)
    std::vector<TrackInfo> suggestSimilarTo(const std::string& name_or_path,
                                            size_t max_results = 5);

	// ============================================================
    // 11) Playlist theo THỂ LOẠI
    // ============================================================
    bool buildGenrePlaylist(const std::string& genre); // tạo danh sách các bài theo thể loại
    bool playGenreIndex(int pos);                      // phát bài thứ pos trong danh sách thể loại
    bool playNextGenre();                              // tự chuyển sang bài kế tiếp trong thể loại
	
	// Liệt kê tất cả thể loại hiện có trong playlist
    std::vector<std::string> listGenres() const;
	
private:
    // ============================================================
    // Playlist helpers
    // ============================================================
    void scanDirectoryRecursive(const std::string& dir,
                            std::vector<TrackInfo>& out,
                            std::unordered_map<std::string, TrackInfo>& cache);

    int findNextTrackIndex(int start, int direction);

    // Chuẩn hóa đường dẫn thư mục tương đối → tuyệt đối UTF-8 hợp lệ
    bool resolveDirectoryRelative(const std::string& relative_dir,
                                  std::string& out_full);

    // Tìm index theo keyword (tên hoặc đường dẫn), so khớp UTF-8, case-insensitive ASCII
    int findTrackIndexByKeyword(const std::string& keyword) const;

    // ============================================================
    // Playback Thread
    // ============================================================
    void playbackThreadFunc();              // Thread main loop
    bool decodeAndPlayFile(const TrackInfo& track);

    void joinPlaybackThreadWithTimeout();   // Gom code join/detach thread

    // ============================================================
    // MP3 Decoder Utilities
    // ============================================================
    bool InitializeMp3Decoder();            // Init mini-mp3
    void cleanupMp3Decoder();               // Free decoder
    size_t SkipId3Tag(uint8_t* data, size_t size);
    void resetSampleRate();                 // Restore sample-rate codec

    // ============================================================
    // Lịch sử phát & gợi ý
    // ============================================================
    void recordPlayHistory(int index);      // Cập nhật history + play_count

private:
    // ============================================================
    // DATA MEMBERS
    // ============================================================
    SdCard* sd_card_;                       // Thẻ SD được gắn kết
    // Playlist / thư mục
    std::string root_directory_;
    std::vector<TrackInfo> playlist_;
    mutable std::mutex playlist_mutex_;
    int current_index_;
    std::vector<uint32_t> play_count_;      // Đếm số lần phát từng bài
	// Cache ID3 toàn bộ file đã từng thấy trong session (RAM only)
    std::unordered_map<std::string, TrackInfo> id3_cache_;

    // Playback state / thread
    std::thread playback_thread_;
    std::atomic<bool> stop_requested_;
    std::atomic<bool> pause_requested_;
    std::atomic<PlayerState> state_;

    mutable std::mutex state_mutex_;
    std::condition_variable state_cv_;

    // Playback options
    bool shuffle_enabled_;
    RepeatMode repeat_mode_;

    // Progress tracking
    std::atomic<int64_t> current_play_time_ms_;
    std::atomic<int64_t> total_duration_ms_;

    // FFT buffer (display owns memory)
    int16_t* final_pcm_data_fft_;

    // mini-mp3 decoder
    void* mp3_decoder_;
    bool mp3_decoder_initialized_;
    MP3FrameInfo mp3_frame_info_;
	
    // PLAYLIST THEO THỂ LOẠI (genre playlist)
    std::vector<int> genre_playlist_;     // danh sách index các bài trùng thể loại
    int genre_current_pos_ = -1;          // đang ở bài thứ mấy trong genre_playlist
    std::string genre_current_key_;       // thể loại hiện tại (vd: "rock")

    // History / gợi ý
    mutable std::mutex history_mutex_;
    std::vector<int> play_history_indices_; // FIFO index đã phát (giới hạn kích thước)

};

#endif // ESP32_SD_MUSIC_H
