#include "ogg_demuxer.h"
#include "esp_log.h"

#define TAG "OggDemuxer"

/// @brief 重置解封器
void OggDemuxer::Reset()
{
    opus_info_ = {
        .head_seen = false,
        .tags_seen = false,
        .sample_rate = 48000
    };

    state_ = ParseState::FIND_PAGE;
    ctx_.packet_len = 0;
    ctx_.seg_count = 0;
    ctx_.seg_index = 0;
    ctx_.data_offset = 0;
    ctx_.bytes_needed = 4;          // 需要4字节"OggS"
    ctx_.seg_remaining = 0;
    ctx_.body_size = 0;
    ctx_.body_offset = 0;
    ctx_.packet_continued = false;
    
    // 清空缓冲区数据
    memset(ctx_.header, 0, sizeof(ctx_.header));
    memset(ctx_.seg_table, 0, sizeof(ctx_.seg_table));
    memset(ctx_.packet_buf, 0, sizeof(ctx_.packet_buf));
}

/// @brief 处理数据块
/// @param data 输入数据
/// @param size 输入数据大小
/// @return 已处理的字节数
size_t OggDemuxer::Process(const uint8_t* data, size_t size)
{
    size_t processed = 0;  // 已处理的字节数
    
    while (processed < size) {
        switch (state_) {
          case ParseState::FIND_PAGE: {
            // 寻找页头"OggS"
            if (ctx_.bytes_needed < 4) {
                // 处理不完整的"OggS"匹配（跨数据块）
                size_t to_copy = std::min(size - processed, ctx_.bytes_needed);
                memcpy(ctx_.header + (4 - ctx_.bytes_needed), data + processed, to_copy);
                
                processed += to_copy;
                ctx_.bytes_needed -= to_copy;
                
                if (ctx_.bytes_needed == 0) {
                    // 检查是否匹配"OggS"
                    if (memcmp(ctx_.header, "OggS", 4) == 0) {
                        state_ = ParseState::PARSE_HEADER;
                        ctx_.data_offset = 4;
                        ctx_.bytes_needed = 27 - 4;  // 还需要23字节完成页头
                    } else {
                        // 匹配失败，滑动1字节继续匹配
                        memmove(ctx_.header, ctx_.header + 1, 3);
                        ctx_.bytes_needed = 1;
                    }
                } else {
                    // 数据不足，等待更多数据
                    return processed;
                }
            } else if (ctx_.bytes_needed == 4) {
                // 在数据块中查找完整的"OggS"
                bool found = false;
                size_t i = 0;
                size_t remaining = size - processed;
                
                // 搜索"OggS"
                for (; i + 4 <= remaining; i++) {
                    if (memcmp(data + processed + i, "OggS", 4) == 0) {
                        found = true;
                        break;
                    }
                }
                
                if (found) {
                    // 找到"OggS"，跳过已搜索的字节
                    processed += i;
                    
                    // 不记录找到的"OggS"，无必要
                    // memcpy(ctx_.header, data + processed, 4);
                    processed += 4;
                    
                    state_ = ParseState::PARSE_HEADER;
                    ctx_.data_offset = 4;
                    ctx_.bytes_needed = 27 - 4;  // 还需要23字节
                } else {
                    // 没有找到完整"OggS"，保存可能的部分匹配
                    size_t partial_len = remaining - i;
                    if (partial_len > 0) {
                        memcpy(ctx_.header, data + processed + i, partial_len);
                        ctx_.bytes_needed = 4 - partial_len;
                        processed += i + partial_len;
                    } else {
                        processed += i;  // 已搜索所有字节
                    }
                    return processed;  // 返回已处理的字节数
                }
            } else {
                ESP_LOGE(TAG, "OggDemuxer run in error state: bytes_needed=%zu", ctx_.bytes_needed);
                Reset();
                return processed;
            }
            break;
          }
            
          case ParseState::PARSE_HEADER: {
            size_t available = size - processed;
            
            if (available < ctx_.bytes_needed) {
                // 数据不足，复制可用的部分
                memcpy(ctx_.header + ctx_.data_offset, 
                        data + processed, available);
                
                ctx_.data_offset += available;
                ctx_.bytes_needed -= available;
                processed += available;
                return processed;  // 等待更多数据
            } else {
                // 有足够的数据完成页头
                size_t to_copy = ctx_.bytes_needed;
                memcpy(ctx_.header + ctx_.data_offset, 
                        data + processed, to_copy);
                
                processed += to_copy;
                ctx_.data_offset += to_copy;
                ctx_.bytes_needed = 0;
                
                // 验证页头
                if (ctx_.header[4] != 0) {
                    ESP_LOGE(TAG, "无效的Ogg版本: %d", ctx_.header[4]);
                    state_ = ParseState::FIND_PAGE;
                    ctx_.bytes_needed = 4;
                    ctx_.data_offset = 0;
                    break;
                }
                
                ctx_.seg_count = ctx_.header[26];
                if (ctx_.seg_count > 0 && ctx_.seg_count <= 255) {
                    state_ = ParseState::PARSE_SEGMENTS;
                    ctx_.bytes_needed = ctx_.seg_count;
                    ctx_.data_offset = 0;
                } else if (ctx_.seg_count == 0) {
                    // 没有段，直接跳到下一个页面
                    state_ = ParseState::FIND_PAGE;
                    ctx_.bytes_needed = 4;
                    ctx_.data_offset = 0;
                } else {
                    ESP_LOGE(TAG, "无效的段数: %u", ctx_.seg_count);
                    state_ = ParseState::FIND_PAGE;
                    ctx_.bytes_needed = 4;
                    ctx_.data_offset = 0;
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
                return processed;  // 等待更多数据
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
                
                // 计算数据体总大小
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
                
                // 检查段数据是否已经部分读取
                if (ctx_.seg_remaining > 0) {
                    seg_len = ctx_.seg_remaining;
                } else {
                    ctx_.seg_remaining = seg_len;
                }
                
                // 检查缓冲区是否足够
                if (ctx_.packet_len + seg_len > sizeof(ctx_.packet_buf)) {
                    ESP_LOGE(TAG, "包缓冲区溢出: %zu + %u > %zu", ctx_.packet_len, seg_len, sizeof(ctx_.packet_buf));
                    state_ = ParseState::FIND_PAGE;
                    ctx_.packet_len = 0;
                    ctx_.packet_continued = false;
                    ctx_.seg_remaining = 0;
                    ctx_.bytes_needed = 4;
                    return processed;
                }
                
                // 复制数据
                size_t to_copy = std::min(size - processed, (size_t)seg_len);
                memcpy(ctx_.packet_buf + ctx_.packet_len, data + processed, to_copy);
                
                processed += to_copy;
                ctx_.packet_len += to_copy;
                ctx_.body_offset += to_copy;
                ctx_.seg_remaining -= to_copy;
                
                // 检查段是否完整
                if (ctx_.seg_remaining > 0) {
                    // 段不完整，等待更多数据
                    return processed;
                }
                
                // 段完整
                bool seg_continued = (ctx_.seg_table[ctx_.seg_index] == 255);
                
                if (!seg_continued) {
                    // 包结束
                    if (ctx_.packet_len) {
                        if (!opus_info_.head_seen) {
                            if (ctx_.packet_len >=8 && memcmp(ctx_.packet_buf, "OpusHead", 8) == 0) {
                                opus_info_.head_seen = true;
                                if (ctx_.packet_len >= 19) {
                                    opus_info_.sample_rate = ctx_.packet_buf[12] | 
                                                            (ctx_.packet_buf[13] << 8) | 
                                                            (ctx_.packet_buf[14] << 16) | 
                                                            (ctx_.packet_buf[15] << 24);
                                    ESP_LOGI(TAG, "OpusHead found, sample_rate=%d", opus_info_.sample_rate);
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
                                ESP_LOGI(TAG, "OpusTags found.");
                                ctx_.packet_len = 0;
                                ctx_.packet_continued = false;
                                ctx_.seg_index++;
                                ctx_.seg_remaining = 0;
                                continue;  
                            }
                        }
                        if (opus_info_.head_seen && opus_info_.tags_seen) {
                            if (on_demuxer_finished_) {
                                on_demuxer_finished_(ctx_.packet_buf, opus_info_.sample_rate, ctx_.packet_len);
                            }
                        } else {
                            ESP_LOGW(TAG, "当前Ogg容器未解析到OpusHead/OpusTags，丢弃");
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
                // 检查是否所有数据体都已读取
                if (ctx_.body_offset < ctx_.body_size) {
                    ESP_LOGW(TAG, "数据体不完整: %zu/%zu", 
                            ctx_.body_offset, ctx_.body_size);
                }
                
                // 如果包跨页，保持packet_len和packet_continued
                if (!ctx_.packet_continued) {
                    ctx_.packet_len = 0;
                }
                
                // 进入下一页面
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

