#pragma once

#include <ogon/fwd.hpp>
#include <ogon/error.hpp>
#include <ogon/base64.hpp>

#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <stdexcept>
#include <initializer_list>

namespace ogon {

struct uri_t {
    std::string uri;
    bool operator==(const uri_t& other) const { return uri == other.uri; }
};

enum class value_t : uint8_t {
    null,
    int64,
    double_,
    bool_,
    string,
    array,
    object,
    ref,
    binary       // новый тип
};

class value {
public:
    using array_t = std::vector<value>;
    using object_t = std::unordered_map<std::string, value>;
    using binary_t = std::vector<uint8_t>;

    // Конструкторы
    value() noexcept : data_(std::monostate{}) {}
    value(std::nullptr_t) noexcept : data_(std::monostate{}) {}

    value(int32_t v) : data_(static_cast<int64_t>(v)) {}
    value(uint32_t v) : data_(static_cast<int64_t>(v)) {}
    value(int64_t v) : data_(v) {}
    value(uint64_t v) : data_(static_cast<int64_t>(v)) {}
    value(double v) : data_(v) {}
    value(bool v) : data_(v) {}
    value(const std::string& v) : data_(v) {}
    value(const char* v) : data_(std::string(v)) {}
    value(std::string&& v) noexcept : data_(std::move(v)) {}
    value(const array_t& v) : data_(v) {}
    value(array_t&& v) noexcept : data_(std::move(v)) {}
    value(const object_t& v) : data_(v) {}
    value(object_t&& v) noexcept : data_(std::move(v)) {}
    value(std::initializer_list<value> list) : data_(array_t(list)) {}
    value(std::initializer_list<std::pair<const std::string, value>> list) : data_(object_t(list)) {}

    // Конструктор для ref
    value(const std::string& str, value_t type) {
        if (type == value_t::ref) data_ = uri_t{str};
        else if (type == value_t::binary) {
            // если строка начинается с BASE64:, декодируем
            if (str.compare(0, 7, "BASE64:") == 0) {
                data_ = base64_decode(str.substr(7));
            } else {
                // можно считать сырым бинарным представлением
                data_ = binary_t(str.begin(), str.end());
            }
        } else {
            throw type_error("Invalid explicit type constructor");
        }
    }

    // Конструктор для binary (vector<uint8_t>)
    value(binary_t&& bin) noexcept : data_(std::move(bin)) {}
    value(const binary_t& bin) : data_(bin) {}

    // Тип значения
    value_t type() const noexcept {
        switch (data_.index()) {
            case 0: return value_t::null;
            case 1: return value_t::int64;
            case 2: return value_t::double_;
            case 3: return value_t::bool_;
            case 4: return value_t::string;
            case 5: return value_t::array;
            case 6: return value_t::object;
            case 7: return value_t::ref;
            case 8: return value_t::binary;
            default: return value_t::null;
        }
    }

    bool is_null() const noexcept { return type() == value_t::null; }
    bool is_int() const noexcept { return type() == value_t::int64; }
    bool is_double() const noexcept { return type() == value_t::double_; }
    bool is_bool() const noexcept { return type() == value_t::bool_; }
    bool is_string() const noexcept { return type() == value_t::string; }
    bool is_array() const noexcept { return type() == value_t::array; }
    bool is_object() const noexcept { return type() == value_t::object; }
    bool is_ref() const noexcept { return type() == value_t::ref; }
    bool is_binary() const noexcept { return type() == value_t::binary; }

    // Шаблонные геттеры
    template<typename T> T get() const {
        if constexpr (std::is_same_v<T, int64_t>) {
            if (auto* v = std::get_if<int64_t>(&data_)) return *v;
            throw type_error("not int64");
        } else if constexpr (std::is_same_v<T, double>) {
            if (auto* v = std::get_if<double>(&data_)) return *v;
            throw type_error("not double");
        } else if constexpr (std::is_same_v<T, bool>) {
            if (auto* v = std::get_if<bool>(&data_)) return *v;
            throw type_error("not bool");
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (auto* v = std::get_if<std::string>(&data_)) return *v;
            throw type_error("not string");
        } else if constexpr (std::is_same_v<T, array_t>) {
            if (auto* v = std::get_if<array_t>(&data_)) return *v;
            throw type_error("not array");
        } else if constexpr (std::is_same_v<T, object_t>) {
            if (auto* v = std::get_if<object_t>(&data_)) return *v;
            throw type_error("not object");
        } else if constexpr (std::is_same_v<T, binary_t>) {
            if (auto* v = std::get_if<binary_t>(&data_)) return *v;
            throw type_error("not binary");
        } else {
            throw type_error("unsupported type");
        }
    }

    int64_t get_int64(int64_t def = 0) const noexcept {
        return is_int() ? std::get<int64_t>(data_) : def;
    }
    double get_double(double def = 0.0) const noexcept {
        return is_double() ? std::get<double>(data_) : def;
    }
    bool get_bool(bool def = false) const noexcept {
        return is_bool() ? std::get<bool>(data_) : def;
    }

    std::string get_string(const std::string& def = "") const noexcept {
        if (auto* u = std::get_if<uri_t>(&data_)) return u->uri;
        if (auto* s = std::get_if<std::string>(&data_)) return *s;
        return def;
    }

    const std::string& get_string_ref() const {
        if (auto* u = std::get_if<uri_t>(&data_)) return u->uri;
        return std::get<std::string>(data_);
    }

    const binary_t& get_binary() const {
        return std::get<binary_t>(data_);
    }

    // Операции с массивом
    value& operator[](size_t idx) {
        auto& arr = std::get<array_t>(data_);
        if (idx >= arr.size()) throw std::out_of_range("array index out of range");
        return arr[idx];
    }
    const value& operator[](size_t idx) const {
        return const_cast<value*>(this)->operator[](idx);
    }
    void push_back(const value& val) { std::get<array_t>(data_).push_back(val); }
    void push_back(value&& val) { std::get<array_t>(data_).push_back(std::move(val)); }
    size_t size() const {
        if (is_array()) return std::get<array_t>(data_).size();
        if (is_object()) return std::get<object_t>(data_).size();
        if (is_binary()) return std::get<binary_t>(data_).size();
        return 0;
    }
    bool empty() const { return size() == 0; }

    // Операции с объектом
    value& operator[](const std::string& key) {
        auto& obj = std::get<object_t>(data_);
        return obj[key];
    }
    const value& operator[](const std::string& key) const {
        const auto& obj = std::get<object_t>(data_);
        auto it = obj.find(key);
        if (it != obj.end()) return it->second;
        throw std::out_of_range("object key not found: " + key);
    }
    bool contains(const std::string& key) const {
        if (!is_object()) return false;
        return std::get<object_t>(data_).count(key) > 0;
    }
    void erase(const std::string& key) {
        if (is_object()) std::get<object_t>(data_).erase(key);
    }

    // Итераторы для массивов
    auto begin() {
        if (is_array()) return std::get<array_t>(data_).begin();
        throw type_error("cannot iterate non-array value");
    }
    auto end() {
        if (is_array()) return std::get<array_t>(data_).end();
        throw type_error("cannot iterate non-array value");
    }
    auto begin() const { return const_cast<value*>(this)->begin(); }
    auto end() const { return const_cast<value*>(this)->end(); }

    bool operator==(const value& other) const { return data_ == other.data_; }
    bool operator!=(const value& other) const { return !(*this == other); }

private:
    using variant_type = std::variant<
        std::monostate,
        int64_t,
        double,
        bool,
        std::string,
        array_t,
        object_t,
        uri_t,
        binary_t
    >;
    variant_type data_;
};

} // namespace ogon