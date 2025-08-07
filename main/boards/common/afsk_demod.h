#pragma once

#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <optional>
#include <cmath>
#include "wifi_configuration_ap.h"
#include "application.h"

// Audio signal processing constants for WiFi configuration via audio
const size_t kAudioSampleRate = 6400;
const size_t kMarkFrequency = 1800;
const size_t kSpaceFrequency = 1500;
const size_t kBitRate = 100;
const size_t kWindowSize = 64;

namespace audio_wifi_config
{
    // Main function to receive WiFi credentials through audio signal
    void ReceiveWifiCredentialsFromAudio(Application *app, WifiConfigurationAp *wifi_ap, Display *display, 
                                         size_t input_channels = 1);

    /**
     * Goertzel algorithm implementation for single frequency detection
     * Used to detect specific audio frequencies in the AFSK demodulation process
     */
    class FrequencyDetector
    {
    private:
        float frequency_;              // Target frequency (normalized, i.e., f / fs)
        size_t window_size_;           // Window size for analysis
        float frequency_bin_;          // Frequency bin
        float angular_frequency_;      // Angular frequency
        float cos_coefficient_;        // cos(w)
        float sin_coefficient_;        // sin(w)
        float filter_coefficient_;     // 2 * cos(w)
        std::deque<float> state_buffer_;  // Circular buffer for storing S[-1] and S[-2]

    public:
        /**
         * Constructor
         * @param frequency Normalized frequency (f / fs)
         * @param window_size Window size for analysis
         */
        FrequencyDetector(float frequency, size_t window_size);

        /**
         * Reset the detector state
         */
        void Reset();

        /**
         * Process one audio sample
         * @param sample Input audio sample
         */
        void ProcessSample(float sample);

        /**
         * Calculate current amplitude
         * @return Amplitude value
         */
        float GetAmplitude() const;
    };

    /**
     * Audio signal processor for Mark/Space frequency pair detection
     * Processes audio signals to extract digital data using AFSK demodulation
     */
    class AudioSignalProcessor
    {
    private:
        std::deque<float> input_buffer_;             // Input sample buffer
        size_t input_buffer_size_;                   // Input buffer size = window size
        size_t output_sample_count_;                 // Output sample counter
        size_t samples_per_bit_;                     // Samples per bit threshold
        std::unique_ptr<FrequencyDetector> mark_detector_;   // Mark frequency detector
        std::unique_ptr<FrequencyDetector> space_detector_;  // Space frequency detector

    public:
        /**
         * Constructor
         * @param sample_rate Audio sampling rate
         * @param mark_frequency Mark frequency for digital '1'
         * @param space_frequency Space frequency for digital '0'
         * @param bit_rate Data transmission bit rate
         * @param window_size Analysis window size
         */
        AudioSignalProcessor(size_t sample_rate, size_t mark_frequency, size_t space_frequency,
                           size_t bit_rate, size_t window_size);

        /**
         * Process input audio samples
         * @param samples Input audio sample vector
         * @return Vector of Mark probability values (0.0 to 1.0)
         */
        std::vector<float> ProcessAudioSamples(const std::vector<float> &samples);
    };

    /**
     * Data reception state machine states
     */
    enum class DataReceptionState
    {
        kInactive,  // Waiting for start signal
        kWaiting,   // Detected potential start, waiting for confirmation
        kReceiving  // Actively receiving data
    };

    /**
     * Data buffer for managing audio-to-digital data conversion
     * Handles the complete process from audio signal to decoded text data
     */
    class AudioDataBuffer
    {
    private:
        DataReceptionState current_state_;       // Current reception state
        std::deque<uint8_t> identifier_buffer_;  // Buffer for start/end identifier detection
        size_t identifier_buffer_size_;          // Identifier buffer size
        std::vector<uint8_t> bit_buffer_;        // Buffer for storing bit stream
        size_t max_bit_buffer_size_;             // Maximum bit buffer size
        const std::vector<uint8_t> start_of_transmission_;  // Start-of-transmission identifier
        const std::vector<uint8_t> end_of_transmission_;    // End-of-transmission identifier
        bool enable_checksum_validation_;       // Whether to validate checksum

    public:
        std::optional<std::string> decoded_text; // Successfully decoded text data

        /**
         * Default constructor using predefined start and end identifiers
         */
        AudioDataBuffer();

        /**
         * Constructor with custom parameters
         * @param max_byte_size Expected maximum data size in bytes
         * @param start_identifier Start-of-transmission identifier
         * @param end_identifier End-of-transmission identifier
         * @param enable_checksum Whether to enable checksum validation
         */
        AudioDataBuffer(size_t max_byte_size, const std::vector<uint8_t> &start_identifier,
                      const std::vector<uint8_t> &end_identifier, bool enable_checksum = false);

        /**
         * Process probability data and attempt to decode
         * @param probabilities Vector of Mark probabilities
         * @param threshold Decision threshold for bit detection
         * @return true if complete data was successfully received and decoded
         */
        bool ProcessProbabilityData(const std::vector<float> &probabilities, float threshold = 0.5f);

        /**
         * Calculate checksum for ASCII text
         * @param text Input text string
         * @return Checksum value (0-255)
         */
        static uint8_t CalculateChecksum(const std::string &text);

    private:
        /**
         * Convert bit vector to byte vector
         * @param bits Input bit vector
         * @return Converted byte vector
         */
        std::vector<uint8_t> ConvertBitsToBytes(const std::vector<uint8_t> &bits) const;

        /**
         * Clear all buffers and reset state
         */
        void ClearBuffers();
    };

    // Default start and end transmission identifiers
    extern const std::vector<uint8_t> kDefaultStartTransmissionPattern;
    extern const std::vector<uint8_t> kDefaultEndTransmissionPattern;
}