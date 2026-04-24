#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <ogon/value.hpp>
#include <ogon/error.hpp>
#include <cctype>
#include <sstream>

namespace ogon {

enum class schema_attr_type {
    int_type,
    double_type,
    bool_type,
    string_type,
    ref_type,
    array_type,
    object_type,
    binary_type
};

inline schema_attr_type from_string(const std::string& type_str) {
    if (type_str == "int") return schema_attr_type::int_type;
    if (type_str == "double" || type_str == "float") return schema_attr_type::double_type;
    if (type_str == "bool") return schema_attr_type::bool_type;
    if (type_str == "string") return schema_attr_type::string_type;
    if (type_str == "ref") return schema_attr_type::ref_type;
    if (type_str == "array") return schema_attr_type::array_type;
    if (type_str == "object") return schema_attr_type::object_type;
    if (type_str == "binary") return schema_attr_type::binary_type;
    throw parse_error("Unknown type in schema: " + type_str, 0, 0);
}

struct schema_attr {
    std::string name;
    schema_attr_type type;
    bool required = true;
    std::optional<value> default_value;
};

struct schema_node_def {
    std::string type_name;
    std::vector<schema_attr> attrs;
    std::vector<std::string> required_tags;
};

struct schema_edge_def {
    std::string type_name;
    bool bidirectional = false;
    std::vector<schema_attr> attrs;
};

class type_registry {
public:
    void add_node_schema(const schema_node_def& schema) {
        node_schemas_[schema.type_name] = schema;
    }

    void add_edge_schema(const schema_edge_def& schema) {
        edge_schemas_[schema.type_name] = schema;
    }

    const schema_node_def* get_node_schema(const std::string& type) const {
        auto it = node_schemas_.find(type);
        return it != node_schemas_.end() ? &it->second : nullptr;
    }

    const schema_edge_def* get_edge_schema(const std::string& type) const {
        auto it = edge_schemas_.find(type);
        return it != edge_schemas_.end() ? &it->second : nullptr;
    }

private:
    std::unordered_map<std::string, schema_node_def> node_schemas_;
    std::unordered_map<std::string, schema_edge_def> edge_schemas_;
};

// Парсер @schema блока
inline type_registry parse_schema_block(const std::string& source) {
    type_registry registry;
    size_t pos = 0, line = 1;

    auto skip_ws = [&]() {
        while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t' || source[pos] == '\n' || source[pos] == '\r')) {
            if (source[pos] == '\n') ++line;
            ++pos;
        }
        while (pos < source.size() && source[pos] == '/' && pos+1 < source.size() && (source[pos+1] == '/' || source[pos+1] == '*')) {
            if (source[pos+1] == '/') {
                pos += 2;
                while (pos < source.size() && source[pos] != '\n') ++pos;
            } else {
                pos += 2;
                while (pos+1 < source.size() && !(source[pos]=='*' && source[pos+1]=='/')) {
                    if (source[pos] == '\n') ++line;
                    ++pos;
                }
                if (pos+1 < source.size()) pos += 2;
            }
            while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t' || source[pos] == '\n' || source[pos] == '\r')) {
                if (source[pos] == '\n') ++line;
                ++pos;
            }
        }
    };

    auto parse_id = [&]() -> std::string {
        skip_ws();
        if (pos >= source.size() || !std::isalpha(static_cast<unsigned char>(source[pos])))
            throw parse_error("Expected identifier", line, pos);
        size_t start = pos;
        while (pos < source.size() && (std::isalnum(static_cast<unsigned char>(source[pos])) || source[pos] == '_')) ++pos;
        return source.substr(start, pos - start);
    };

    while (pos < source.size()) {
        skip_ws();
        if (pos >= source.size()) break;
        std::string decl = parse_id();
        if (decl == "node" || decl.empty()) {
            if (decl.empty()) --pos; // rollback
            skip_ws(); // <-- ВАЖНО: пропускаем пробелы перед именем
            std::string type_name;
            if (pos < source.size() && source[pos] == '"') {
                ++pos;
                size_t start = pos;
                while (pos < source.size() && source[pos] != '"') ++pos;
                if (pos >= source.size()) throw parse_error("Unterminated string", line, pos);
                type_name = source.substr(start, pos - start);
                ++pos;
            } else {
                type_name = parse_id();
            }
            skip_ws();
            if (source[pos] != '{') throw parse_error("Expected '{'", line, pos);
            ++pos;
            schema_node_def node_schema;
            node_schema.type_name = type_name;
            while (pos < source.size()) {
                skip_ws();
                if (source[pos] == '}') { ++pos; break; }
                bool optional = false;
                if (source.compare(pos, 9, "@optional") == 0) {
                    optional = true;
                    pos += 9;
                    skip_ws();
                }
                std::string attr_name = parse_id();
                skip_ws();
                if (source[pos] != ':') throw parse_error("Expected ':'", line, pos);
                ++pos;
                skip_ws();
                std::string attr_type_str = parse_id();
                schema_attr_type attr_type = from_string(attr_type_str);
                bool required = true;
                std::optional<value> default_val;
                skip_ws();
                if (source.compare(pos, 8, "required") == 0) {
                    pos += 8;
                    required = true;
                } else if (source.compare(pos, 8, "optional") == 0) {
                    pos += 8;
                    required = false;
                } else if (source[pos] == '=') {
                    ++pos;
                    while (pos < source.size() && source[pos] != ';' && source[pos] != '}' && source[pos] != '\n') ++pos;
                }
                if (optional) required = false;
                schema_attr attr{attr_name, attr_type, required, default_val};
                node_schema.attrs.push_back(attr);
                skip_ws();
                if (pos < source.size() && source[pos] == ';') ++pos;
                else if (pos < source.size() && source[pos] == ',') ++pos;
            }
            registry.add_node_schema(node_schema);
        } else if (decl == "edge") {
            skip_ws(); // аналогично для edge
            std::string type_name;
            if (pos < source.size() && source[pos] == '"') {
                ++pos;
                size_t start = pos;
                while (pos < source.size() && source[pos] != '"') ++pos;
                if (pos >= source.size()) throw parse_error("Unterminated string", line, pos);
                type_name = source.substr(start, pos - start);
                ++pos;
            } else {
                type_name = parse_id();
            }
            skip_ws();
            if (source[pos] != '{') throw parse_error("Expected '{'", line, pos);
            ++pos;
            schema_edge_def edge_schema;
            edge_schema.type_name = type_name;
            while (pos < source.size()) {
                skip_ws();
                if (source[pos] == '}') { ++pos; break; }
                bool optional = false;
                if (source.compare(pos, 9, "@optional") == 0) {
                    optional = true;
                    pos += 9;
                    skip_ws();
                }
                std::string attr_name = parse_id();
                skip_ws();
                if (source[pos] != ':') throw parse_error("Expected ':'", line, pos);
                ++pos;
                skip_ws();
                std::string attr_type_str = parse_id();
                schema_attr_type attr_type = from_string(attr_type_str);
                bool required = true;
                std::optional<value> default_val;
                skip_ws();
                if (source.compare(pos, 8, "required") == 0) pos += 8;
                else if (source.compare(pos, 8, "optional") == 0) { pos += 8; required = false; }
                if (optional) required = false;
                edge_schema.attrs.push_back({attr_name, attr_type, required, default_val});
                skip_ws();
                if (pos < source.size() && source[pos] == ';') ++pos;
                else if (pos < source.size() && source[pos] == ',') ++pos;
            }
            registry.add_edge_schema(edge_schema);
        } else {
            throw parse_error("Expected 'node' or 'edge' in schema", line, pos);
        }
    }
    return registry;
}

} // namespace ogon