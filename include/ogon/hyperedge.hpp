#pragma once

#include <ogon/fwd.hpp>
#include <ogon/value.hpp>

#include <string>
#include <vector>
#include <unordered_map>

namespace ogon {

struct endpoint {
    std::string node_id;
    int port = 0;
    std::string role; // "in", "out", "inout"

    endpoint() = default;
    endpoint(std::string node, int p = 0, std::string r = "")
        : node_id(std::move(node)), port(p), role(std::move(r)) {}
};

class hyperedge {
public:
    struct impl {
        bool bidirectional = false;
        std::string type_forward;
        std::string type_backward;
        std::vector<endpoint> forward_endpoints;
        std::vector<endpoint> backward_endpoints;
        std::vector<std::string> tags;
        std::unordered_map<std::string, value> attributes;
    };

    hyperedge() = default;

    // Bidirectional
    void set_bidirectional(bool b) { ptr_->bidirectional = b; }
    bool is_bidirectional() const noexcept { return ptr_->bidirectional; }

    // Types
    void set_forward_type(const std::string& t) { ptr_->type_forward = t; }
    const std::string& forward_type() const noexcept { return ptr_->type_forward; }
    void set_backward_type(const std::string& t) { ptr_->type_backward = t; }
    const std::string& backward_type() const noexcept { return ptr_->type_backward; }

    // Forward endpoints
    void add_forward_endpoint(const endpoint& ep) { ptr_->forward_endpoints.push_back(ep); }
    const std::vector<endpoint>& forward_endpoints() const noexcept { return ptr_->forward_endpoints; }

    // Backward endpoints
    void add_backward_endpoint(const endpoint& ep) { ptr_->backward_endpoints.push_back(ep); }
    const std::vector<endpoint>& backward_endpoints() const noexcept { return ptr_->backward_endpoints; }

    // Tags
    void set_tags(const std::vector<std::string>& tags) { ptr_->tags = tags; }
    const std::vector<std::string>& tags() const noexcept { return ptr_->tags; }
    void add_tag(const std::string& tag) { ptr_->tags.push_back(tag); }

    // Attributes
    value& operator[](const std::string& key) { return ptr_->attributes[key]; }
    const value& operator[](const std::string& key) const {
        auto it = ptr_->attributes.find(key);
        if (it != ptr_->attributes.end()) return it->second;
        throw std::out_of_range("edge attribute not found: " + key);
    }
    bool has_attribute(const std::string& key) const {
        return ptr_->attributes.find(key) != ptr_->attributes.end();
    }
    const std::unordered_map<std::string, value>& attributes() const { return ptr_->attributes; }

    bool valid() const noexcept { return ptr_ != nullptr; }

    bool operator==(const hyperedge& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const hyperedge& other) const { return !(*this == other); }

private:
    friend class graph;
    hyperedge(impl* p) : ptr_(p) {}
    impl* ptr_ = nullptr;
};

} // namespace ogon