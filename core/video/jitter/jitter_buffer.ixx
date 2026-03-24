module;

#include <chrono>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <mutex>

import video_streaming.rtp.packet;
import video_streaming.common.time;

export module video_streaming.jitter;

namespace video_streaming {

export class JitterBuffer {
public:
    struct Stats {
        size_t buffer_size = 0;
        std::chrono::milliseconds average_delay{0};
        uint64_t packets_lost = 0;
        uint64_t packets_late = 0;
    };

    explicit JitterBuffer(size_t max_packets = 50, std::chrono::milliseconds playout_delay = std::chrono::milliseconds(50));
    ~JitterBuffer() = default;

    // Add a packet to the buffer
    void push(const RtpPacket& packet);

    // Retrieve the next packet if it's ready
    bool pop(RtpPacket& packet);

    // Retrieve ready packets (for batch processing)
    // This is a compatibility helper for receiver loops that process multiple NALs
    std::vector<std::vector<uint8_t>> get_ready_packets();
    void add_packet(const std::vector<uint8_t>& packet_data); // Legacy/helper method if needed

    void reset();
    Stats get_stats() const;

private:
    size_t m_max_packets;
    std::chrono::milliseconds m_playout_delay;
    
    mutable std::mutex m_mutex;
    std::map<uint16_t, RtpPacket> m_buffer;
    uint16_t m_last_popped_seq = 0;
    bool m_first_packet = true;
};

} // namespace video_streaming