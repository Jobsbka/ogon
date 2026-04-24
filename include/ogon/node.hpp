#pragma once

#include <ogon/fwd.hpp>
#include <ogon/value.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace ogon {

class node {
public:
    // internal use
    struct impl {
        std::string type;
        std::string id;
        std::vector<std::string> tags;
        std::unordered_map<std::string, value> attributes;
    };

    node() = default;

    // Accessors
    const std::string& type() const noexcept { return ptr_->type; }
    const std::string& id() const noexcept { return ptr_->id; }

    value& operator[](const std::string& key) {
        return ptr_->attributes[key];
    }
    const value& operator[](const std::string& key) const {
        auto it = ptr_->attributes.find(key);
        if (it != ptr_->attributes.end()) return it->second;
        throw std::out_of_range("attribute not found: " + key);
    }
    bool has_attribute(const std::string& key) const {
        return ptr_->attributes.find(key) != ptr_->attributes.end();
    }
    void remove_attribute(const std::string& key) {
        ptr_->attributes.erase(key);
    }

    void set_tag(const std::vector<std::string>& tags) { ptr_->tags = tags; }
    const std::vector<std::string>& tags() const noexcept { return ptr_->tags; }
    void add_tag(const std::string& tag) { ptr_->tags.push_back(tag); }

    // For iteration over attributes
    auto attributes_begin() { return ptr_->attributes.begin(); }
    auto attributes_end() { return ptr_->attributes.end(); }
    auto attributes_begin() const { return ptr_->attributes.begin(); }
    auto attributes_end() const { return ptr_->attributes.end(); }

    // Valid check
    bool valid() const noexcept { return ptr_ != nullptr; }

    // Comparison (based on id and type)
    bool operator==(const node& other) const {
        return ptr_ == other.ptr_;
    }
    bool operator!=(const node& other) const { return !(*this == other); }

private:
    friend class graph;
    node(impl* p) : ptr_(p) {}
    impl* ptr_ = nullptr;
};

} // namespace ogon