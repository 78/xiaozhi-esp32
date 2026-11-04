#ifndef READING_TRANSCRIPT_H
#define READING_TRANSCRIPT_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

// Conversation model behind the reading message style.
//
// The chat messages are grouped into turns: one user question plus the assistant
// sentences that answer it. Sentences are appended as they arrive instead of
// replacing each other, so the whole answer stays on screen. The model keeps only
// the most recent turns and a bounded amount of text, which matters on boards
// without PSRAM.
//
// It has no LVGL dependency so the policy can be unit-tested on a host.
class ReadingTranscript {
public:
    struct Turn {
        uint32_t id = 0;
        std::string user;                    // empty when the turn starts with the assistant
        std::vector<std::string> sentences;  // assistant sentences in arrival order
        bool trimmed = false;                // leading sentences were dropped to fit the cap

        // The assistant sentences joined for display. CJK sentences are joined as is;
        // a space is inserted between two ASCII words so English stays readable.
        std::string AssistantText() const;
        size_t Bytes() const;
    };

    struct Update {
        uint32_t turn_id = 0;           // the turn that changed (0 when nothing changed)
        bool new_turn = false;          // the turn was created by this update
        std::vector<uint32_t> dropped;  // older turns removed to stay within the cap
    };

    // Result of a manual scroll request, in content pixels from the top.
    struct ScrollStep {
        int32_t target = 0;
        bool moved = false;
        bool follow = false;  // the view is back at the newest content
    };

    ReadingTranscript(size_t max_turns, size_t max_bytes);

    // A recognised user utterance always starts a new turn.
    Update AddUser(const std::string& text);
    // Appends an assistant sentence to the current turn, starting one if needed.
    Update AddAssistant(const std::string& text);
    // Removes the newest assistant sentence if it equals `text` (used for transient
    // hints). The turn itself is dropped when nothing is left in it.
    Update RemoveAssistant(const std::string& text);
    void Clear();

    const std::deque<Turn>& turns() const { return turns_; }
    const Turn* Find(uint32_t id) const;
    size_t TotalBytes() const;

    // Moves the view by `delta` pixels (negative shows earlier content) within
    // [0, max_scroll]. Reaching the bottom means the view follows new content again.
    static ScrollStep Step(int32_t current, int32_t max_scroll, int32_t delta);

private:
    Turn& StartTurn();
    void EnforceCap(Update& update);

    size_t max_turns_;
    size_t max_bytes_;
    uint32_t next_id_ = 1;
    std::deque<Turn> turns_;
};

#endif  // READING_TRANSCRIPT_H
