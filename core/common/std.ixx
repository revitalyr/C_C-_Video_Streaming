module;

#include <format>

export module video_streaming.std;

export namespace video_streaming {

// Re-export std::format for use in modules
using std::format;
using std::make_format_args;
using std::format_args;
using std::vformat;

} // namespace video_streaming
