#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>

namespace ogon {

inline std::vector<uint8_t> base64_decode(const std::string& input) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (c == '=') break;
        const char* pos = strchr(chars, c);
        if (!pos) throw std::invalid_argument("Invalid Base64 character");
        val = (val << 6) + (pos - chars);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

inline std::string base64_encode(const std::vector<uint8_t>& data) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    for (uint8_t byte : data) {
        val = (val << 8) + byte;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4) result.push_back('=');
    return result;
}

} // namespace ogon