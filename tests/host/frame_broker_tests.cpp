#include "camera_hub/FrameBroker.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <utility>

using namespace camera_hub;

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}

int main() {
    std::array<std::uint8_t, FrameBroker::slotCount * 16> storage{};
    std::array<std::uint8_t, 6> jpeg{0xff, 0xd8, 1, 2, 0xff, 0xd9};
    FrameBroker broker(storage.data(), storage.size(), 16);
    require(broker.valid(), "external storage validated");
    require(!FrameBroker(nullptr, 0, 16).valid(), "null storage rejected");
    require(!FrameBroker(storage.data(), storage.size(), 17).valid(), "small storage rejected");
    require(!broker.tryTake(Output::Usb), "empty queue");
    for (const auto output : {Output::Usb, Output::Http, Output::Rtsp}) {
        broker.setEnabled(output, true);
    }
    require(broker.publish(jpeg.data(), jpeg.size(), 100) == PublishResult::Published,
            "publish JPEG");
    auto usb = broker.tryTake(Output::Usb);
    auto http = broker.tryTake(Output::Http);
    auto rtsp = broker.tryTake(Output::Rtsp);
    require(usb && http && rtsp, "three outputs receive one frame");
    require(usb.data() == http.data() && usb.data() == rtsp.data(), "one shared copy");
    jpeg[2] = 99;
    require(usb.data()[2] == 1, "capture buffer can be reused");
    require(usb.size() == jpeg.size() && usb.timestampUs() == 100, "frame metadata");
    usb.reset();
    http.reset();
    require(rtsp.data()[2] == 1, "one reader release does not invalidate another");
    auto moved = std::move(rtsp);
    require(!rtsp && moved, "move transfers ownership");
        auto* alias = &moved;
        moved = std::move(*alias);
    require(static_cast<bool>(moved), "self move preserves ownership");
    moved.reset();
    moved.reset();
    require(broker.stats().occupiedSlots == 0, "last reader frees slot");

        const std::array<std::uint8_t, 6> h264{0, 0, 0, 1, 0x65, 0x88};
        require(broker.publish(h264.data(), h264.size(), 101, VideoCodec::H264) == PublishResult::Published,
            "Annex B H264 accepted explicitly");
        auto encoded = broker.tryTake(Output::Rtsp);
        require(encoded && encoded.codec() == VideoCodec::H264, "lease preserves H264 codec");
        auto encodedMoved = std::move(encoded);
        require(encodedMoved.codec() == VideoCodec::H264, "move preserves codec");
        encodedMoved.reset();
        broker.tryTake(Output::Http).reset();
        broker.tryTake(Output::Usb).reset();
        require(broker.publish(h264.data(), h264.size(), 102) == PublishResult::InvalidFrame,
            "H264 cannot be mislabeled JPEG");
        require(broker.publish(jpeg.data(), jpeg.size(), 102, VideoCodec::H264) == PublishResult::InvalidFrame,
            "JPEG cannot be mislabeled H264");

    for (std::uint64_t timestamp = 1; timestamp <= 20; ++timestamp) {
        require(broker.publish(jpeg.data(), jpeg.size(), timestamp) == PublishResult::Published,
                "slow HTTP cannot block publish");
        auto fastUsb = broker.tryTake(Output::Usb);
        auto fastRtsp = broker.tryTake(Output::Rtsp);
        require(fastUsb.timestampUs() == timestamp && fastRtsp.timestampUs() == timestamp,
                "fast readers keep current frames");
    }
    require(broker.stats().dropped[1] == 18, "slow reader drops oldest");
    auto oldHttp = broker.tryTake(Output::Http);
    auto newHttp = broker.tryTake(Output::Http);
    require(oldHttp.timestampUs() == 19 && newHttp.timestampUs() == 20, "bounded HTTP queue");
    oldHttp.reset();
    newHttp.reset();

    {
        std::array<std::uint8_t, FrameBroker::slotCount * 16> latestStorage{};
        FrameBroker latestBroker(latestStorage.data(), latestStorage.size(), 16);
        latestBroker.setEnabled(Output::Rtsp, true);
        latestBroker.setEnabled(Output::Http, true);
        require(latestBroker.publish(jpeg.data(), jpeg.size(), 1) == PublishResult::Published, "first latest frame");
        require(latestBroker.publish(jpeg.data(), jpeg.size(), 2) == PublishResult::Published, "second latest frame");
        auto latest = latestBroker.tryTakeLatest(Output::Rtsp);
        require(latest.timestampUs() == 2, "RTP selects newest queued frame");
        require(latestBroker.stats().dropped[2] == 1, "stale RTP reference counted and released");
        auto oldestHttp = latestBroker.tryTake(Output::Http);
        require(oldestHttp.timestampUs() == 1, "latest RTP does not alter HTTP FIFO");
        latestBroker.setEnabled(Output::Http, false);
        require(latest.data()[0] == 0xff, "latest lease survives clearing another output");
        oldestHttp.reset();
        latest.reset();
        require(latestBroker.stats().occupiedSlots == 0, "latest selection releases all slots");
        require(!latestBroker.tryTakeLatest(Output::Count), "invalid latest output rejected");
    }

    std::array<FrameLease, FrameBroker::slotCount> held{};
    broker.setEnabled(Output::Http, false);
    broker.setEnabled(Output::Rtsp, false);
    for (auto& lease : held) {
        require(broker.publish(jpeg.data(), jpeg.size(), 200) == PublishResult::Published,
                "allocate pool slot");
        lease = broker.tryTake(Output::Usb);
    }
    require(broker.publish(jpeg.data(), jpeg.size(), 201) == PublishResult::PoolExhausted,
            "pool exhaustion bounded");
    held[0].reset();
    require(broker.publish(jpeg.data(), jpeg.size(), 202) == PublishResult::Published,
            "pool recovers on release");
    broker.setEnabled(Output::Usb, false);
    for (auto& lease : held) {
        lease.reset();
    }
    require(broker.stats().occupiedSlots == 0, "disable clears queued references");
    require(!broker.tryTake(Output::Count), "invalid output rejected");
    broker.setEnabled(Output::Count, true);
    require(broker.publish(nullptr, 0, 0) == PublishResult::InvalidFrame, "null JPEG rejected");
    require(broker.publish(jpeg.data(), 3, 0) == PublishResult::InvalidFrame, "short JPEG rejected");
        require(broker.publish(jpeg.data(), 1000000, 0) == PublishResult::TooLarge,
            "reject declared oversized length before reading JPEG envelope");
    std::array<std::uint8_t, 20> oversized{};
    oversized[0] = 0xff;
    oversized[1] = 0xd8;
    oversized[18] = 0xff;
    oversized[19] = 0xd9;
    require(broker.publish(oversized.data(), oversized.size(), 0) == PublishResult::TooLarge,
            "oversized frame rejected");
    require(broker.publish(jpeg.data(), jpeg.size(), 300) == PublishResult::Published,
            "publish without subscribers");
    require(broker.stats().occupiedSlots == 0, "no subscribers retain no frames");
    std::atomic<bool> finished{false};
    std::array<std::thread, FrameBroker::outputCount> consumers;
    for (std::size_t index = 0; index < consumers.size(); ++index) {
        const auto output = static_cast<Output>(index);
        broker.setEnabled(output, true);
        consumers[index] = std::thread([&, index, output] {
            std::uint64_t previousTimestamp = 0;
            while (!finished.load() || broker.stats().queued[index] != 0) {
                auto frame = output == Output::Rtsp ? broker.tryTakeLatest(output) : broker.tryTake(output);
                if (frame) {
                    require(frame.timestampUs() > previousTimestamp, "monotonic consumer timestamps");
                    require(frame.size() == jpeg.size() &&
                            frame.data()[2] == frame.timestampUs() % 256,
                            "concurrent readers keep immutable frame content");
                    previousTimestamp = frame.timestampUs();
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (std::uint64_t timestamp = 1; timestamp <= 10000; ++timestamp) {
        jpeg[2] = static_cast<std::uint8_t>(timestamp % 256);
        require(broker.publish(jpeg.data(), jpeg.size(), timestamp) == PublishResult::Published,
                "one producer and three consumers fit in bounded pool");
    }
    finished.store(true);
    for (auto& consumer : consumers) {
        consumer.join();
    }
    require(broker.stats().occupiedSlots == 0, "concurrent readers release all buffers");
    std::puts("PASS: frame ownership, fan-out, backpressure, bounds, recovery and concurrency");
}