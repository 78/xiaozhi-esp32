#include "reading_transcript.h"

#include <algorithm>

namespace {

// Audio frame length assumed when a sentence's last frame is reported.
constexpr uint32_t kFrameMs = 60;
// Sentences shorter than this say little about the speaking rate.
constexpr uint32_t kMinCalibrationMs = 600;

bool IsAsciiWordByte(char c) {
    const unsigned char byte = static_cast<unsigned char>(c);
    return byte > 0x20 && byte < 0x7F;
}

// Whether a joining space goes between `text` (so far) and `sentence`.
bool NeedsSpace(const std::string& text, const std::string& sentence) {
    return !text.empty() && !sentence.empty() && IsAsciiWordByte(text.back()) &&
           IsAsciiWordByte(sentence.front());
}

}  // namespace

size_t Utf8Length(const std::string& text) {
    size_t length = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++length;
        }
    }
    return length;
}

std::string ReadingTranscript::Turn::AssistantText() const {
    std::string text;
    for (const auto& sentence : sentences) {
        if (NeedsSpace(text, sentence)) {
            text += ' ';
        }
        text += sentence;
    }
    return text;
}

std::vector<size_t> ReadingTranscript::Turn::SentenceEnds() const {
    std::vector<size_t> ends;
    ends.reserve(sentences.size());
    std::string text;
    size_t chars = 0;
    for (const auto& sentence : sentences) {
        if (NeedsSpace(text, sentence)) {
            text += ' ';
            ++chars;
        }
        text += sentence;
        chars += Utf8Length(sentence);
        ends.push_back(chars);
    }
    return ends;
}

size_t ReadingTranscript::Turn::Bytes() const {
    size_t bytes = user.size();
    for (const auto& sentence : sentences) {
        bytes += sentence.size();
    }
    return bytes;
}

ReadingTranscript::ReadingTranscript(size_t max_turns, size_t max_bytes)
    : max_turns_(std::max<size_t>(max_turns, 1)), max_bytes_(max_bytes) {}

ReadingTranscript::Turn& ReadingTranscript::StartTurn() {
    Turn turn;
    turn.id = next_id_++;
    if (next_id_ == 0) {
        next_id_ = 1;  // 0 means "no turn" in Update
    }
    turns_.push_back(std::move(turn));
    return turns_.back();
}

ReadingTranscript::Update ReadingTranscript::AddUser(const std::string& text) {
    Update update;
    if (text.empty()) {
        return update;
    }
    Turn& turn = StartTurn();
    turn.user = text;
    update.turn_id = turn.id;
    update.new_turn = true;
    EnforceCap(update);
    return update;
}

ReadingTranscript::Update ReadingTranscript::AddAssistant(const std::string& text,
                                                          uint32_t speech_id) {
    Update update;
    if (text.empty()) {
        return update;
    }
    if (turns_.empty()) {
        StartTurn();
        update.new_turn = true;
    }
    Turn& turn = turns_.back();
    turn.sentences.push_back(text);
    turn.speech_ids.push_back(speech_id);
    update.turn_id = turn.id;
    EnforceCap(update);
    return update;
}

ReadingTranscript::Update ReadingTranscript::RemoveAssistant(const std::string& text) {
    Update update;
    if (turns_.empty()) {
        return update;
    }
    Turn& turn = turns_.back();
    if (turn.sentences.empty() || turn.sentences.back() != text) {
        return update;
    }
    turn.sentences.pop_back();
    turn.speech_ids.pop_back();
    if (turn.user.empty() && turn.sentences.empty()) {
        update.dropped.push_back(turn.id);
        turns_.pop_back();
    } else {
        update.turn_id = turn.id;
    }
    return update;
}

void ReadingTranscript::Clear() { turns_.clear(); }

const ReadingTranscript::Turn* ReadingTranscript::Find(uint32_t id) const {
    for (const auto& turn : turns_) {
        if (turn.id == id) {
            return &turn;
        }
    }
    return nullptr;
}

size_t ReadingTranscript::TotalBytes() const {
    size_t bytes = 0;
    for (const auto& turn : turns_) {
        bytes += turn.Bytes();
    }
    return bytes;
}

void ReadingTranscript::EnforceCap(Update& update) {
    // Older turns go first; the newest turn is the one being read and always stays.
    while (turns_.size() > 1 && (turns_.size() > max_turns_ || TotalBytes() > max_bytes_)) {
        update.dropped.push_back(turns_.front().id);
        turns_.pop_front();
    }
    // A single answer longer than the cap loses its oldest sentences, never the newest.
    Turn& turn = turns_.back();
    while (turn.sentences.size() > 1 && turn.Bytes() > max_bytes_) {
        turn.sentences.erase(turn.sentences.begin());
        turn.speech_ids.erase(turn.speech_ids.begin());
        turn.trimmed = true;
    }
}

ReadingTranscript::ScrollStep ReadingTranscript::Step(int32_t current, int32_t max_scroll,
                                                      int32_t delta) {
    ScrollStep step;
    const int32_t upper = std::max<int32_t>(max_scroll, 0);
    const int32_t from = std::clamp<int32_t>(current, 0, upper);
    step.target = std::clamp<int32_t>(from + delta, 0, upper);
    step.moved = step.target != current;
    step.follow = step.target >= upper;
    return step;
}

void SpeechFollower::Reset() {
    current_id_ = 0;
    position_ms_ = 0;
    current_done_ = false;
    all_done_ = false;
    interrupted_ = false;
}

uint32_t SpeechFollower::EstimateMs(const std::string& text) {
    uint32_t ms = 0;
    for (size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        size_t length = 1;
        if (lead >= 0xF0) {
            length = 4;
        } else if (lead >= 0xE0) {
            length = 3;
        } else if (lead >= 0xC0) {
            length = 2;
        }
        if (length >= 3) {
            ms += 230;  // CJK and other wide scripts: about one syllable per character
        } else if (length == 2) {
            ms += 90;  // accented Latin, Cyrillic, ...
        } else if (lead == ' ') {
            ms += 20;
        } else if ((lead >= '0' && lead <= '9') || ((lead | 0x20) >= 'a' && (lead | 0x20) <= 'z')) {
            ms += 70;
        } else {
            ms += 120;  // punctuation: a short pause
        }
        i += length;
    }
    return ms;
}

void SpeechFollower::Calibrate(const ReadingTranscript::Turn& turn, uint32_t speech_id,
                               uint32_t actual_ms) {
    for (size_t i = 0; i < turn.speech_ids.size(); ++i) {
        if (turn.speech_ids[i] != speech_id) {
            continue;
        }
        const uint32_t estimate = EstimateMs(turn.sentences[i]);
        if (estimate < kMinCalibrationMs) {
            return;
        }
        const float ratio = std::clamp(static_cast<float>(actual_ms) / estimate, 0.4f, 2.5f);
        scale_ = 0.6f * scale_ + 0.4f * ratio;
        return;
    }
}

bool SpeechFollower::OnProgress(uint32_t speech_id, uint32_t position_ms,
                                const ReadingTranscript::Turn& turn) {
    if (speech_id == 0 || speech_id < current_id_) {
        return false;  // a late frame of an earlier sentence
    }
    if (speech_id != current_id_) {
        // The previous sentence has finished: learn how long it really took.
        if (current_id_ != 0 && !current_done_) {
            Calibrate(turn, current_id_, position_ms_ + kFrameMs);
        }
        current_id_ = speech_id;
        position_ms_ = position_ms;
        current_done_ = false;
        all_done_ = false;
        return true;
    }
    if (position_ms <= position_ms_) {
        return false;
    }
    position_ms_ = position_ms;
    return word_level_ && !current_done_;
}

bool SpeechFollower::OnPlaybackPaused(bool speaking, const ReadingTranscript::Turn& turn) {
    if (interrupted_) {
        return false;
    }
    bool changed = false;
    if (current_id_ != 0 && !current_done_) {
        Calibrate(turn, current_id_, position_ms_ + kFrameMs);
        current_done_ = true;
        changed = true;
    }
    if (!speaking && !all_done_) {
        all_done_ = true;
        changed = true;
    }
    return changed;
}

size_t SpeechFollower::LitChars(const ReadingTranscript::Turn& turn) const {
    const std::vector<size_t> ends = turn.SentenceEnds();
    if (ends.empty()) {
        return 0;
    }
    if (all_done_) {
        return ends.back();
    }
    size_t lit = 0;
    for (size_t i = 0; i < ends.size(); ++i) {
        const uint32_t id = turn.speech_ids[i];
        if (id == 0 || id < current_id_) {
            lit = ends[i];
            continue;
        }
        if (id != current_id_) {
            break;  // not played yet
        }
        if (current_done_ || !word_level_) {
            return ends[i];
        }
        const size_t start = i == 0 ? 0 : ends[i - 1];
        const size_t chars = ends[i] - start;
        const float total = EstimateMs(turn.sentences[i]) * scale_;
        size_t part = chars;
        if (total > 0) {
            part = static_cast<size_t>(chars * std::min(1.0f, position_ms_ / total));
        }
        // The first character lights as soon as the sentence starts.
        return start + std::clamp<size_t>(part, 1, chars);
    }
    return lit;
}
