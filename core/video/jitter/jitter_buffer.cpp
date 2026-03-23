#include "jitter_buffer.hpp"
#include <algorithm>
#include <cstring>

JitterBuffer::JitterBuffer(size_t max_size, Milliseconds delay)
    : m_max_size(max_size), m_delay(delay) {}

void JitterBuffer::push(const RtpPacket& packet) {
    LockGuard lock(m_mutex);
    
    Timestamp now = TimeUtils::now();
    Timestamp playout_time = now + m_delay.count();
    
    SequenceNumber seq = packet.header.sequence_number;
    
    // Check if buffer is full
    if (m_buffer.size() >= m_max_size) {
        // Remove oldest packet
        if (!m_buffer.empty()) {
            m_buffer.erase(m_buffer.begin());
            m_dropped_packets++;
        }
    }
    
    // Insert packet
    m_buffer.emplace(seq, JitterPacket(packet, now, playout_time));
    
    // Calculate jitter for adaptive delay
    calculate_jitter();
    
    // Clean up old packets
    cleanup_old_packets();
}

bool JitterBuffer::pop(RtpPacket& packet) {
    LockGuard lock(m_mutex);
    
    Timestamp now = TimeUtils::now();
    
    // Find ready packets
    for (auto it = m_buffer.begin(); it != m_buffer.end(); ++it) {
        if (is_packet_ready(it->second)) {
            packet = it->second.packet;
            m_buffer.erase(it);
            
            // Update expected sequence
            update_expected_sequence(packet.header.sequence_number);
            
            // Adjust delay adaptively
            adjust_delay();
            
            return true;
        }
    }
    
    return false;
}

size_t JitterBuffer::size() const {
    LockGuard lock(m_mutex);
    return m_buffer.size();
}

bool JitterBuffer::empty() const {
    LockGuard lock(m_mutex);
    return m_buffer.empty();
}

void JitterBuffer::set_delay(Milliseconds delay) {
    LockGuard lock(m_mutex);
    m_delay = std::max(MIN_DELAY, std::min(delay, MAX_DELAY));
}

void JitterBuffer::set_max_size(size_t max_size) {
    LockGuard lock(m_mutex);
    m_max_size = max_size;
    
    // Trim buffer if necessary
    while (m_buffer.size() > m_max_size) {
        m_buffer.erase(m_buffer.begin());
        m_dropped_packets++;
    }
}

void JitterBuffer::reset() {
    LockGuard lock(m_mutex);
    m_buffer.clear();
    m_expected_sequence = 0;
    m_dropped_packets = 0;
    m_late_packets = 0;
    m_jitter = 0;
}

bool JitterBuffer::is_packet_ready(const JitterPacket& jitter_packet) const {
    Timestamp now = TimeUtils::now();
    return now >= jitter_packet.expected_playout_time;
}

void JitterBuffer::cleanup_old_packets() {
    Timestamp now = TimeUtils::now();
    Timestamp cutoff = now - (m_delay.count() * 2); // Remove packets older than 2x delay
    
    auto it = m_buffer.begin();
    while (it != m_buffer.end()) {
        if (it->second.arrival_time < cutoff) {
            it = m_buffer.erase(it);
            m_late_packets++;
        } else {
            ++it;
        }
    }
}

SequenceNumber JitterBuffer::get_expected_sequence() const {
    return m_expected_sequence;
}

void JitterBuffer::update_expected_sequence(SequenceNumber seq) {
    // Handle sequence number wraparound
    if (seq > m_expected_sequence) {
        m_expected_sequence = seq + 1;
    } else if (seq < m_expected_sequence && 
               (m_expected_sequence - seq) > 0x8000) { // Wraparound detected
        m_expected_sequence = seq + 1;
    }
}

void JitterBuffer::adjust_delay() {
    // Simple adaptive delay based on jitter
    if (m_jitter > 50) { // High jitter
        m_delay = std::min(m_delay + Milliseconds(10), MAX_DELAY);
    } else if (m_jitter < 20) { // Low jitter
        m_delay = std::max(m_delay - Milliseconds(5), MIN_DELAY);
    }
}

void JitterBuffer::calculate_jitter() {
    if (m_buffer.empty()) {
        return;
    }
    
    // Get the most recent packet
    auto it = m_buffer.end();
    --it;
    
    const JitterPacket& jp = it->second;
    Timestamp arrival = jp.arrival_time;
    Timestamp rtp_timestamp = jp.packet.header.timestamp;
    
    if (m_last_arrival_time > 0) {
        // Calculate transit time difference
        i32 transit_time = static_cast<i32>(arrival) - static_cast<i32>(rtp_timestamp);
        i32 delta = transit_time - m_last_transit_time;
        
        if (delta < 0) {
            delta = -delta;
        }
        
        // Update jitter estimate (RFC 3550 formula)
        m_jitter = m_jitter + (static_cast<u32>(delta) - m_jitter) / 16;
        m_last_transit_time = transit_time;
    } else {
        m_last_transit_time = static_cast<i32>(arrival) - static_cast<i32>(rtp_timestamp);
    }
    
    m_last_arrival_time = arrival;
}
