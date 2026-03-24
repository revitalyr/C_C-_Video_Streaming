module;

#include <chrono>
#include <string>

export module video_streaming.common.time;

import video_streaming.common.types;

namespace video_streaming {

export class TimeUtils {
public:
    static Timestamp now();
    static Milliseconds to_milliseconds(Timestamp timestamp);
    static Timestamp from_milliseconds(Milliseconds ms);
    static String current_time_string();
    static void sleep_for(Milliseconds ms);
    static void sleep_until(Timestamp timestamp);
};

} // namespace video_streaming