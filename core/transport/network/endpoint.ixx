module;

#include <string>
#include <compare> // for operator<=>

export module video_streaming.network.endpoint;

import video_streaming.common.types;

namespace video_streaming {

export struct Endpoint {
    IpAddress ip_address;
    Port port;
    
    Endpoint() : ip_address("0.0.0.0"), port(0) {}
    Endpoint(const IpAddress& ip, Port p) : ip_address(ip), port(p) {}
    Endpoint(const String& ip_port);
    
    bool is_valid() const;
    String to_string() const;
    bool from_string(const String& str);
    
    // Comparison operators for use in maps/sets
    auto operator<=>(const Endpoint&) const = default;
};

} // namespace video_streaming