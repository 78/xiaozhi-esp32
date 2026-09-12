import os
import shlex
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEMUXER = ROOT / "main" / "audio" / "demuxer"


class OggDemuxerTest(unittest.TestCase):
    def test_zero_length_lace_is_processed_without_more_input(self):
        driver = r"""
            #include <algorithm>
            #include <cassert>
            #include <cstdint>
            #include <cstring>
            #include <vector>

            #include "ogg_demuxer.h"

            static uint32_t Crc(const std::vector<uint8_t>& bytes) {
                uint32_t crc = 0;
                for (uint8_t byte : bytes) {
                    crc ^= static_cast<uint32_t>(byte) << 24;
                    for (int bit = 0; bit < 8; ++bit) {
                        crc = (crc & 0x80000000U) ?
                            (crc << 1) ^ 0x04c11db7U : crc << 1;
                    }
                }
                return crc;
            }

            static std::vector<uint8_t> Page(const std::vector<uint8_t>& laces,
                                             const std::vector<uint8_t>& body,
                                             uint8_t flags, uint32_t sequence) {
                std::vector<uint8_t> page(27, 0);
                std::memcpy(page.data(), "OggS", 4);
                page[5] = flags;
                page[14] = 1;
                page[18] = sequence & 0xff;
                page[19] = (sequence >> 8) & 0xff;
                page[26] = static_cast<uint8_t>(laces.size());
                page.insert(page.end(), laces.begin(), laces.end());
                page.insert(page.end(), body.begin(), body.end());
                const uint32_t crc = Crc(page);
                for (int i = 0; i < 4; ++i) {
                    page[22 + i] = (crc >> (8 * i)) & 0xff;
                }
                return page;
            }

            static std::vector<uint8_t> PaddedOpusPacket(size_t size) {
                size_t padding = 0;
                while (3 + padding + padding / 254 + 1 != size) {
                    ++padding;
                }
                std::vector<uint8_t> packet{0x03, 0x41};
                for (size_t remaining = padding; remaining >= 254; remaining -= 254) {
                    packet.push_back(255);
                }
                packet.push_back(padding % 254);
                packet.push_back(0);
                packet.resize(size, 0);
                return packet;
            }

            static std::vector<uint8_t> Stream(const std::vector<uint8_t>& packet,
                                               bool zero_on_next_page = false) {
                std::vector<uint8_t> head(19, 0);
                std::memcpy(head.data(), "OpusHead", 8);
                head[8] = 1;
                head[9] = 1;
                head[12] = 0x80;
                head[13] = 0xbb;
                std::vector<uint8_t> tags(16, 0);
                std::memcpy(tags.data(), "OpusTags", 8);

                auto stream = Page({19}, head, 2, 0);
                auto tags_page = Page({16}, tags, 0, 1);
                stream.insert(stream.end(), tags_page.begin(), tags_page.end());

                std::vector<uint8_t> laces(packet.size() / 255, 255);
                if (packet.size() % 255) {
                    laces.push_back(packet.size() % 255);
                } else if (!zero_on_next_page) {
                    laces.push_back(0);
                }
                auto audio_page = Page(laces, packet, zero_on_next_page ? 0 : 4, 2);
                stream.insert(stream.end(), audio_page.begin(), audio_page.end());
                if (zero_on_next_page) {
                    auto final_page = Page({0}, {}, 5, 3);
                    stream.insert(stream.end(), final_page.begin(), final_page.end());
                }
                return stream;
            }

            static void Check(const std::vector<uint8_t>& stream,
                              const std::vector<uint8_t>& expected, size_t split) {
                OggDemuxer demuxer;
                std::vector<std::vector<uint8_t>> packets;
                demuxer.OnPacket([&](const uint8_t* data, int rate, int duration, size_t len) {
                    assert(rate == 48000);
                    assert(duration == 10);
                    packets.emplace_back(data, data + len);
                });
                assert(demuxer.Process(stream.data(), split) == split);
                assert(demuxer.Process(stream.data() + split, stream.size() - split) ==
                       stream.size() - split);
                assert(!demuxer.HasError());
                assert(demuxer.Finish());
                assert(packets == std::vector<std::vector<uint8_t>>{expected});
            }

            int main() {
                for (size_t size : {254U, 255U, 256U, 510U}) {
                    const auto packet = PaddedOpusPacket(size);
                    const auto stream = Stream(packet);
                    for (size_t split = 0; split <= stream.size(); ++split) {
                        Check(stream, packet, split);
                    }
                }

                const auto continued_packet = PaddedOpusPacket(255);
                const auto continued_stream = Stream(continued_packet, true);
                for (size_t split = 0; split <= continued_stream.size(); ++split) {
                    Check(continued_stream, continued_packet, split);
                }

                auto truncated = Stream(PaddedOpusPacket(254));
                truncated.pop_back();
                OggDemuxer demuxer;
                size_t packet_count = 0;
                demuxer.OnPacket([&](const uint8_t*, int, int, size_t) { ++packet_count; });
                assert(demuxer.Process(truncated.data(), truncated.size()) == truncated.size());
                assert(packet_count == 0);
                assert(!demuxer.HasError());
                assert(!demuxer.Finish());
            }
        """

        with tempfile.TemporaryDirectory() as directory:
            build_dir = Path(directory)
            (build_dir / "esp_log.h").write_text(
                "#define ESP_LOGE(...) ((void)0)\n"
                "#define ESP_LOGW(...) ((void)0)\n"
                "#define ESP_LOGD(...) ((void)0)\n",
                encoding="utf-8",
            )
            source = build_dir / "ogg_demuxer_test.cc"
            source.write_text(textwrap.dedent(driver), encoding="utf-8")
            executable = build_dir / "ogg_demuxer_test"
            command = shlex.split(os.environ.get("CXX", "c++")) + [
                "-std=c++20",
                f"-I{build_dir}",
                f"-I{DEMUXER}",
                str(source),
                str(DEMUXER / "ogg_demuxer.cc"),
                "-o",
                str(executable),
            ]
            subprocess.run(command, check=True, cwd=build_dir)
            subprocess.run([executable], check=True, cwd=build_dir)


if __name__ == "__main__":
    unittest.main()
