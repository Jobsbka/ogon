#pragma once

#include <string>
#include <ogon/value.hpp>

namespace ogon {
inline namespace literals {

inline value operator"" _ref(const char* str, size_t len) {
    return value(std::string(str, len), value_t::ref);
}

inline value operator"" _uri(const char* str, size_t len) {
    return value(std::string(str, len), value_t::ref);
}

} // namespace literals
} // namespace ogon