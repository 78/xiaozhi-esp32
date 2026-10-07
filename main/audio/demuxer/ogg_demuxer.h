#ifndef OGG_DEMUXER_H_
#define OGG_DEMUXER_H_

#include <functional>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

class OggDemuxer {
private:
    enum ParseState : int8_t {
        FIND_PAGE,
        PARSE_HEADER,
        PARSE_SEGMENTS,
        PARSE_DATA
    };

    struct Opus_t {
        bool    head_seen{false};
        bool    tags_seen{false};
        bool    mono{false};
        int     sample_rate{48000};
    };


    // Use fixed-size buffers to avoid dynamic allocation.
    struct context_t {
        bool packet_continued{false};   // Whether the current packet spans segments
        uint8_t header[27];             // Ogg page header
        uint8_t seg_table[255];         // Current segment table
        uint8_t packet_buf[2048];       // 2 KB packet buffer
        size_t packet_len = 0;          // Bytes accumulated in the packet buffer
        size_t seg_count = 0;           // Segment count in the current page
        size_t seg_index = 0;           // Current segment index
        size_t data_offset = 0;         // Bytes read in the current parsing stage
        size_t bytes_needed = 0;        // Bytes still needed for the current field
        size_t seg_remaining = 0;       // Bytes remaining in the current segment
        size_t body_size = 0;           // Total page body size
        size_t body_offset = 0;         // Bytes read from the page body
    };
    
public:
    OggDemuxer() {
        Reset();
    }
    
    void Reset();
    
    size_t Process(const uint8_t* data, size_t size);

    bool Finish() const;
    bool HasError() const { return has_error_; }

    void OnPacket(std::function<void(const uint8_t* data, int sample_rate, int frame_duration_ms,
                                     size_t len)> on_packet) {
        on_packet_ = std::move(on_packet);
    }
private:

    ParseState  state_ = ParseState::FIND_PAGE;
    context_t   ctx_;
    Opus_t      opus_info_;
    bool has_error_ = false;
    size_t packet_count_ = 0;
    std::function<void(const uint8_t*, int, int, size_t)> on_packet_;

    static int GetOpusPacketDurationMs(const uint8_t* data, size_t size);
};

#endif
