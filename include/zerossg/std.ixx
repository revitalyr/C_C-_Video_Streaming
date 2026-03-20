module;

#include <format>

export module zerossg.std;

export namespace zerossg {

// Re-export std::format for use in modules
using std::format;
using std::make_format_args;
using std::format_args;
using std::vformat;

} // namespace zerossg
