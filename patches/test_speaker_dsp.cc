// Host test for speaker_dsp.h.  Build and run:
//   g++ -std=c++17 -O2 -Wall -Wextra -o /tmp/test_speaker_dsp patches/test_speaker_dsp.cc && /tmp/test_speaker_dsp
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "speaker_dsp.h"

static int failures = 0;
#define CHECK(cond, ...)                         \
    do {                                         \
        if (!(cond)) {                           \
            printf("FAIL: " __VA_ARGS__);        \
            printf("\n");                        \
            failures++;                          \
        }                                        \
    } while (0)

static const int kRate = 24000;

static std::vector<int16_t> Sine(double hz, double amp, int n) {
    std::vector<int16_t> v(n);
    for (int i = 0; i < n; i++) v[i] = (int16_t)(amp * 32767.0 * sin(2 * M_PI * hz * i / kRate));
    return v;
}

static double PeakOut(const std::vector<int32_t>& o, int skip) {
    double p = 0;
    for (size_t i = skip; i < o.size(); i++) p = fmax(p, fabs((double)o[i]) / 2147483647.0);
    return p;
}

static std::vector<int32_t> Run(SpeakerDsp& d, const std::vector<int16_t>& in, int volume) {
    std::vector<int32_t> out(in.size());
    d.Process(in.data(), out.data(), (int)in.size(), kRate, volume);
    return out;
}

int main() {
    const int n = kRate;  // one second
    // Quiet input, so the limiter stays out of the way and only the filter and gain show.
    const double amp = 0.02;

    // 1. High-pass: bass is cut hard, voice range passes.
    {
        SpeakerDsp a, b, c;
        double bass = PeakOut(Run(a, Sine(80, amp, n), 100), kRate / 2);
        double mid = PeakOut(Run(b, Sine(1000, amp, n), 100), kRate / 2);
        double db = 20 * log10(bass / mid);
        printf("80 Hz vs 1 kHz: %.1f dB\n", db);
        CHECK(db < -15.0, "80 Hz should be at least 15 dB below 1 kHz, got %.1f dB", db);
        double one_k_gain = 20 * log10(mid / (amp * SPK_MAX_AMP));
        printf("1 kHz gain: %.1f dB (makeup %.1f)\n", one_k_gain, (double)SPK_MAKEUP_DB);
        CHECK(fabs(one_k_gain - SPK_MAKEUP_DB) < 1.0, "1 kHz gain should be about the makeup gain");
        (void)c;
    }

    // 2. Loud input: output never exceeds volume * SPK_MAX_AMP (no hard clipping surprise).
    {
        SpeakerDsp d;
        double p = PeakOut(Run(d, Sine(500, 1.0, n), 100), 0);
        printf("loud sine peak at volume 100: %.4f (max %.4f)\n", p, (double)SPK_MAX_AMP);
        CHECK(p <= SPK_MAX_AMP * 1.0001, "peak %.4f above SPK_MAX_AMP", p);
        CHECK(p > SPK_MAX_AMP * 0.7, "peak %.4f suspiciously low", p);
    }

    // 3. Volume is linear and zero is silent.
    {
        SpeakerDsp a, b, z;
        double p100 = PeakOut(Run(a, Sine(500, 1.0, n), 100), kRate / 2);
        double p50 = PeakOut(Run(b, Sine(500, 1.0, n), 50), kRate / 2);
        CHECK(fabs(p50 / p100 - 0.5) < 0.02, "volume 50 should be half of 100, ratio %.3f", p50 / p100);
        CHECK(PeakOut(Run(z, Sine(500, 1.0, n), 0), 0) == 0.0, "volume 0 should be silent");
    }

    // 4. Limiter evens out levels: loud bursts are pulled down, quiet speech is lifted, and the
    //    peak never goes above the ceiling.
    {
        std::vector<int16_t> burst(n);
        for (int i = 0; i < n; i++) {
            double amp_i = i < n / 2 ? 0.1 : 0.9;  // quiet half, then loud half
            burst[i] = (int16_t)(amp_i * 32767.0 * sin(2 * M_PI * 500.0 * i / kRate));
        }
        SpeakerDsp d;
        auto out = Run(d, burst, 100);
        auto rms_of = [&](int from, int to) {
            double acc = 0;
            for (int i = from; i < to; i++) {
                double x = (double)out[i] / 2147483647.0;
                acc += x * x;
            }
            return sqrt(acc / (to - from));
        };
        double quiet = rms_of(4000, n / 2 - 100);
        double loud = rms_of(n / 2 + 6000, n - 100);
        double in_ratio = 0.9 / 0.1;
        double out_ratio = loud / quiet;
        printf("loud/quiet ratio: in %.1f, out %.1f; limiter gain at end %.2f\n", in_ratio, out_ratio,
               d.limiter_gain());
        CHECK(out_ratio < in_ratio * 0.7, "limiter should shrink the loud/quiet ratio");
        CHECK(PeakOut(out, 0) <= SPK_MAX_AMP * 1.0001, "burst peak above ceiling");
        CHECK(d.limiter_gain() < 0.9f, "limiter should be working on the loud half");
    }

    // 5. Chunked processing gives the same result as one big call (filter state carries over).
    {
        auto in = Sine(300, 0.3, n);
        SpeakerDsp whole, chunked;
        auto a = Run(whole, in, 80);
        std::vector<int32_t> b(in.size());
        for (int i = 0; i < n; i += 480) {
            chunked.Process(in.data() + i, b.data() + i, 480, kRate, 80);
        }
        long diff = 0;
        for (int i = 0; i < n; i++) diff += labs((long)a[i] - (long)b[i]);
        CHECK(diff == 0, "chunked output differs from one-shot output");
    }

    // 6. Silence in, silence out. Filter stays stable on full-scale noise.
    {
        SpeakerDsp d;
        std::vector<int16_t> zeros(n, 0);
        CHECK(PeakOut(Run(d, zeros, 100), 0) == 0.0, "silence should stay silent");
        std::vector<int16_t> noise(n);
        srand(2);
        for (auto& s : noise) s = (int16_t)((rand() % 65536) - 32768);
        auto out = Run(d, noise, 100);
        bool ok = true;
        for (auto v : out) if (v > (int32_t)(SPK_MAX_AMP * 2147483647.0f * 1.001f) || v < -(int32_t)(SPK_MAX_AMP * 2147483647.0f * 1.001f)) ok = false;
        CHECK(ok, "noise pushed output above the ceiling");
    }

    // 7. Sample rate change reconfigures and does not blow up.
    {
        SpeakerDsp d;
        auto in = Sine(500, 0.5, 4800);
        std::vector<int32_t> out(in.size());
        d.Process(in.data(), out.data(), 4800, 16000, 100);
        d.Process(in.data(), out.data(), 4800, 24000, 100);
        CHECK(PeakOut(out, 0) <= SPK_MAX_AMP * 1.0001, "rate change broke the ceiling");
    }

    if (failures == 0) printf("ALL OK\n");
    return failures == 0 ? 0 : 1;
}
