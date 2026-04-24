#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <ogon/value.hpp>
#include <ogon/value_serialization.hpp>

namespace ogon {

class template_library {
public:
    void define(const std::string& name, const std::vector<std::string>& params, const std::string& body) {
        templates_[name] = {params, body};
    }

    std::string instantiate(const std::string& name, const std::unordered_map<std::string, value>& args) const {
        auto it = templates_.find(name);
        if (it == templates_.end()) throw std::runtime_error("Template not found: " + name);
        std::string body = it->second.second;
        for (const auto& [param, val] : args) {
            std::string placeholder = "{{" + param + "}}";
            std::string replacement = value_to_agon_string(val);
            size_t pos = 0;
            while ((pos = body.find(placeholder, pos)) != std::string::npos) {
                body.replace(pos, placeholder.length(), replacement);
                pos += replacement.length();
            }
        }
        return body;
    }

private:
    std::unordered_map<std::string, std::pair<std::vector<std::string>, std::string>> templates_;
};

} // namespace ogon