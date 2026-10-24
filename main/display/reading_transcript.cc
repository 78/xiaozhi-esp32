#include "reading_transcript.h"

#include <algorithm>

namespace {

bool IsAsciiWordByte(char c) {
    const unsigned char byte = static_cast<unsigned char>(c);
    return byte > 0x20 && byte < 0x7F;
}

}  // namespace

std::string ReadingTranscript::Turn::AssistantText() const {
    std::string text;
    for (const auto& sentence : sentences) {
        if (!text.empty() && !sentence.empty() && IsAsciiWordByte(text.back()) &&
            IsAsciiWordByte(sentence.front())) {
            text += ' ';
        }
        text += sentence;
    }
    return text;
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

ReadingTranscript::Update ReadingTranscript::AddAssistant(const std::string& text) {
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
