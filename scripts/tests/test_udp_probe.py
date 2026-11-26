"""Exercise the production UDP probe state machine on the host."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("c++"), "requires a host C++ compiler")
class UdpProbeTests(unittest.TestCase):
    def test_rounds_retries_deadlines_and_wire_format(self):
        source = r"""
#include "main/protocols/udp_probe.h"
#include <cassert>

int main() {
    const uint8_t ssrc[] = {0x11, 0x22, 0x33, 0x44};
    const uint8_t expected[] = {
        2, 0, 0, 16, 0x11, 0x22, 0x33, 0x44, 0, 0, 0, 0, 0, 0, 0, 1
    };
    using Action = UdpProbe::Action;
    UdpProbe probe(ssrc);
    auto unsolicited = probe.Packet();
    unsolicited[0] = 3;
    assert(!probe.Matches(unsolicited.data(), 0));
    assert(probe.NextActionAt() == 0);
    assert(probe.Poll(0) == Action::Send);
    assert(probe.NextActionAt() == 1000000);
    assert(probe.Attempts() == 1 && probe.ElapsedUs(12345) == 12345);
    assert(memcmp(probe.Packet().data(), expected, 16) == 0);
    const auto original = probe.Packet();
    auto pong = original;
    pong[0] = 3;
    assert(!probe.Matches(original.data(), 0)); // reflection
    for (int i = 1; i < 16; ++i) {
        auto modified = pong;
        modified[i] ^= 1;
        assert(!probe.Matches(modified.data(), 1));
    }
    assert(probe.Poll(999999) == Action::None);
    assert(probe.Poll(1000000) == Action::Send);
    assert(probe.NextActionAt() == 3000000);
    assert(probe.Attempts() == 2 && probe.ElapsedUs(1000123) == 1000123);
    assert(probe.Packet() == original);
    assert(probe.Poll(2999999) == Action::None);
    assert(probe.Poll(3000000) == Action::Send);
    assert(probe.NextActionAt() == 5000000);
    assert(probe.Packet() == original);
    assert(probe.Matches(pong.data(), 4999999)); // delayed reply to any attempt
    assert(!probe.Matches(pong.data(), 5000000));
    probe.Complete(4999999, 1500000);
    assert(!probe.Matches(pong.data(), 5000000)); // duplicate cannot renew health
    assert(probe.Poll(21499998) == Action::None);
    assert(probe.Poll(21499999) == Action::Send);
    assert(probe.Packet()[15] == 2);
    assert(probe.Attempts() == 1 && probe.ElapsedUs(21500999) == 1000);
    for (int i = 8; i < 12; ++i) assert(probe.Packet()[i] == 0); // no ACK
    assert(!probe.Matches(pong.data(), 21499999)); // previous round
    pong = probe.Packet();
    pong[0] = 3;
    assert(probe.Matches(pong.data(), 21499999));
    assert(probe.Poll(26499999) == Action::Failed);
    assert(probe.Failed());
    assert(!probe.Matches(pong.data(), 26499999));
    assert(probe.Poll(99999999) == Action::None); // report once, no new round

    UdpProbe fast(ssrc);
    assert(fast.Poll(100) == Action::Send);
    pong = fast.Packet();
    pong[0] = 3;
    assert(fast.Matches(pong.data(), 101)); // pending installed before send
    fast.Complete(101, 0);
    assert(fast.Poll(1000100) == Action::None); // cancelled retry
    assert(fast.Poll(15000101) == Action::Send);
    assert(fast.Packet()[15] == 2);

    UdpProbe stalled(ssrc);
    assert(stalled.Poll(0) == Action::Send);
    assert(stalled.Poll(6000000) == Action::Failed); // no deadline extension
    UdpProbe next_session(ssrc);
    assert(next_session.Poll(0) == Action::Send);
    assert(next_session.Packet()[15] == 1);
}
"""
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "probe_test.cc"
            binary = pathlib.Path(directory) / "probe_test"
            cpp.write_text(source)
            subprocess.run(
                ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT),
                 str(cpp), "-o", str(binary)], check=True, capture_output=True, text=True
            )
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_production_timer_and_pong_authentication(self):
        # Adapt platform APIs; compile the production UDP callback, close, send and timer paths.
        import shlex
        flags = None
        if shutil.which('pkg-config'):
            result = subprocess.run(['pkg-config', '--cflags', '--libs', 'openssl'],
                                    capture_output=True, text=True)
            if result.returncode == 0:
                flags = shlex.split(result.stdout)
        if flags is None:
            prefix = pathlib.Path('/opt/homebrew/opt/openssl@3')
            if not (prefix / 'include/openssl/hmac.h').exists():
                self.skipTest('requires OpenSSL development libraries')
            flags = ['-I' + str(prefix / 'include'), '-L' + str(prefix / 'lib'), '-lcrypto']
        text = (ROOT / 'main/protocols/mqtt_protocol.cc').read_text()
        opened_start = text.index('bool MqttProtocol::IsAudioChannelOpened() const {')
        sends_start = text.index('bool MqttProtocol::SendText(')
        sends_end = text.index('void MqttProtocol::CloseAudioChannel(', sends_start)
        callback_start = text.index('    udp->OnMessage([this, generation, probe]')
        callback_end = text.index('\n    if (auto connected = udp->Connect', callback_start)
        callback = text[callback_start:callback_end]
        close_end = text.index('bool MqttProtocol::OpenAudioChannel()', sends_end)
        method = (text[sends_start:close_end] + text[opened_start:] +
                  '\nvoid MqttProtocol::InstallReceiver(std::shared_ptr<Udp> udp, uint64_t generation, '
                  'std::shared_ptr<ProbeChannel> probe) {\n' + callback + '\n}\n')
        # Instrument lock acquisition only; keep the production callback/routing/body unchanged.
        method = method.replace('std::lock_guard<std::mutex>',
                                'std::lock_guard<ObservedMutex>')
        source = r'''
#include "main/protocols/udp_probe.h"
#include <cassert>
#include <memory>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <vector>
#include <functional>
#include <string>
#include <arpa/inet.h>
#include <cstdarg>
#include <cstdio>
#include <openssl/hmac.h>
#include <openssl/crypto.h>
int64_t clock_us = 0;
std::atomic<bool> clock_read{false};
int64_t esp_timer_get_time() {
    const auto now = clock_us;
    clock_read = true;
    return now;
}
// A barrier lets the controller hold the receiver at the real callback's first lock.
// No sleeps or mutex scheduling order are used to decide which side of the deadline wins.
thread_local bool in_delivery = false;
struct ObservedMutex {
    std::mutex mutex;
    std::mutex gate;
    std::condition_variable changed;
    bool pause_receiver = false;
    bool receiver_waiting = false;
    void lock() {
        if (in_delivery) {
            std::unique_lock<std::mutex> lock(gate);
            receiver_waiting = true;
            changed.notify_all();
            changed.wait(lock, [&] { return !pause_receiver; });
        }
        mutex.lock();
    }
    void unlock() { mutex.unlock(); }
    void PauseReceiver() {
        std::lock_guard<std::mutex> lock(gate);
        receiver_waiting = false;
        pause_receiver = true;
    }
    void WaitForReceiver() {
        std::unique_lock<std::mutex> lock(gate);
        assert(changed.wait_for(lock, std::chrono::seconds(5), [&] { return receiver_waiting; }));
    }
    void ReleaseReceiver() {
        std::lock_guard<std::mutex> lock(gate);
        pause_receiver = false;
        changed.notify_all();
    }
};
uint32_t esp_random() { return 0; }
constexpr int PSA_SUCCESS = 0;
constexpr int PSA_ALG_SHA_256 = 256;
int PSA_ALG_HMAC(int hash) { assert(hash == 256); return hash; }
int PSA_ALG_TRUNCATED_MAC(int alg, int bytes) { assert(bytes == 16); return alg; }
using Key = std::array<uint8_t, 16>;
int psa_mac_verify(const Key& key, int algorithm, const uint8_t* data, size_t size,
                   const uint8_t* tag, size_t tag_size) {
    assert(algorithm == 256 && size == 16 && tag_size == 16);
    unsigned length = 0;
    uint8_t digest[32];
    HMAC(EVP_sha256(), key.data(), key.size(), data, size, digest, &length);
    return CRYPTO_memcmp(digest, tag, 16) == 0 ? PSA_SUCCESS : -1;
}
bool mac_compute_fails = false;
int psa_mac_compute(const Key& key, int algorithm, const uint8_t* data, size_t size,
                    uint8_t* tag, size_t capacity, size_t* tag_size) {
    assert(algorithm == 256 && size == 16 && capacity == 16);
    if (mac_compute_fails) return -1;
    uint8_t digest[32];
    unsigned length = 0;
    HMAC(EVP_sha256(), key.data(), key.size(), data, size, digest, &length);
    memcpy(tag, digest, 16);
    *tag_size = 16;
    return PSA_SUCCESS;
}
struct Timer { bool active = false; int64_t due = -1; };
void esp_timer_stop(Timer* timer) { timer->active = false; }
void esp_timer_start_once(Timer* timer, int64_t delay) {
    assert(delay > 0);
    timer->active = true;
    timer->due = clock_us + delay;
}
namespace Lang { namespace Strings {
constexpr int SERVER_ERROR = 1;
constexpr int SERVER_TIMEOUT = 2;
} }
struct Udp {
    std::vector<std::string> sent;
    std::function<void(const std::string&)> on_send;
    std::function<void(const std::string&)> on_message;
    void OnMessage(std::function<void(const std::string&)> callback) {
        on_message = std::move(callback);
    }
    void Deliver(const std::string& data) {
        in_delivery = true;
        on_message(data);
        in_delivery = false;
    }
    int Send(const std::string& data) {
        sent.push_back(data);
        if (on_send) on_send(data);
        return data.size();
    }
};
#define ESP_LOGE(...)
#define ESP_LOGW(...)
#define TAG "MQTT"
std::vector<std::string> logs;
void log_info(const char*, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    logs.emplace_back(buffer);
}
#define ESP_LOGI(...) log_info(__VA_ARGS__)
struct AudioStreamPacket {
    int sample_rate = 0;
    int frame_duration = 0;
    uint32_t timestamp = 0;
    std::vector<uint8_t> payload;
};
struct Mqtt {
    std::vector<std::string> messages;
    bool Publish(const std::string&, const std::string& message) {
        messages.push_back(message);
        return true;
    }
};
struct MqttProtocol {
    struct ProbeChannel {
        explicit ProbeChannel(const uint8_t* ssrc) : probe(ssrc) {}
        UdpProbe probe;
        Key key{};
        std::atomic<bool> receiving_pong{false};
    };
    mutable ObservedMutex channel_mutex_;
    std::shared_ptr<ProbeChannel> probe_channel_;
    Timer timer;
    Timer* probe_timer_ = &timer;
    std::shared_ptr<Udp> udp_;
    int failures = 0;
    int closes = 0;
    std::unique_ptr<Mqtt> mqtt_ = std::make_unique<Mqtt>();
    std::string publish_topic_ = "out";
    std::string aes_nonce_ = std::string(16, 0);
    uint32_t local_sequence_ = 0;
    uint32_t remote_sequence_ = 0;
    int server_sample_rate_ = 24000;
    int server_frame_duration_ = 60;
    std::chrono::steady_clock::time_point last_incoming_time_;
    std::function<void(std::unique_ptr<AudioStreamPacket>)> on_incoming_audio_;
    uint64_t channel_generation_ = 0;
    std::atomic<bool> awaiting_hello_{false};
    std::string session_id_ = "test-session";
    std::function<void()> on_audio_channel_closed_ = [this] { ++closes; };
    bool SendText(const std::string&);
    bool SendAudio(std::unique_ptr<AudioStreamPacket>);
    bool CryptAesCtr(const uint8_t* input, size_t size, const uint8_t*, uint8_t* output) {
        memcpy(output, input, size);
        return true;
    }
    bool error_occurred_ = false;
    bool receive_timed_out = false;
    bool IsTimeout() const { return receive_timed_out; }
    bool IsAudioChannelOpened() const;
    void SetError(int message, const std::string& detail = "") {
        assert((message == Lang::Strings::SERVER_TIMEOUT && detail == "udp_probe_timeout") ||
               (message == Lang::Strings::SERVER_ERROR && detail == "udp_probe_auth_error"));
        error_occurred_ = true;
        ++failures;
    }
    void CloseAudioChannel(bool send_goodbye = true);
    void InstallReceiver(std::shared_ptr<Udp>, uint64_t, std::shared_ptr<ProbeChannel>);
    void ArmProbeTimer(int64_t due);
    void ProcessPending();
    void ReceivePong(const std::shared_ptr<ProbeChannel>&, const std::string&);
};
'''
        source += method
        source += r'''
int main() {
    const uint8_t ssrc[] = {0x11, 0x22, 0x33, 0x44};
    MqttProtocol protocol;
    auto fresh = [&]() {
        auto channel = std::make_shared<MqttProtocol::ProbeChannel>(ssrc);
        for (int i = 0; i < 16; ++i) channel->key[i] = i;
        protocol.probe_channel_ = channel;
        protocol.error_occurred_ = false;
        protocol.receive_timed_out = false;
        channel->probe.Poll(0);
        clock_us = 100;
        return channel;
    };
    auto channel = fresh();
    auto pong = channel->probe.Packet();
    pong[0] = 3;
    const uint8_t tag[] = {0x7c,0xd8,0x50,0x17,0x61,0x35,0x5f,0x10,
                           0x09,0x44,0x7b,0xad,0x2c,0x4f,0x8a,0x56};
    memcpy(pong.data()+16, tag, 16);
    auto receive = [&](auto ctx, auto bytes) {
        protocol.ReceivePong(ctx, std::string(reinterpret_cast<char*>(bytes.data()), bytes.size()));
    };
    receive(channel, pong);
    assert(logs.size() == 1 && logs.back() == "UDP RTT: 0.100 ms");
    assert(!channel->receiving_pong.load());
    assert(protocol.timer.due == 15000100);
    assert(channel->probe.Poll(1000000) == UdpProbe::Action::None);
    clock_us = 10000000;
    receive(channel, pong); // completed duplicate cannot extend next-round deadline
    assert(logs.size() == 1); // duplicate PONG does not produce another RTT sample
    assert(channel->probe.Poll(15000100) == UdpProbe::Action::Send);
    for (int i = 0; i < 32; ++i) {
        channel = fresh();
        auto altered = pong;
        altered[i] ^= 1;
        receive(channel, altered);
        assert(channel->probe.Poll(1000000) == UdpProbe::Action::Send);
    }
    assert(logs.size() == 1); // malformed or unauthenticated packets cannot pollute samples
    channel = fresh();
    clock_us = 5000000;
    receive(channel, pong);
    assert(channel->probe.Poll(clock_us) == UdpProbe::Action::Failed);
    assert(logs.size() == 1);
    auto old = fresh();
    channel = fresh(); // previous session callback cannot complete the new pending round
    receive(old, pong);
    assert(channel->probe.Poll(1000000) == UdpProbe::Action::Send);
    protocol.ReceivePong(nullptr, std::string(32, 0)); // legacy has no probe context
    channel = fresh();
    protocol.ReceivePong(channel, std::string(31, 0));
    assert(channel->probe.Poll(1000000) == UdpProbe::Action::Send);

    channel = fresh();
    clock_us = 4999999;
    clock_read = false;
    std::unique_lock<ObservedMutex> lock(protocol.channel_mutex_);
    std::thread receiver([&]() { receive(channel, pong); });
    while (!clock_read.load()) std::this_thread::yield();
    clock_us = 5000100;
    assert(channel->receiving_pong.load()); // timer handler must defer final timeout
    lock.unlock();
    receiver.join();
    assert(!channel->receiving_pong.load());
    assert(channel->probe.Poll(clock_us) == UdpProbe::Action::None);

    // Retransmitted requests share a sequence: elapsed time is not an unambiguous RTT.
    channel = fresh();
    assert(channel->probe.Poll(1000000) == UdpProbe::Action::Send);
    clock_us = 1100123;
    receive(channel, pong);
    assert(logs.back() == "UDP RTT: unknown after retransmission (attempts=2, elapsed=1100.123 ms)");
    const auto samples = logs.size();
    receive(channel, pong);
    assert(logs.size() == samples);

    // Real timer driver: exactly three identical sends and a single final failure.
    channel = fresh();
    channel->probe = UdpProbe(ssrc);
    protocol.udp_ = std::make_shared<Udp>();
    auto udp = protocol.udp_;
    clock_us = 0;
    protocol.ProcessPending();
    assert(udp->sent.size() == 1 && udp->sent[0].size() == 32);
    const uint8_t ping_tag[] = {0xda,0xef,0x4d,0xd6,0x3a,0x34,0xfe,0xb1,
                                0x52,0xd1,0x60,0x95,0x50,0x0e,0x95,0x06};
    assert(memcmp(udp->sent[0].data()+16, ping_tag, 16) == 0);
    assert(protocol.timer.due == 1000000);
    clock_us = 1000000;
    protocol.ProcessPending();
    assert(udp->sent.size() == 2 && protocol.timer.due == 3000000);
    clock_us = 3000000;
    protocol.ProcessPending();
    assert(udp->sent.size() == 3 && protocol.timer.due == 5000000);
    assert(udp->sent[0] == udp->sent[1] && udp->sent[1] == udp->sent[2]);
    clock_us = 5000000;
    protocol.ProcessPending();
    protocol.ProcessPending();
    assert(protocol.failures == 1 && !protocol.timer.active && udp->sent.size() == 3);
    assert(protocol.probe_channel_ == nullptr && protocol.udp_ == nullptr);

    // Fast PONG during Send must replace the retry timer with the next-period timer.
    channel = fresh();
    channel->probe = UdpProbe(ssrc);
    protocol.udp_ = std::make_shared<Udp>();
    udp = protocol.udp_;
    udp->on_send = [&](const std::string&) { receive(channel, pong); };
    clock_us = 0;
    protocol.ProcessPending();
    assert(udp->sent.size() == 1 && protocol.timer.due == 15000000);
    clock_us = 1000000;
    protocol.ProcessPending(); // stale/coalesced timer event cannot retransmit
    assert(udp->sent.size() == 1 && protocol.timer.due == 15000000);
    protocol.CloseAudioChannel();
    protocol.ProcessPending(); // stale event after close
    assert(!protocol.timer.active && udp->sent.size() == 1);

    // Retain the pre-existing receive-timeout predicate; PONGs cannot bypass it.
    channel = fresh();
    protocol.udp_ = std::make_shared<Udp>();
    assert(protocol.IsAudioChannelOpened());
    protocol.receive_timed_out = true;
    receive(channel, pong);
    assert(!protocol.IsAudioChannelOpened());
    assert(protocol.probe_channel_ == channel && protocol.udp_ != nullptr);
    assert(protocol.closes == 2); // only probe failure and the explicit close above
    protocol.receive_timed_out = false;
    assert(protocol.IsAudioChannelOpened());
    protocol.probe_channel_.reset(); // legacy still uses precisely the same predicate
    assert(protocol.IsAudioChannelOpened());
    protocol.receive_timed_out = true;
    assert(!protocol.IsAudioChannelOpened());

    // Initial signing failure reports/closes immediately, without queued work or retry.
    channel = fresh();
    channel->probe = UdpProbe(ssrc);
    protocol.udp_ = std::make_shared<Udp>();
    udp = protocol.udp_;
    mac_compute_fails = true;
    protocol.ProcessPending();
    protocol.ProcessPending();
    assert(protocol.error_occurred_ && protocol.failures == 2);
    assert(protocol.udp_ == nullptr && protocol.probe_channel_ == nullptr);
    assert(udp->sent.empty() && !protocol.timer.active);

    // Normal audio/control sends must not pump, rearm or advance a pending probe.
    mac_compute_fails = false;
    channel = fresh();
    channel->probe = UdpProbe(ssrc);
    protocol.timer.active = false;
    protocol.udp_ = std::make_shared<Udp>();
    udp = protocol.udp_;
    protocol.aes_nonce_[0] = 1;
    const auto messages = protocol.mqtt_->messages.size();
    assert(protocol.SendText("listen"));
    assert(protocol.mqtt_->messages.size() == messages + 1 && udp->sent.empty());
    auto audio = std::make_unique<AudioStreamPacket>();
    audio->payload = {1, 2, 3};
    assert(protocol.SendAudio(std::move(audio)));
    assert(udp->sent.size() == 1 && udp->sent[0][0] == 1);
    assert(udp->sent[0].size() == 19);
    assert(channel->probe.NextActionAt() == 0 && !protocol.timer.active);
    for (int i = 12; i < 16; ++i) assert(channel->probe.Packet()[i] == 0);

    // Install the exact OnMessage lambda extracted from production OpenAudioChannel,
    // and close with the production CloseAudioChannel, rather than bypassing routing.
    auto wire_session = [&]() {
        if (protocol.udp_) protocol.CloseAudioChannel(false);
        auto current = fresh();
        ++protocol.channel_generation_;
        protocol.udp_ = std::make_shared<Udp>();
        protocol.InstallReceiver(protocol.udp_, protocol.channel_generation_, current);
        return current;
    };
    auto wire_pong = [&](auto current) {
        auto bytes = current->probe.Packet();
        bytes[0] = 3;
        size_t size = 0;
        assert(psa_mac_compute(current->key, 256, bytes.data(), 16,
                               bytes.data() + 16, 16, &size) == 0);
        return std::string(reinterpret_cast<char*>(bytes.data()), bytes.size());
    };

    channel = wire_session();
    auto wire = protocol.udp_;
    auto reply = wire_pong(channel);
    clock_us = 4999999;
    clock_read = false;
    std::unique_lock<ObservedMutex> held(protocol.channel_mutex_);
    protocol.channel_mutex_.PauseReceiver();
    std::thread callback([wire, reply] { wire->Deliver(reply); });
    protocol.channel_mutex_.WaitForReceiver();
    // Fails on PR head 05830574: the callback's generation lock precedes timestamp/claim.
    assert(clock_read.load() && channel->receiving_pong.load());
    clock_us = 5000100;
    held.unlock();
    const auto failures_before = protocol.failures;
    protocol.ProcessPending(); // force timeout handler to run before receiver gets the lock
    assert(protocol.failures == failures_before && protocol.probe_channel_ == channel);
    protocol.channel_mutex_.ReleaseReceiver();
    callback.join();
    assert(protocol.timer.due == 19999999);
    assert(logs.back() == "UDP RTT: 4999.999 ms");
    assert(!channel->receiving_pong.load());
    const auto completed_samples = logs.size();
    clock_us = 5000200;
    wire->Deliver(reply); // completed duplicate remains ignored even after original deadline
    assert(logs.size() == completed_samples && protocol.timer.due == 19999999);

    // Actually late at callback entry is rejected, and cannot postpone failure indefinitely.
    channel = wire_session();
    wire = protocol.udp_;
    reply = wire_pong(channel);
    const auto late_samples = logs.size();
    clock_us = 5000000;
    wire->Deliver(reply);
    assert(logs.size() == late_samples && !channel->receiving_pong.load());
    protocol.ProcessPending();
    assert(protocol.failures == failures_before + 1 && protocol.udp_ == nullptr);

    // Close while the old callback is waiting: no timer/key/session resurrection.
    channel = wire_session();
    wire = protocol.udp_;
    reply = wire_pong(channel);
    held.lock();
    protocol.channel_mutex_.PauseReceiver();
    std::thread closing_callback([wire, reply] { wire->Deliver(reply); });
    protocol.channel_mutex_.WaitForReceiver();
    held.unlock();
    protocol.CloseAudioChannel(false);
    const auto close_samples = logs.size();
    protocol.channel_mutex_.ReleaseReceiver();
    closing_callback.join();
    assert(logs.size() == close_samples && protocol.probe_channel_ == nullptr);
    assert(!protocol.timer.active && !channel->receiving_pong.load());
    wire->Deliver(reply); // old callback entering only after close is also harmless
    assert(logs.size() == close_samples && !protocol.timer.active);

    // Reopen before releasing the old callback. A captured shared channel identity,
    // checked under the lock, must protect the new pending round and HMAC key.
    channel = wire_session();
    wire = protocol.udp_;
    reply = wire_pong(channel);
    held.lock();
    protocol.channel_mutex_.PauseReceiver();
    std::thread reopening_callback([wire, reply] { wire->Deliver(reply); });
    protocol.channel_mutex_.WaitForReceiver();
    held.unlock();
    protocol.CloseAudioChannel(false);
    auto reopened = wire_session();
    reopened->key[0] ^= 1;
    auto reopened_wire = protocol.udp_;
    protocol.ProcessPending();
    const auto reopen_samples = logs.size();
    const auto reopen_due = protocol.timer.due;
    protocol.channel_mutex_.ReleaseReceiver();
    reopening_callback.join();
    assert(protocol.probe_channel_ == reopened && !reopened->receiving_pong.load());
    assert(logs.size() == reopen_samples && protocol.timer.due == reopen_due);
    assert(reopened->probe.Matches(reinterpret_cast<const uint8_t*>(reply.data()), clock_us));
    wire->Deliver(reply); // old callback entering after reopen, not just one already waiting
    assert(logs.size() == reopen_samples && protocol.timer.due == reopen_due);
    reopened_wire->Deliver(reply); // old wire packet delivered to NEW callback fails new key's MAC
    assert(logs.size() == reopen_samples && protocol.timer.due == reopen_due);
    assert(!reopened->receiving_pong.load());
    reopened_wire->Deliver(wire_pong(reopened));
    assert(logs.size() == reopen_samples + 1 && protocol.timer.due == 15000100);

    // PONG routing moved ahead of the gate, but audio must still honor generation.
    int audio_received = 0;
    protocol.remote_sequence_ = 0;
    protocol.on_incoming_audio_ = [&](auto packet) {
        assert(packet->payload.size() == 3);
        ++audio_received;
    };
    auto audio_bytes = reply.substr(0, 16);
    audio_bytes[0] = 1;
    audio_bytes[2] = 0;
    audio_bytes[3] = 3;
    audio_bytes.append("abc");
    wire->Deliver(audio_bytes);
    assert(audio_received == 0 && protocol.remote_sequence_ == 0);
    reopened_wire->Deliver(audio_bytes);
    assert(audio_received == 1 && protocol.remote_sequence_ == 1);

}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / 'pong_test.cc'
            binary = pathlib.Path(directory) / 'pong_test'
            cpp.write_text(source)
            subprocess.run(['c++', '-std=c++17', '-pthread', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT), str(cpp), '-o', str(binary)] + flags,
                           check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
