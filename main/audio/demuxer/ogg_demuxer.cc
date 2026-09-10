#include "ogg_demuxer.h"

#include <algorithm>

#include "esp_log.h"

#define TAG "OggDemuxer"

/// @brief Reset the demuxer.
void OggDemuxer::Reset()
{
    opus_info_ = {
        .head_seen = false,
        .tags_seen = false,
        .mono = false,
        .sample_rate = 48000
    };

    has_error_ = false;
    packet_count_ = 0;

    state_ = ParseState::FIND_PAGE;
    ctx_.packet_len = 0;
    ctx_.seg_count = 0;
    ctx_.seg_index = 0;
    ctx_.data_offset = 0;
    ctx_.bytes_needed = 4;          // Four bytes are needed for "OggS"
    ctx_.seg_remaining = 0;
    ctx_.body_size = 0;
    ctx_.body_offset = 0;
    ctx_.packet_continued = false;
    
    // Clear buffered data.
    memset(ctx_.header, 0, sizeof(ctx_.header));
    memset(ctx_.seg_table, 0, sizeof(ctx_.seg_table));
    memset(ctx_.packet_buf, 0, sizeof(ctx_.packet_buf));
}

int OggDemuxer::GetOpusPacketDurationMs(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return -1;
    }

    const uint8_t toc = data[0];
    const uint8_t config = toc >> 3;
    int frame_duration_us = 0;
    if (config < 12) {
        static constexpr int kSilkDurationsUs[] = {10000, 20000, 40000, 60000};
        frame_duration_us = kSilkDurationsUs[config & 0x03];
    } else if (config < 16) {
        frame_duration_us = (config & 0x01) ? 20000 : 10000;
    } else {
        static constexpr int kCeltDurationsUs[] = {2500, 5000, 10000, 20000};
        frame_duration_us = kCeltDurationsUs[config & 0x03];
    }

    int frame_count = 0;
    switch (toc & 0x03) {
        case 0:
            frame_count = 1;
            break;
        case 1:
        case 2:
            frame_count = 2;
            break;
        case 3:
            if (size < 2) {
                return -1;
            }
            frame_count = data[1] & 0x3f;
            break;
    }

    const int packet_duration_us = frame_duration_us * frame_count;
    if (frame_count == 0 || packet_duration_us > 120000 || packet_duration_us % 1000 != 0) {
        return -1;
    }
    return packet_duration_us / 1000;
}

bool OggDemuxer::Finish() const {
    return !has_error_ && opus_info_.head_seen && opus_info_.tags_seen && opus_info_.mono &&
           packet_count_ > 0 && state_ == ParseState::FIND_PAGE && ctx_.bytes_needed == 4 &&
           ctx_.packet_len == 0;
}

/// @brief Process an input block.
/// @param data Input data.
/// @param size Input size in bytes.
/// @return Number of bytes processed.
size_t OggDemuxer::Process(const uint8_t* data, size_t size)
{
    size_t processed = 0;  // Number of bytes processed
    
    while (processed < size) {
        switch (state_) {
          case ParseState::FIND_PAGE: {
            // Find the "OggS" page capture pattern.
            if (ctx_.bytes_needed < 4) {
                // Continue a partial "OggS" match across input blocks.
                size_t to_copy = std::min(size - processed, ctx_.bytes_needed);
                memcpy(ctx_.header + (4 - ctx_.bytes_needed), data + processed, to_copy);
                
                processed += to_copy;
                ctx_.bytes_needed -= to_copy;
                
                if (ctx_.bytes_needed == 0) {
                    // Check whether the capture pattern matches "OggS".
                    if (memcmp(ctx_.header, "OggS", 4) == 0) {
                        state_ = ParseState::PARSE_HEADER;
                        ctx_.data_offset = 4;
                        ctx_.bytes_needed = 27 - 4;  // 23 more bytes complete the header
                    } else {
                        // Shift by one byte and continue matching.
                        memmove(ctx_.header, ctx_.header + 1, 3);
                        ctx_.bytes_needed = 1;
                    }
                } else {
                    // Wait for more data.
                    return processed;
                }
            } else if (ctx_.bytes_needed == 4) {
                // Search the input block for a complete "OggS" pattern.
                bool found = false;
                size_t i = 0;
                size_t remaining = size - processed;
                
                // Search for "OggS".
                for (; i + 4 <= remaining; i++) {
                    if (memcmp(data + processed + i, "OggS", 4) == 0) {
                        found = true;
                        break;
                    }
                }
                
                if (found) {
                    // Skip bytes before the matched "OggS" pattern.
                    processed += i;
                    
                    // The matched "OggS" bytes do not need to be copied.
                    // memcpy(ctx_.header, data + processed, 4);
                    processed += 4;
                    
                    state_ = ParseState::PARSE_HEADER;
                    ctx_.data_offset = 4;
                    ctx_.bytes_needed = 27 - 4;  // 23 more bytes are needed
                } else {
                    // Save a possible partial match when no complete pattern is found.
                    size_t partial_len = remaining - i;
                    if (partial_len > 0) {
                        memcpy(ctx_.header, data + processed + i, partial_len);
                        ctx_.bytes_needed = 4 - partial_len;
                        processed += i + partial_len;
                    } else {
                        processed += i;  // All bytes have been searched
                    }
                    return processed;
                }
            } else {
                ESP_LOGE(TAG, "OggDemuxer run in error state: bytes_needed=%zu", ctx_.bytes_needed);
                has_error_ = true;
                return processed;
            }
            break;
          }
            
          case ParseState::PARSE_HEADER: {
            size_t available = size - processed;
            
            if (available < ctx_.bytes_needed) {
                // Copy the available bytes and wait for more data.
                memcpy(ctx_.header + ctx_.data_offset, 
                        data + processed, available);
                
                ctx_.data_offset += available;
                ctx_.bytes_needed -= available;
                processed += available;
                return processed;
            } else {
                // Complete the page header.
                size_t to_copy = ctx_.bytes_needed;
                memcpy(ctx_.header + ctx_.data_offset, 
                        data + processed, to_copy);
                
                processed += to_copy;
                ctx_.data_offset += to_copy;
                ctx_.bytes_needed = 0;
                
                // Validate the page header.
                if (ctx_.header[4] != 0) {
                    ESP_LOGE(TAG, "Invalid Ogg version: %d", ctx_.header[4]);
                    has_error_ = true;
                    return processed;
                }
                
                ctx_.seg_count = ctx_.header[26];
                if (ctx_.seg_count > 0 && ctx_.seg_count <= 255) {
                    state_ = ParseState::PARSE_SEGMENTS;
                    ctx_.bytes_needed = ctx_.seg_count;
                    ctx_.data_offset = 0;
                } else if (ctx_.seg_count == 0) {
                    // Skip directly to the next page when there are no segments.
                    state_ = ParseState::FIND_PAGE;
                    ctx_.bytes_needed = 4;
                    ctx_.data_offset = 0;
                } else {
                    ESP_LOGE(TAG, "Invalid Ogg segment count: %u", ctx_.seg_count);
                    has_error_ = true;
                    return processed;
                }
            }
            break;
        }
            
          case ParseState::PARSE_SEGMENTS: {
            size_t available = size - processed;
            
            if (available < ctx_.bytes_needed) {
                memcpy(ctx_.seg_table + ctx_.data_offset, 
                        data + processed, available);
                
                ctx_.data_offset += available;
                ctx_.bytes_needed -= available;
                processed += available;
                return processed;
            } else {
                size_t to_copy = ctx_.bytes_needed;
                memcpy(ctx_.seg_table + ctx_.data_offset, 
                        data + processed, to_copy);
                
                processed += to_copy;
                ctx_.data_offset += to_copy;
                ctx_.bytes_needed = 0;
                
                state_ = ParseState::PARSE_DATA;
                ctx_.seg_index = 0;
                ctx_.data_offset = 0;
                
                // Calculate the total page body size.
                ctx_.body_size = 0;
                for (size_t i = 0; i < ctx_.seg_count; ++i) {
                    ctx_.body_size += ctx_.seg_table[i];
                }
                ctx_.body_offset = 0;
                ctx_.seg_remaining = 0;
            }
            break;
        }
            
          case ParseState::PARSE_DATA: {
            while (ctx_.seg_index < ctx_.seg_count && processed < size) {
                uint8_t seg_len = ctx_.seg_table[ctx_.seg_index];
                
                // Continue a partially read segment.
                if (ctx_.seg_remaining > 0) {
                    seg_len = ctx_.seg_remaining;
                } else {
                    ctx_.seg_remaining = seg_len;
                }
                
                // Check that the packet buffer has enough space.
                if (ctx_.packet_len + seg_len > sizeof(ctx_.packet_buf)) {
                    ESP_LOGE(TAG, "Ogg packet buffer overflow: %zu + %u > %zu",
                             ctx_.packet_len, seg_len, sizeof(ctx_.packet_buf));
                    has_error_ = true;
                    return processed;
                }
                
                // Copy segment data.
                size_t to_copy = std::min(size - processed, (size_t)seg_len);
                memcpy(ctx_.packet_buf + ctx_.packet_len, data + processed, to_copy);
                
                processed += to_copy;
                ctx_.packet_len += to_copy;
                ctx_.body_offset += to_copy;
                ctx_.seg_remaining -= to_copy;
                
                // Check whether the segment is complete.
                if (ctx_.seg_remaining > 0) {
                    // Wait for the rest of the segment.
                    return processed;
                }
                
                // The segment is complete.
                bool seg_continued = (ctx_.seg_table[ctx_.seg_index] == 255);
                
                if (!seg_continued) {
                    // The packet ends at this segment.
                    if (ctx_.packet_len) {
                        if (!opus_info_.head_seen) {
                            if (ctx_.packet_len >=8 && memcmp(ctx_.packet_buf, "OpusHead", 8) == 0) {
                                opus_info_.head_seen = true;
                                if (ctx_.packet_len >= 19) {
                                    opus_info_.mono = ctx_.packet_buf[9] == 1;
                                    const uint32_t input_sample_rate =
                                        static_cast<uint32_t>(ctx_.packet_buf[12]) |
                                        (static_cast<uint32_t>(ctx_.packet_buf[13]) << 8) |
                                        (static_cast<uint32_t>(ctx_.packet_buf[14]) << 16) |
                                        (static_cast<uint32_t>(ctx_.packet_buf[15]) << 24);
                                    switch (input_sample_rate) {
                                        case 8000:
                                        case 12000:
                                        case 16000:
                                        case 24000:
                                        case 48000:
                                            opus_info_.sample_rate = input_sample_rate;
                                            break;
                                        default:
                                            // The OpusHead input rate is informational. Decode at a
                                            // native Opus rate when it is not directly supported.
                                            opus_info_.sample_rate = 48000;
                                            break;
                                    }
                                    ESP_LOGD(TAG, "OpusHead found, sample_rate=%d", opus_info_.sample_rate);
                                    if (!opus_info_.mono) {
                                        ESP_LOGE(TAG, "Only mono Ogg Opus streams are supported");
                                        has_error_ = true;
                                        return processed;
                                    }
                                } else {
                                    has_error_ = true;
                                    return processed;
                                }
                                ctx_.packet_len = 0;
                                ctx_.packet_continued = false;
                                ctx_.seg_index++;
                                ctx_.seg_remaining = 0;
                                continue;
                            }
                        }
                        if (!opus_info_.tags_seen) {
                            if (ctx_.packet_len >= 8 && memcmp(ctx_.packet_buf, "OpusTags", 8) == 0) {
                                opus_info_.tags_seen = true;
                                ESP_LOGD(TAG, "OpusTags found.");
                                ctx_.packet_len = 0;
                                ctx_.packet_continued = false;
                                ctx_.seg_index++;
                                ctx_.seg_remaining = 0;
                                continue;  
                            }
                        }
                        if (opus_info_.head_seen && opus_info_.tags_seen) {
                            const int frame_duration_ms =
                                GetOpusPacketDurationMs(ctx_.packet_buf, ctx_.packet_len);
                            if (frame_duration_ms <= 0) {
                                ESP_LOGE(TAG, "Unsupported Opus packet duration");
                                has_error_ = true;
                                return processed;
                            }
                            ++packet_count_;
                            if (on_packet_) {
                                on_packet_(ctx_.packet_buf, opus_info_.sample_rate,
                                           frame_duration_ms, ctx_.packet_len);
                            }
                        } else {
                            ESP_LOGW(TAG, "Dropping Ogg packet before OpusHead/OpusTags");
                        }
                    }
                    ctx_.packet_len = 0;
                    ctx_.packet_continued = false;
                } else {
                    ctx_.packet_continued = true;
                }
                
                ctx_.seg_index++;
                ctx_.seg_remaining = 0;
            }
            
            if (ctx_.seg_index == ctx_.seg_count) {
                // Check whether the complete page body was read.
                if (ctx_.body_offset < ctx_.body_size) {
                    ESP_LOGW(TAG, "Incomplete Ogg page body: %zu/%zu",
                            ctx_.body_offset, ctx_.body_size);
                }
                
                // Preserve packet state when a packet continues on the next page.
                if (!ctx_.packet_continued) {
                    ctx_.packet_len = 0;
                }
                
                // Continue with the next page.
                state_ = ParseState::FIND_PAGE;
                ctx_.bytes_needed = 4;
                ctx_.data_offset = 0;
            }
            break;
        }
        }
    }
    
    return processed;
}
