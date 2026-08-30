#ifndef MORSE_CODE_H
#define MORSE_CODE_H

#include <string>
#include <vector>

// Encodes text into a sequence of fixed-width on/off ticks, one Morse "unit"
// each, using standard ITU timing (a dot is one "on" tick, a dash three;
// gaps are "off" ticks — one between symbols in a letter, three between
// letters, seven between words). Driving a timer at unit-length intervals
// and looking up ticks[index] is enough to blink the sequence out.
// Letters are matched case-insensitively; characters with no Morse
// representation are skipped.
std::vector<bool> EncodeMorseTicks(const std::string& text);

// Morse unit duration in milliseconds for a given words-per-minute rate.
inline int MorseUnitMs(int wpm) {
    return wpm > 0 ? 1200 / wpm : 1200 / 20;
}

#endif  // MORSE_CODE_H
