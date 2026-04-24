#pragma once

#include <string>
#include <optional>
#include <functional>
#include <unordered_map>
#include <ogon/value.hpp>

namespace ogon {

// Абстрактный resolver
class resolver {
public:
    virtual ~resolver() = default;
    virtual std::optional<value> resolve(const std::string& uri) const = 0;
};

// Простой resolver, позволяющий задать callbacks
class map_resolver : public resolver {
public:
    using resolve_func = std::function<std::optional<value>(const std::string&)>;

    void add_handler(const std::string& prefix, resolve_func func) {
        handlers_[prefix] = std::move(func);
    }

    std::optional<value> resolve(const std::string& uri) const override {
        for (const auto& [prefix, func] : handlers_) {
            if (uri.compare(0, prefix.size(), prefix) == 0) {
                return func(uri);
            }
        }
        return std::nullopt;
    }

private:
    std::unordered_map<std::string, resolve_func> handlers_;
};

} // namespace ogon