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
        std::vector<uint32_t> speech_ids;    // per sentence: its text-to-speech id, or 0
        bool trimmed = false;                // leading sentences were dropped to fit the cap

        // The assistant sentences joined for display. CJK sentences are joined as is;
        // a space is inserted between two ASCII words so English stays readable.
        std::string AssistantText() const;
        // Where each sentence ends in AssistantText(), counted in characters (UTF-8 code
        // points, the unit LVGL labels index text by). A joining space belongs to the
        // sentence after it.
        std::vector<size_t> SentenceEnds() const;
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
    // `speech_id` is non-zero when the sentence is spoken (see SpeechFollower).
    Update AddAssistant(const std::string& text, uint32_t speech_id = 0);
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

// Number of characters (UTF-8 code points) in `text`.
size_t Utf8Length(const std::string& text);

// Works out how much of the newest answer has been spoken, so the display can light the
// text as it is read out.
//
// Playback is reported per audio frame as (sentence speech id, position of the frame in
// that sentence). Speech ids grow with every sentence. The audio length of a sentence is
// not known while it plays, so the position is turned into characters with an estimate of
// how long each character takes, scaled by how long the earlier sentences really took.
// Sentences without a speech id count as already spoken.
class SpeechFollower {
public:
    // A new answer starts. The learned speaking rate is kept.
    void Reset();
    // false: light a whole sentence as soon as its audio starts.
    void set_word_level(bool word_level) { word_level_ = word_level; }
    // A frame of `speech_id` started playing at `position_ms`. Returns true when the lit
    // part may have changed.
    bool OnProgress(uint32_t speech_id, uint32_t position_ms, const ReadingTranscript::Turn& turn);
    // No frame has played for a while. `speaking` tells whether more audio may still come.
    // Unless the answer was interrupted, the sentence that was playing has finished, and
    // when nothing more is coming the whole answer counts as spoken.
    bool OnPlaybackPaused(bool speaking, const ReadingTranscript::Turn& turn);
    // The answer was cut off: what has not been played stays unlit.
    void OnInterrupted() { interrupted_ = true; }
    // Characters of turn.AssistantText() that have been spoken.
    size_t LitChars(const ReadingTranscript::Turn& turn) const;
    float rate_scale() const { return scale_; }

    // Estimated speaking time of `text` before calibration.
    static uint32_t EstimateMs(const std::string& text);

private:
    void Calibrate(const ReadingTranscript::Turn& turn, uint32_t speech_id, uint32_t actual_ms);

    uint32_t current_id_ = 0;  // sentence whose audio is playing (or played last)
    uint32_t position_ms_ = 0;
    bool current_done_ = false;
    bool all_done_ = false;
    bool interrupted_ = false;
    bool word_level_ = true;
    float scale_ = 1.0f;  // measured / estimated speaking time
};

#endif  // READING_TRANSCRIPT_H
