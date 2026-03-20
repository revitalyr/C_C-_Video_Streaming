#pragma once

#include "../common/types.hpp"

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
