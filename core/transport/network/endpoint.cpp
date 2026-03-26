module;

#include <sstream>

module video_streaming.network.endpoint;

import video_streaming.common.types;

namespace video_streaming {

Endpoint::Endpoint(const String& ip_port) {
    from_string(ip_port);
}

bool Endpoint::is_valid() const {
    return !ip_address.empty() && port > 0;
}

String Endpoint::to_string() const {
    return ip_address + ":" + std::to_string(static_cast<int>(port));
}

bool Endpoint::from_string(const String& str) {
    size_t colon_pos = str.find(':');
    if (colon_pos == String::npos) {
        return false;
    }
    
    ip_address = str.substr(0, colon_pos);
    String port_str = str.substr(colon_pos + 1);
    
    try {
        port = static_cast<Port>(std::stoi(port_str));
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace video_streaming
