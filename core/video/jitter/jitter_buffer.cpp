module;

#include <algorithm>
#include <cstring>
#include <chrono>
#include <mutex>
#include <vector>
#include <cstdint>

module video_streaming.jitter;
import video_streaming.rtp.packet;
import video_streaming.common.time;

namespace video_streaming {

JitterBuffer::JitterBuffer(size_t max_packets, std::chrono::milliseconds playout_delay)
    : m_max_packets(max_packets), m_playout_delay(playout_delay) {}

void JitterBuffer::push(const RtpPacket& packet) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Simple logic: just store by sequence number
    // Real implementation would handle wrap-around, timestamps, etc.
    m_buffer[packet.header.sequence_number] = packet;
    
    // Drop oldest if buffer full
    if (m_buffer.size() > m_max_packets) {
        m_buffer.erase(m_buffer.begin());
    }
}

bool JitterBuffer::pop(RtpPacket& packet) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_buffer.empty()) return false;
    
    auto it = m_buffer.begin();
    
    if (m_first_packet) {
        m_last_popped_seq = it->first - 1;
        m_first_packet = false;
    }
    
    // Check strict ordering (simplified)
    uint16_t expected = m_last_popped_seq + 1;
    
    if (it->first == expected) {
        packet = std::move(it->second);
        m_buffer.erase(it);
        m_last_popped_seq = expected;
        return true;
    }
    
    // Force pop if buffer is getting too full (latency control)
    if (m_buffer.size() > m_max_packets / 2) {
         packet = std::move(it->second);
         m_last_popped_seq = it->first; // Resync
         m_buffer.erase(it);
         return true;
    }
    
    return false;
}

std::vector<std::vector<uint8_t>> JitterBuffer::get_ready_packets() {
    std::vector<std::vector<uint8_t>> packets;
    RtpPacket pkt;
    while (pop(pkt)) {
        packets.push_back(pkt.payload);
    }
    return packets;
}

// For legacy/test support
void JitterBuffer::add_packet(const std::vector<uint8_t>& packet_data) {
    // Only for compatibility, not fully implemented for raw data
    // Assuming data contains enough to make a dummy packet for tests
}

void JitterBuffer::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.clear();
    m_first_packet = true;
}

JitterBuffer::Stats JitterBuffer::get_stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    Stats stats;
    stats.buffer_size = m_buffer.size();
    return stats;
}

} // namespace video_streaming
