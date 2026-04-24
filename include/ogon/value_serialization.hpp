#pragma once

#include <ogon/value.hpp>
#include <ogon/base64.hpp>
#include <string>
#include <sstream>

namespace ogon {

inline std::string escape_string(const std::string& s) {
    std::string res;
    for (char c : s) {
        switch (c) {
            case '"': res += "\\\""; break;
            case '\\': res += "\\\\"; break;
            case '\n': res += "\\n"; break;
            case '\r': res += "\\r"; break;
            case '\t': res += "\\t"; break;
            default: res += c;
        }
    }
    return res;
}

inline std::string value_to_agon_string(const value& val) {
    switch (val.type()) {
        case value_t::null: return "null";
        case value_t::int64: return std::to_string(val.get_int64());
        case value_t::double_: return std::to_string(val.get_double());
        case value_t::bool_: return val.get_bool() ? "true" : "false";
        case value_t::string: return "\"" + escape_string(val.get_string_ref()) + "\"";
        case value_t::ref:    return "\"" + val.get_string() + "\"";
        case value_t::binary:
            return "BASE64:" + base64_encode(val.get_binary());
        case value_t::array: {
            std::string res = "[";
            const auto& arr = val.get<value::array_t>();
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i) res += ", ";
                res += value_to_agon_string(arr[i]);
            }
            res += "]";
            return res;
        }
        case value_t::object: {
            std::string res = "{";
            const auto& obj = val.get<value::object_t>();
            bool first = true;
            for (const auto& [k, v] : obj) {
                if (!first) res += ", ";
                first = false;
                res += k + "=" + value_to_agon_string(v);
            }
            res += "}";
            return res;
        }
    }
    return "null";
}

} // namespace ogon