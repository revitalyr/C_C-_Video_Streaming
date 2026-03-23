#pragma once

#include "common/types.hpp"
#include <fmt/format.h>

struct Endpoint {
    IpAddress ip_address;
    Port port;
    
    Endpoint() = default;
    Endpoint(const IpAddress& ip, Port p) : ip_address(ip), port(p) {}
    Endpoint(const String& ip_port); // "192.168.1.100:5004" format
    
    bool is_valid() const {
        return !ip_address.empty() && port > 0;
    }
    
    String to_string() const;
    bool from_string(const String& str);
    
    bool operator==(const Endpoint& other) const {
        return ip_address == other.ip_address && port == other.port;
    }
    
    bool operator!=(const Endpoint& other) const {
        return !(*this == other);
    }
    
    bool operator<(const Endpoint& other) const {
        if (ip_address != other.ip_address) {
            return ip_address < other.ip_address;
        }
        return port < other.port;
    }
};

// Hash function for std::unordered_map
struct EndpointHash {
    std::size_t operator()(const Endpoint& endpoint) const {
        return std::hash<String>()(endpoint.ip_address) ^ 
               (std::hash<Port>()(endpoint.port) << 1);
    }
};

// Custom formatter for Endpoint to allow direct logging
template <>
struct fmt::formatter<Endpoint> {
    enum class Presentation { Full, Ip, Port };
    Presentation presentation = Presentation::Full;

    constexpr auto parse(fmt::format_parse_context& ctx) {
        auto it = ctx.begin();
        auto end = ctx.end();
        
        if (it != end && *it != '}') {
            if (end - it >= 2 && it[0] == 'i' && it[1] == 'p') {
                presentation = Presentation::Ip;
                it += 2;
            } else if (end - it >= 4 && it[0] == 'p' && it[1] == 'o' && it[2] == 'r' && it[3] == 't') {
                presentation = Presentation::Port;
                it += 4;
            } else {
                throw fmt::format_error("invalid format specifier for Endpoint");
            }
        }
        
        if (it != end && *it != '}') throw fmt::format_error("invalid format specifier for Endpoint");
        return it;
    }

    template <typename FormatContext>
    auto format(const Endpoint& endpoint, FormatContext& ctx) const {
        if (presentation == Presentation::Ip) return fmt::format_to(ctx.out(), "{}", endpoint.ip_address);
        if (presentation == Presentation::Port) return fmt::format_to(ctx.out(), "{}", endpoint.port);
        return fmt::format_to(ctx.out(), "{}:{}", endpoint.ip_address, endpoint.port);
    }
};
