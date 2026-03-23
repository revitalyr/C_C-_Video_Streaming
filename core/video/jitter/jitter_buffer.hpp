#pragma once

#include "rtp/rtp_packet.hpp"
#include "common/time.hpp"
#include <map>
#include <queue>
#include <mutex>

struct JitterPacket {
    RtpPacket packet;
    Timestamp arrival_time;
    Timestamp expected_playout_time;
    
    JitterPacket(const RtpPacket& pkt, Timestamp arrival, Timestamp playout)
        : packet(pkt), arrival_time(arrival), expected_playout_time(playout) {}
};

class JitterBuffer {
public:
    explicit JitterBuffer(size_t max_size = DEFAULT_JITTER_BUFFER_SIZE,
                         Milliseconds delay = DEFAULT_JITTER_DELAY);
    
    // Push packet into buffer
    void push(const RtpPacket& packet);
    
    // Pop next packet for playout (returns false if no packet ready)
    bool pop(RtpPacket& packet);
    
    // Get statistics
    size_t size() const;
    bool empty() const;
    
    // Configuration
    void set_delay(Milliseconds delay);
    Milliseconds get_delay() const { return m_delay; }
    
    void set_max_size(size_t max_size);
    size_t get_max_size() const { return m_max_size; }
    
    // Statistics
    size_t get_dropped_packets() const { return m_dropped_packets; }
    size_t get_late_packets() const { return m_late_packets; }
    
    // Reset buffer
    void reset();

private:
    bool is_packet_ready(const JitterPacket& jitter_packet) const;
    void cleanup_old_packets();
    SequenceNumber get_expected_sequence() const;
    void update_expected_sequence(SequenceNumber seq);
    
    // Adaptive delay adjustment
    void adjust_delay();
    void calculate_jitter();

private:
    mutable Mutex m_mutex;
    std::map<SequenceNumber, JitterPacket> m_buffer;
    std::queue<SequenceNumber> m_ready_queue;
    
    SequenceNumber m_expected_sequence{0};
    Milliseconds m_delay{DEFAULT_JITTER_DELAY};
    size_t m_max_size{DEFAULT_JITTER_BUFFER_SIZE};
    
    // Statistics
    size_t m_dropped_packets{0};
    size_t m_late_packets{0};
    Timestamp m_last_arrival_time{0};
    i32 m_last_transit_time{0};
    u32 m_jitter{0};
    
    // Adaptive delay parameters
    static constexpr Milliseconds MIN_DELAY{50};
    static constexpr Milliseconds MAX_DELAY{500};
    static constexpr size_t STATS_WINDOW = 100;
};
