#include "endpoint.hpp"
#include <sstream>

Endpoint::Endpoint(const String& ip_port) {
    from_string(ip_port);
}

String Endpoint::to_string() const {
    return ip_address + ":" + std::to_string(port);
}

bool Endpoint::from_string(const String& str) {
    size_t colon_pos = str.find_last_of(':');
    if (colon_pos == String::npos) {
        return false;
    }
    
    ip_address = str.substr(0, colon_pos);
    String port_str = str.substr(colon_pos + 1);
    
    try {
        port = static_cast<Port>(std::stoul(port_str));
        return true;
    } catch (...) {
        return false;
    }
}
