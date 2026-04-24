// include/ogon/graph.hpp
#pragma once

#include <ogon/fwd.hpp>
#include <ogon/node.hpp>
#include <ogon/hyperedge.hpp>
#include <ogon/value.hpp>
#include <ogon/error.hpp>
#include <ogon/base64.hpp>
#include <ogon/resolver.hpp>
#include <ogon/schema_parser.hpp>
#include <ogon/value_serialization.hpp>

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <fstream>
#include <cctype>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <functional>

namespace ogon {

struct data_block {
    std::string name;
    value data;
};

struct table_block {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<value>> rows;
};

struct template_def {
    std::string name;
    std::vector<std::string> params;
    std::string body;
};

class graph {
public:
    graph() = default;
    ~graph() = default;
    graph(const graph&) = delete;
    graph& operator=(const graph&) = delete;
    graph(graph&&) = default;
    graph& operator=(graph&&) = default;

    // ---------- Узлы ----------
    node add_node(const std::string& type, const std::string& id, bool allow_dup = false) {
        if (!allow_dup && node_index_.count(id)) {
            throw graph_error("Node with id '" + id + "' already exists");
        }
        auto ptr = std::make_unique<node::impl>();
        ptr->type = type;
        ptr->id = id;
        auto* raw = ptr.get();
        nodes_.push_back(std::move(ptr));
        node_index_[id] = raw;
        return node(raw);
    }

    node node_by_id(const std::string& id) const {
        auto it = node_index_.find(id);
        if (it != node_index_.end()) return node(it->second);
        return node();
    }

    void remove_node(const std::string& id) {
        auto it = node_index_.find(id);
        if (it == node_index_.end()) return;
        edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
            [&](const std::unique_ptr<hyperedge::impl>& e) {
                for (const endpoint& ep : e->forward_endpoints)
                    if (ep.node_id == id) return true;
                for (const endpoint& ep : e->backward_endpoints)
                    if (ep.node_id == id) return true;
                return false;
            }), edges_.end());
        auto iter = std::find_if(nodes_.begin(), nodes_.end(),
            [&](const std::unique_ptr<node::impl>& n) { return n.get() == it->second; });
        if (iter != nodes_.end()) {
            nodes_.erase(iter);
            node_index_.erase(it);
        }
    }

    // ---------- Рёбра ----------
    hyperedge add_edge() {
        auto ptr = std::make_unique<hyperedge::impl>();
        auto* raw = ptr.get();
        edges_.push_back(std::move(ptr));
        return hyperedge(raw);
    }

    hyperedge connect(const std::string& left_id, const std::string& right_id,
                      const std::string& type = "") {
        auto e = add_edge();
        e.add_forward_endpoint(endpoint(left_id, 0, "out"));
        e.add_forward_endpoint(endpoint(right_id, 0, "in"));
        if (!type.empty()) e.set_forward_type(type);
        return e;
    }

    hyperedge connect(const node& left, const node& right, const std::string& type = "") {
        return connect(left.id(), right.id(), type);
    }

    // ---------- Метаданные ----------
    void set_metadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }
    std::string get_metadata(const std::string& key) const {
        auto it = metadata_.find(key);
        return it != metadata_.end() ? it->second : "";
    }
    const std::unordered_map<std::string, std::string>& metadata() const { return metadata_; }

    // ---------- @data ----------
    void set_data_block(const std::string& name, const value& data) {
        data_blocks_[name] = data;
    }
    value* get_data_block(const std::string& name) {
        auto it = data_blocks_.find(name);
        return it != data_blocks_.end() ? &it->second : nullptr;
    }
    const value* get_data_block(const std::string& name) const {
        auto it = data_blocks_.find(name);
        return it != data_blocks_.end() ? &it->second : nullptr;
    }
    const std::unordered_map<std::string, value>& data_blocks() const { return data_blocks_; }

    // ---------- @table ----------
    void set_table(const std::string& name, const table_block& table) {
        tables_[name] = table;
    }
    table_block* get_table(const std::string& name) {
        auto it = tables_.find(name);
        return it != tables_.end() ? &it->second : nullptr;
    }
    const table_block* get_table(const std::string& name) const {
        auto it = tables_.find(name);
        return it != tables_.end() ? &it->second : nullptr;
    }
    const std::unordered_map<std::string, table_block>& tables() const { return tables_; }

    // ---------- Шаблоны ----------
    void define_template(const std::string& name, const std::vector<std::string>& params, const std::string& body) {
        templates_[name] = template_def{name, params, body};
    }
    const template_def* get_template(const std::string& name) const {
        auto it = templates_.find(name);
        return it != templates_.end() ? &it->second : nullptr;
    }
    void instantiate_template(const std::string& name, const std::unordered_map<std::string, value>& args);

    // ---------- Разрешение ссылок ----------
    void resolve_references(const resolver& resolver);

    // ---------- Итераторы ----------
    auto node_range() const {
        struct iterator {
            using vec_t = std::vector<std::unique_ptr<node::impl>>;
            vec_t::const_iterator it;
            iterator(vec_t::const_iterator i) : it(i) {}
            node operator*() const { return node(it->get()); }
            bool operator!=(const iterator& other) const { return it != other.it; }
            iterator& operator++() { ++it; return *this; }
        };
        struct range { const graph* g;
            iterator begin() const { return iterator(g->nodes_.cbegin()); }
            iterator end() const { return iterator(g->nodes_.cend()); }
        };
        return range{this};
    }

    auto edge_range() const {
        struct iterator {
            using vec_t = std::vector<std::unique_ptr<hyperedge::impl>>;
            vec_t::const_iterator it;
            iterator(vec_t::const_iterator i) : it(i) {}
            hyperedge operator*() const { return hyperedge(it->get()); }
            bool operator!=(const iterator& other) const { return it != other.it; }
            iterator& operator++() { ++it; return *this; }
        };
        struct range { const graph* g;
            iterator begin() const { return iterator(g->edges_.cbegin()); }
            iterator end() const { return iterator(g->edges_.cend()); }
        };
        return range{this};
    }

    // ---------- Сериализация ----------
    std::string to_agon(bool with_section_comments = false) const;

    static std::string value_to_agon_string(const value& val) {
        return ogon::value_to_agon_string(val);
    }

    // Публичная вспомогательная функция для парсера
    static void skip_whitespace_and_comments(const std::string& s, size_t& pos, size_t& line);

    // ---------- Парсинг ----------
    static graph parse(const std::string& data, bool lenient = false);
    static graph parse(std::istream& stream, bool lenient = false);
    static graph parse_file(const std::string& path, bool lenient = false);

    void save(const std::string& filename) const;
    void load(const std::string& filename);

private:
    std::vector<std::unique_ptr<node::impl>> nodes_;
    std::vector<std::unique_ptr<hyperedge::impl>> edges_;
    std::unordered_map<std::string, node::impl*> node_index_;
    std::unordered_map<std::string, std::string> metadata_;
    std::unordered_map<std::string, value> data_blocks_;
    std::unordered_map<std::string, table_block> tables_;
    std::unordered_map<std::string, template_def> templates_;

    // Сериализация хелперы
    static void serialize_node(const node::impl& n, std::ostringstream& out);
    static void serialize_edge(const hyperedge& he, std::ostringstream& out);
    static std::string format_eps(const std::vector<endpoint>& eps);

    // Вспомогательный метод: пропускает пробелы/табы и комментарии, но НЕ переходит через '\n'
    static void skip_spaces(const std::string& s, size_t& pos, size_t& line);

    // Пропускает запятые и пробелы до stop-символа, но не переходит \n
    static void skip_delimiters(const std::string& s, size_t& pos, size_t& line, char stop);

    // Парсер (приватные методы)
    static void parse_impl(const std::string& data, graph& g, const std::string& base_dir, bool lenient);
    static void parse_include(const std::string& s, size_t& pos, graph& g, size_t& line, const std::string& base_dir);
    static void parse_data_block(const std::string& s, size_t& pos, graph& g, size_t& line);
    static void parse_table_block(const std::string& s, size_t& pos, graph& g, size_t& line);
    static void parse_node_line(const std::string& s, size_t& pos, graph& g, size_t& line);
    static void parse_edge_line(const std::string& s, size_t& pos, graph& g, size_t& line);
    static std::vector<endpoint> parse_endpoint_list(const std::string& s, size_t& pos, size_t& line);
    static bool parse_single_endpoint(const std::string& s, size_t& pos, endpoint& ep, size_t& line);
    static void parse_edge_attrs(const std::string& s, size_t& pos, hyperedge& e, size_t& line);
    static void parse_metadata(const std::string& s, size_t& pos, graph& g, size_t& line);
    static bool parse_id(const std::string& s, size_t& pos, std::string& out);
    static bool parse_id_or_number(const std::string& s, size_t& pos, std::string& out);
    static value parse_value(const std::string& s, size_t& pos, size_t& line);
    static bool parse_string(const std::string& s, size_t& pos, std::string& out, size_t& line);
    static value parse_number(const std::string& s, size_t& pos, size_t& line);
    static value parse_literal(const std::string& s, size_t& pos, size_t& line);
    static value parse_array(const std::string& s, size_t& pos, size_t& line);
    static value parse_object(const std::string& s, size_t& pos, size_t& line);
    static std::pair<std::string, value> parse_attr(const std::string& s, size_t& pos, size_t& line);
    static void skip_to_next_line(const std::string& s, size_t& pos, size_t& line);
};

// ============================================================
// РЕАЛИЗАЦИЯ МЕТОДОВ
// ============================================================

inline void graph::instantiate_template(const std::string& name, const std::unordered_map<std::string, value>& args) {
    const template_def* tmpl = get_template(name);
    if (!tmpl) throw graph_error("Template not found: " + name);
    std::string body = tmpl->body;
    for (const auto& [param, val] : args) {
        std::string placeholder = "{{" + param + "}}";
        std::string replacement;
        if (val.is_string()) {
            replacement = val.get_string();
        } else {
            replacement = ogon::value_to_agon_string(val);
        }
        size_t pos = 0;
        while ((pos = body.find(placeholder, pos)) != std::string::npos) {
            body.replace(pos, placeholder.length(), replacement);
            pos += replacement.length();
        }
    }
    graph temp = parse(body);
    for (auto n : temp.node_range()) {
        auto new_node = add_node(n.type(), n.id(), true);
        new_node.set_tag(n.tags());
        for (auto it = n.attributes_begin(); it != n.attributes_end(); ++it) {
            new_node[it->first] = it->second;
        }
    }
    for (auto e : temp.edge_range()) {
        auto new_edge = add_edge();
        if (e.is_bidirectional()) {
            new_edge.set_bidirectional(true);
            new_edge.set_forward_type(e.forward_type());
            new_edge.set_backward_type(e.backward_type());
            for (const endpoint& ep : e.forward_endpoints()) new_edge.add_forward_endpoint(ep);
            for (const endpoint& ep : e.backward_endpoints()) new_edge.add_backward_endpoint(ep);
        } else {
            new_edge.set_forward_type(e.forward_type());
            for (const endpoint& ep : e.forward_endpoints()) new_edge.add_forward_endpoint(ep);
        }
        new_edge.set_tags(e.tags());
        for (const auto& [k, v] : e.attributes()) new_edge[k] = v;
    }
    for (auto& [k, v] : temp.data_blocks_) data_blocks_[k] = v;
    for (auto& [k, v] : temp.tables_) tables_[k] = v;
}

inline std::string graph::to_agon(bool with_section_comments) const {
    std::ostringstream out;
    bool section_started = false;

    if (!metadata_.empty()) {
        for (const auto& [k, v] : metadata_) {
            out << "#!" << k << " " << v << "\n";
        }
        section_started = true;
    }

    if (!nodes_.empty()) {
        if (section_started) out << "\n";
        if (with_section_comments) out << "// nodes\n";
        for (const auto& nptr : nodes_) {
            serialize_node(*nptr, out);
            out << "\n";
        }
        section_started = true;
    }

    if (!edges_.empty()) {
        if (section_started) out << "\n";
        if (with_section_comments) out << "// edges\n";
        for (const auto& eptr : edges_) {
            hyperedge he(eptr.get());
            serialize_edge(he, out);
            out << "\n";
        }
        section_started = true;
    }

    bool has_data_or_table = !data_blocks_.empty() || !tables_.empty();
    if (has_data_or_table) {
        if (section_started) out << "\n";
        if (with_section_comments && !data_blocks_.empty()) out << "// data\n";
        for (const auto& [name, val] : data_blocks_) {
            out << "@data \"" << ogon::escape_string(name) << "\" ";
            out << ogon::value_to_agon_string(val);
            out << "\n";
        }
        if (!data_blocks_.empty() && !tables_.empty()) {
            out << "\n";
            if (with_section_comments) out << "// tables\n";
        } else if (!tables_.empty()) {
            if (with_section_comments) out << "// tables\n";
        }
        if (!tables_.empty()) {
            size_t count = 0;
            for (const auto& [name, table] : tables_) {
                out << "@table \"" << ogon::escape_string(name) << "\" {\n";
                out << "    columns = [";
                for (size_t i = 0; i < table.columns.size(); ++i) {
                    if (i) out << ", ";
                    out << "\"" << ogon::escape_string(table.columns[i]) << "\"";
                }
                out << "]\n";
                out << "    rows = [\n";
                for (size_t r = 0; r < table.rows.size(); ++r) {
                    out << "        [";
                    for (size_t c = 0; c < table.rows[r].size(); ++c) {
                        if (c) out << ", ";
                        out << ogon::value_to_agon_string(table.rows[r][c]);
                    }
                    out << "]";
                    if (r + 1 < table.rows.size()) out << ",";
                    out << "\n";
                }
                out << "    ]\n}";
                ++count;
                if (count < tables_.size()) out << "\n";
            }
        }
    }
    return out.str();
}

inline void graph::resolve_references(const resolver& resolver) {
    for (auto& nptr : nodes_) {
        for (auto& [key, val] : nptr->attributes) {
            if (val.is_ref()) {
                std::string uri = val.get_string();
                auto resolved = resolver.resolve(uri);
                if (resolved) {
                    val = std::move(*resolved);
                }
            }
        }
    }
    for (auto& eptr : edges_) {
        for (auto& [key, val] : eptr->attributes) {
            if (val.is_ref()) {
                std::string uri = val.get_string();
                auto resolved = resolver.resolve(uri);
                if (resolved) {
                    val = std::move(*resolved);
                }
            }
        }
    }
}

inline graph graph::parse(const std::string& data, bool lenient) {
    graph g;
    parse_impl(data, g, std::string(), lenient);
    return g;
}

inline graph graph::parse(std::istream& stream, bool lenient) {
    std::stringstream ss;
    ss << stream.rdbuf();
    return parse(ss.str(), lenient);
}

inline graph graph::parse_file(const std::string& path, bool lenient) {
    std::ifstream in(path);
    if (!in) throw parse_error("Cannot open file: " + path, 0, 0);
    std::stringstream ss;
    ss << in.rdbuf();
    graph g;
    std::filesystem::path base = std::filesystem::absolute(path).parent_path();
    parse_impl(ss.str(), g, base.string(), lenient);
    return g;
}

inline void graph::save(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) throw graph_error("Cannot open file: " + filename);
    out << to_agon();
}

inline void graph::load(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) throw graph_error("Cannot open file: " + filename);
    std::stringstream ss;
    ss << in.rdbuf();
    *this = parse(ss.str());
}

// ============================= ВСПОМОГАТЕЛЬНЫЙ МЕТОД =============================
inline void graph::skip_spaces(const std::string& s, size_t& pos, size_t& line) {
    while (pos < s.size()) {
        char c = s[pos];
        if (c == ' ' || c == '\t') {
            ++pos;
        } else if (c == '/' && pos + 1 < s.size() && s[pos + 1] == '/') {
            pos += 2;
            while (pos < s.size() && s[pos] != '\n') ++pos;
        } else if (c == '/' && pos + 1 < s.size() && s[pos + 1] == '*') {
            pos += 2;
            while (pos < s.size() && !(s[pos] == '*' && pos + 1 < s.size() && s[pos + 1] == '/')) {
                if (s[pos] == '\n') break;
                ++pos;
            }
            if (pos < s.size() && s[pos] == '*' && pos + 1 < s.size() && s[pos + 1] == '/') {
                pos += 2;
            }
        } else {
            break;
        }
    }
}

inline void graph::skip_delimiters(const std::string& s, size_t& pos, size_t& line, char stop) {
    while (pos < s.size()) {
        if (s[pos] == stop) break;
        if (s[pos] == ',' || std::isspace(static_cast<unsigned char>(s[pos]))) {
            if (s[pos] == '\n') break;
            ++pos;
            skip_spaces(s, pos, line);
        } else break;
    }
}

// ============================= ПАРСЕР =============================
void graph::parse_impl(const std::string& data, graph& g, const std::string& base_dir, bool lenient) {
    size_t pos = 0, line = 1;
    while (pos < data.size()) {
        graph::skip_whitespace_and_comments(data, pos, line);
        if (pos >= data.size()) break;
        try {
            if (data[pos] == '@') {
                ++pos;
                std::string keyword;
                if (pos < data.size() && std::isalpha(static_cast<unsigned char>(data[pos]))) {
                    size_t start = pos;
                    while (pos < data.size() && (std::isalnum(static_cast<unsigned char>(data[pos])) || data[pos] == '_')) ++pos;
                    keyword = data.substr(start, pos - start);
                }
                if (keyword == "data") {
                    parse_data_block(data, pos, g, line);
                } else if (keyword == "table") {
                    parse_table_block(data, pos, g, line);
                } else if (keyword == "include") {
                    parse_include(data, pos, g, line, base_dir);
                } else if (keyword == "template") {
                    graph::skip_whitespace_and_comments(data, pos, line);
                    std::string tname;
                    if (!parse_string(data, pos, tname, line))
                        throw parse_error("Expected template name", line, pos);
                    graph::skip_whitespace_and_comments(data, pos, line);
                    if (data[pos] != '(') throw parse_error("Expected '(' for template params", line, pos);
                    ++pos;
                    std::vector<std::string> params;
                    graph::skip_whitespace_and_comments(data, pos, line);
                    while (data[pos] != ')') {
                        std::string param;
                        if (!parse_id(data, pos, param)) throw parse_error("Expected parameter name", line, pos);
                        params.push_back(param);
                        graph::skip_whitespace_and_comments(data, pos, line);
                        if (data[pos] == ',') { ++pos; graph::skip_whitespace_and_comments(data, pos, line); }
                    }
                    ++pos; // пропускаем ')'
                    graph::skip_whitespace_and_comments(data, pos, line);
                    if (data[pos] != '{') throw parse_error("Expected '{' for template body", line, pos);
                    ++pos;
                    size_t body_start = pos;
                    int brace_count = 1;
                    while (pos < data.size() && brace_count > 0) {
                        char c = data[pos];
                        if (c == '"') {
                            ++pos;
                            while (pos < data.size()) {
                                if (data[pos] == '\\' && pos+1 < data.size()) { pos += 2; continue; }
                                if (data[pos] == '"') { ++pos; break; }
                                ++pos;
                            }
                            continue;
                        }
                        if (c == '{') {
                            if (pos+1 < data.size() && data[pos+1] == '{') {
                                pos += 2;
                                continue;
                            }
                            ++brace_count;
                        } else if (c == '}') {
                            if (pos+1 < data.size() && data[pos+1] == '}') {
                                pos += 2;
                                continue;
                            }
                            --brace_count;
                            if (brace_count == 0) {
                                // тело заканчивается на этом символе '}', не включая его
                                std::string body = data.substr(body_start, pos - body_start);
                                ++pos; // пропускаем '}'
                                g.define_template(tname, params, body);
                                break;
                            }
                        }
                        ++pos;
                    }
                    if (brace_count > 0) throw parse_error("Unterminated template body", line, pos);
                } else if (keyword == "use") {
                    graph::skip_whitespace_and_comments(data, pos, line);
                    std::string tname;
                    if (!parse_string(data, pos, tname, line))
                        throw parse_error("Expected template name", line, pos);
                    graph::skip_whitespace_and_comments(data, pos, line);
                    if (data[pos] != '(') throw parse_error("Expected '('", line, pos);
                    ++pos;
                    std::unordered_map<std::string, value> args;
                    graph::skip_whitespace_and_comments(data, pos, line);
                    while (data[pos] != ')') {
                        auto [k, v] = parse_attr(data, pos, line);
                        args[k] = std::move(v);
                        graph::skip_whitespace_and_comments(data, pos, line);
                        if (data[pos] == ',') { ++pos; graph::skip_whitespace_and_comments(data, pos, line); }
                    }
                    ++pos;
                    g.instantiate_template(tname, args);
                } else if (keyword == "schema") {
                    graph::skip_whitespace_and_comments(data, pos, line);
                    if (pos < data.size() && data[pos] == '"') {
                        std::string dummy;
                        parse_string(data, pos, dummy, line);
                    } else {
                        std::string dummy;
                        parse_id(data, pos, dummy);
                    }
                    graph::skip_whitespace_and_comments(data, pos, line);
                    if (data[pos] != '{') throw parse_error("Expected '{' after schema name", line, pos);
                    ++pos;
                    int brace_count = 1;
                    while (pos < data.size() && brace_count > 0) {
                        if (data[pos] == '{') ++brace_count;
                        else if (data[pos] == '}') --brace_count;
                        ++pos;
                    }
                } else {
                    pos -= keyword.size();
                    parse_node_line(data, pos, g, line);
                }
            } else if (data[pos] == '#') {
                ++pos;
                if (pos < data.size() && data[pos] == '!') {
                    ++pos;
                    parse_metadata(data, pos, g, line);
                } else {
                    parse_edge_line(data, pos, g, line);
                }
            } else {
                throw parse_error("Unexpected character: " + std::string(1, data[pos]), line, pos);
            }
        } catch (const parse_error& e) {
            if (!lenient) throw;
            skip_to_next_line(data, pos, line);
            graph::skip_whitespace_and_comments(data, pos, line); 
        }
    }
}

void graph::skip_to_next_line(const std::string& s, size_t& pos, size_t& line) {
    while (pos < s.size() && s[pos] != '\n') ++pos;
    if (pos < s.size()) { ++pos; ++line; }
}

// ============================= include =============================
void graph::parse_include(const std::string& s, size_t& pos, graph& g, size_t& line, const std::string& base_dir) {
    graph::skip_spaces(s, pos, line);
    std::string filename;
    if (!parse_string(s, pos, filename, line))
        throw parse_error("Expected filename after @include", line, pos);
    std::filesystem::path inc_path;
    if (!base_dir.empty()) {
        inc_path = std::filesystem::path(base_dir) / filename;
    } else {
        inc_path = filename;
    }
    graph inc_graph = graph::parse_file(inc_path.string());
    for (auto n : inc_graph.node_range()) {
        if (n.id().empty()) {
            std::string type = n.type();
            std::vector<std::string> tags = n.tags();
            std::unordered_map<std::string, value> attrs;
            for (auto it = n.attributes_begin(); it != n.attributes_end(); ++it) attrs[it->first] = it->second;
            std::ostringstream body;
            body << "@" << type;
            if (!tags.empty()) { body << " ["; for (size_t i=0;i<tags.size();++i) { if(i) body<<","; body<<tags[i]; } body<<"]"; }
            if (!attrs.empty()) {
                body << " (";
                bool first = true;
                for (const auto& [k, v] : attrs) {
                    if (!first) body << ", ";
                    first = false;
                    body << k << "=" << ogon::value_to_agon_string(v);
                }
                body << ")";
            }
            g.define_template(type, {}, body.str());
        } else {
            g.add_node(n.type(), n.id(), true);
            auto new_node = g.node_by_id(n.id());
            new_node.set_tag(n.tags());
            for (auto it = n.attributes_begin(); it != n.attributes_end(); ++it) {
                new_node[it->first] = it->second;
            }
        }
    }
    for (auto e : inc_graph.edge_range()) {
        auto new_edge = g.add_edge();
        if (e.is_bidirectional()) {
            new_edge.set_bidirectional(true);
            new_edge.set_forward_type(e.forward_type());
            new_edge.set_backward_type(e.backward_type());
            for (const endpoint& ep : e.forward_endpoints()) new_edge.add_forward_endpoint(ep);
            for (const endpoint& ep : e.backward_endpoints()) new_edge.add_backward_endpoint(ep);
        } else {
            new_edge.set_forward_type(e.forward_type());
            for (const endpoint& ep : e.forward_endpoints()) new_edge.add_forward_endpoint(ep);
        }
        new_edge.set_tags(e.tags());
        for (const auto& [k, v] : e.attributes()) new_edge[k] = v;
    }
    for (auto& [k, v] : inc_graph.data_blocks_) g.data_blocks_[k] = v;
    for (auto& [k, v] : inc_graph.tables_) g.tables_[k] = v;
    for (const auto& [k, v] : inc_graph.templates_) {
        g.define_template(k, v.params, v.body);
    }
}

// ============================= data block =============================
void graph::parse_data_block(const std::string& s, size_t& pos, graph& g, size_t& line) {
    graph::skip_spaces(s, pos, line);
    std::string name;
    if (!parse_string(s, pos, name, line))
        throw parse_error("Expected data block name", line, pos);
    graph::skip_spaces(s, pos, line);
    auto val = parse_object(s, pos, line);
    g.data_blocks_[name] = val;
}

// ============================= table block =============================
void graph::parse_table_block(const std::string& s, size_t& pos, graph& g, size_t& line) {
    graph::skip_whitespace_and_comments(s, pos, line);
    std::string name;
    if (!parse_string(s, pos, name, line))
        throw parse_error("Expected table name", line, pos);
    graph::skip_whitespace_and_comments(s, pos, line);
    if (s[pos] != '{') throw parse_error("Expected '{'", line, pos);
    ++pos;
    table_block table;
    table.name = name;
    while (pos < s.size()) {
        graph::skip_whitespace_and_comments(s, pos, line);
        if (s[pos] == '}') { ++pos; break; }
        std::string key;
        if (!parse_id(s, pos, key)) throw parse_error("Expected 'columns' or 'rows'", line, pos);
        graph::skip_whitespace_and_comments(s, pos, line);
        if (s[pos] != '=') throw parse_error("Expected '='", line, pos);
        ++pos;
        if (key == "columns") {
            auto arr = parse_value(s, pos, line);
            if (!arr.is_array()) throw parse_error("columns must be array", line, pos);
            for (size_t i = 0; i < arr.size(); ++i)
                table.columns.push_back(arr[i].get_string());
        } else if (key == "rows") {
            auto arr = parse_value(s, pos, line);
            if (!arr.is_array()) throw parse_error("rows must be array", line, pos);
            for (size_t i = 0; i < arr.size(); ++i) {
                const auto& row_val = arr[i];
                if (!row_val.is_array()) throw parse_error("each row must be array", line, pos);
                std::vector<value> row;
                for (size_t j = 0; j < row_val.size(); ++j)
                    row.push_back(row_val[j]);
                table.rows.push_back(std::move(row));
            }
        } else {
            throw parse_error("Unknown table key: " + key, line, pos);
        }
        graph::skip_whitespace_and_comments(s, pos, line);
        if (s[pos] == ',') { ++pos; continue; }
    }
    g.tables_[name] = std::move(table);
}

// ============================= NODE =============================
void graph::parse_node_line(const std::string& s, size_t& pos, graph& g, size_t& line) {
    std::string type;
    if (!parse_id(s, pos, type))
        throw parse_error("Expected node type", line, pos);
    graph::skip_spaces(s, pos, line);

    std::string id;
    bool has_id = false;
    if (pos < s.size() && s[pos] == '"') {
        has_id = parse_string(s, pos, id, line);
    } else {
        has_id = parse_id_or_number(s, pos, id);
    }
    graph::skip_spaces(s, pos, line);

    std::vector<std::string> tags;
    while (pos < s.size() && s[pos] == '[') {
        ++pos;
        graph::skip_spaces(s, pos, line);
        while (pos < s.size() && s[pos] != ']') {
            std::string tag;
            if (!parse_id(s, pos, tag))
                throw parse_error("Expected tag", line, pos);
            tags.push_back(tag);
            graph::skip_spaces(s, pos, line);
            if (s[pos] == ',') { ++pos; graph::skip_spaces(s, pos, line); }
        }
        if (pos < s.size() && s[pos] == ']') ++pos;
        else throw parse_error("Expected closing ']'", line, pos);
        graph::skip_spaces(s, pos, line);
    }

    std::unordered_map<std::string, value> attrs;
    if (pos < s.size() && s[pos] == '(') {
        ++pos;
        graph::skip_spaces(s, pos, line);
        while (pos < s.size() && s[pos] != ')') {
            auto [key, val] = parse_attr(s, pos, line);
            attrs[key] = std::move(val);
            skip_delimiters(s, pos, line, ')');
        }
        if (pos < s.size() && s[pos] == ')') ++pos;
        else throw parse_error("Expected closing ')'", line, pos);
    }

    // Теги после атрибутов
    graph::skip_spaces(s, pos, line);
    while (pos < s.size() && s[pos] == '[') {
        ++pos;
        graph::skip_spaces(s, pos, line);
        while (pos < s.size() && s[pos] != ']') {
            std::string tag;
            if (!parse_id(s, pos, tag))
                throw parse_error("Expected tag", line, pos);
            tags.push_back(tag);
            graph::skip_spaces(s, pos, line);
            if (s[pos] == ',') { ++pos; graph::skip_spaces(s, pos, line); }
        }
        if (pos < s.size() && s[pos] == ']') ++pos;
        else throw parse_error("Expected closing ']'", line, pos);
        graph::skip_spaces(s, pos, line);
    }

    if (!has_id) {
        std::ostringstream body;
        body << "@" << type;
        if (!tags.empty()) {
            body << " [";
            for (size_t i = 0; i < tags.size(); ++i) { if (i) body << ","; body << tags[i]; }
            body << "]";
        }
        if (!attrs.empty()) {
            body << " (";
            bool first = true;
            for (const auto& [k, v] : attrs) {
                if (!first) body << ", ";
                first = false;
                body << k << "=" << ogon::value_to_agon_string(v);
            }
            body << ")";
        }
        g.define_template(type, {}, body.str());
        return;
    }

    auto nd = g.add_node(type, id, true);
    nd.set_tag(tags);
    for (auto& [k, v] : attrs) nd[k] = v;
}

// ============================= EDGE =============================
void graph::parse_edge_line(const std::string& s, size_t& pos, graph& g, size_t& line) {
    auto left_eps = parse_endpoint_list(s, pos, line);
    graph::skip_spaces(s, pos, line);
    bool bidirectional = false;
    std::string type_forward, type_backward;
    int left_in_port = 0, right_in_port = 0;

    if (pos < s.size() && s[pos] == '<') {
        bidirectional = true;
        ++pos;
        graph::skip_spaces(s, pos, line);
        if (!parse_id(s, pos, type_backward))
            throw parse_error("Expected type before '|'", line, pos);
        if (pos < s.size() && s[pos] == ':') {
            ++pos;
            std::string port_str;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) port_str += s[pos++];
            if (port_str.empty()) throw parse_error("Expected port number", line, pos);
            left_in_port = std::stoi(port_str);
        }
        graph::skip_spaces(s, pos, line);
        if (s[pos] != '|') throw parse_error("Expected '|'", line, pos);
        ++pos;
        graph::skip_spaces(s, pos, line);
        if (!parse_id(s, pos, type_forward))
            throw parse_error("Expected type after '|'", line, pos);
        if (pos < s.size() && s[pos] == ':') {
            ++pos;
            std::string port_str;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) port_str += s[pos++];
            if (port_str.empty()) throw parse_error("Expected port number", line, pos);
            right_in_port = std::stoi(port_str);
        }
        graph::skip_spaces(s, pos, line);
        if (s[pos] != '>') throw parse_error("Expected '>'", line, pos);
        ++pos;
    } else {
        if (pos < s.size() && (std::isalpha(static_cast<unsigned char>(s[pos])) || s[pos] == '_')) {
            if (!parse_id(s, pos, type_forward))
                throw parse_error("Expected type or '>'", line, pos);
        }
        graph::skip_spaces(s, pos, line);
        if (s[pos] != '>') throw parse_error("Expected '>'", line, pos);
        ++pos;
    }
    graph::skip_spaces(s, pos, line);
    auto right_eps = parse_endpoint_list(s, pos, line);

    auto e = g.add_edge();
    e.set_bidirectional(bidirectional);

    if (bidirectional) {
        auto fwd_left = left_eps;
        auto fwd_right = right_eps;
        for (auto& ep : fwd_left)
            if (ep.role.empty()) ep.role = "out";
        for (auto& ep : fwd_right) {
            if (ep.role.empty()) ep.role = "in";
            if (right_in_port != 0) ep.port = right_in_port;
        }
        for (const auto& ep : fwd_left) e.add_forward_endpoint(ep);
        for (const auto& ep : fwd_right) e.add_forward_endpoint(ep);
        e.set_forward_type(type_forward);

        auto bwd_left = left_eps;
        auto bwd_right = right_eps;
        for (auto& ep : bwd_right)
            if (ep.role.empty()) ep.role = "out";
        for (auto& ep : bwd_left) {
            if (ep.role.empty()) ep.role = "in";
            if (left_in_port != 0) ep.port = left_in_port;
        }
        for (const auto& ep : bwd_right) e.add_backward_endpoint(ep);
        for (const auto& ep : bwd_left) e.add_backward_endpoint(ep);
        e.set_backward_type(type_backward);
    } else {
        auto fwd_left = left_eps;
        auto fwd_right = right_eps;
        for (auto& ep : fwd_left)
            if (ep.role.empty()) ep.role = "out";
        for (auto& ep : fwd_right)
            if (ep.role.empty()) ep.role = "in";
        for (const auto& ep : fwd_left) e.add_forward_endpoint(ep);
        for (const auto& ep : fwd_right) e.add_forward_endpoint(ep);
        e.set_forward_type(type_forward);
    }

    parse_edge_attrs(s, pos, e, line);
    graph::skip_spaces(s, pos, line);
    while (pos < s.size() && s[pos] == '[') {
        ++pos;
        std::vector<std::string> tags;
        graph::skip_spaces(s, pos, line);
        while (pos < s.size() && s[pos] != ']') {
            std::string tag;
            if (!parse_id(s, pos, tag))
                throw parse_error("Expected tag", line, pos);
            tags.push_back(tag);
            graph::skip_spaces(s, pos, line);
            if (s[pos] == ',') { ++pos; graph::skip_spaces(s, pos, line); }
        }
        if (pos < s.size() && s[pos] == ']') ++pos;
        else throw parse_error("Expected ']'", line, pos);
        graph::skip_spaces(s, pos, line);
        auto current_tags = e.tags();
        current_tags.insert(current_tags.end(), tags.begin(), tags.end());
        e.set_tags(current_tags);
    }
}

// ============================= endpoint list =============================
std::vector<endpoint> graph::parse_endpoint_list(const std::string& s, size_t& pos, size_t& line) {
    std::vector<endpoint> res;
    graph::skip_spaces(s, pos, line);
    if (pos >= s.size()) throw parse_error("Unexpected end of input in edge", line, pos);
    bool in_parens = false;
    if (s[pos] == '(') {
        in_parens = true;
        ++pos;
        graph::skip_spaces(s, pos, line);
        if (s[pos] == ')') { ++pos; return res; }
        while (pos < s.size() && s[pos] != ')') {
            endpoint ep;
            if (!parse_single_endpoint(s, pos, ep, line))
                throw parse_error("Expected endpoint", line, pos);
            res.push_back(ep);
            graph::skip_spaces(s, pos, line);
            if (s[pos] == ',') { ++pos; graph::skip_spaces(s, pos, line); }
        }
        if (pos < s.size() && s[pos] == ')') ++pos;
        else throw parse_error("Expected ')'", line, pos);
    } else {
        endpoint ep;
        if (!parse_single_endpoint(s, pos, ep, line))
            throw parse_error("Expected endpoint", line, pos);
        res.push_back(ep);
    }
    return res;
}

bool graph::parse_single_endpoint(const std::string& s, size_t& pos, endpoint& ep, size_t& line) {
    std::string id;
    if (!parse_id_or_number(s, pos, id)) return false;
    ep.node_id = id;
    if (pos < s.size() && s[pos] == ':') {
        ++pos;
        int port = 0;
        size_t start = pos;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        if (start == pos) throw parse_error("Expected port number", line, pos);
        port = std::stoi(s.substr(start, pos - start));
        ep.port = port;
    }
    if (pos < s.size() && s[pos] == '@') {
        ++pos;
        std::string role;
        if (!parse_id(s, pos, role)) throw parse_error("Expected role after @", line, pos);
        ep.role = role;
    }
    return true;
}

// ============================= edge attrs =============================
void graph::parse_edge_attrs(const std::string& s, size_t& pos, hyperedge& e, size_t& line) {
    graph::skip_spaces(s, pos, line);
    if (pos < s.size() && s[pos] == '(') {
        ++pos;
        graph::skip_spaces(s, pos, line);
        while (pos < s.size() && s[pos] != ')') {
            auto [k, v] = parse_attr(s, pos, line);
            e[k] = std::move(v);
            skip_delimiters(s, pos, line, ')');
        }
        if (pos < s.size() && s[pos] == ')') ++pos;
        else throw parse_error("Expected ')'", line, pos);
    }
}

// ============================= metadata =============================
void graph::parse_metadata(const std::string& s, size_t& pos, graph& g, size_t& line) {
    graph::skip_whitespace_and_comments(s, pos, line);
    std::string key;
    if (!parse_id(s, pos, key)) throw parse_error("Expected metadata key", line, pos);
    graph::skip_whitespace_and_comments(s, pos, line);
    std::string value;
    size_t start = pos;
    while (pos < s.size() && s[pos] != '\n' && !(s[pos]=='/' && pos+1<s.size() && (s[pos+1]=='/' || s[pos+1]=='*')))
        ++pos;
    value = s.substr(start, pos - start);
    while (!value.empty() && std::isspace(value.back())) value.pop_back();
    g.set_metadata(key, value);
}

// ============================= базовые парсеры =============================
bool graph::parse_id(const std::string& s, size_t& pos, std::string& out) {
    if (pos >= s.size()) return false;
    if (!std::isalpha(static_cast<unsigned char>(s[pos])) && s[pos] != '_') return false;
    size_t start = pos;
    while (pos < s.size() && (std::isalnum(static_cast<unsigned char>(s[pos])) || s[pos] == '_')) ++pos;
    out = s.substr(start, pos - start);
    return true;
}

bool graph::parse_id_or_number(const std::string& s, size_t& pos, std::string& out) {
    if (pos >= s.size()) return false;
    if (std::isdigit(static_cast<unsigned char>(s[pos])) || s[pos] == '-') {
        size_t start = pos;
        if (s[pos] == '-') ++pos;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        out = s.substr(start, pos - start);
        return true;
    }
    return parse_id(s, pos, out);
}

value graph::parse_value(const std::string& s, size_t& pos, size_t& line) {
    graph::skip_whitespace_and_comments(s, pos, line);
    if (pos >= s.size()) throw parse_error("Expected value", line, pos);
    char c = s[pos];
    if (c == '"') {
        std::string str;
        if (!parse_string(s, pos, str, line)) throw parse_error("Failed to parse string", line, pos);
        if (str.compare(0, 7, "BASE64:") == 0) {
            try {
                auto binary = base64_decode(str.substr(7));
                return value(binary);
            } catch (...) {
                throw parse_error("Invalid Base64 data", line, pos);
            }
        }
        if (str.compare(0, 6, "ref://") == 0 || str.compare(0, 6, "var://") == 0 ||
            str.compare(0, 7, "asset://") == 0 || str.compare(0, 5, "file://") == 0 ||
            str.compare(0, 7, "bundle://") == 0 || str.compare(0, 5, "http://") == 0 ||
            str.compare(0, 6, "https://") == 0)
            return value(str, value_t::ref);
        return value(str);
    } else if (c == 'B') {
        if (s.compare(pos, 7, "BASE64:") == 0) {
            pos += 7;
            std::string base64_str;
            while (pos < s.size() && (std::isalnum(static_cast<unsigned char>(s[pos])) || s[pos] == '+' || s[pos] == '/' || s[pos] == '=')) {
                base64_str += s[pos++];
            }
            try {
                auto binary = base64_decode(base64_str);
                return value(binary);
            } catch (...) {
                throw parse_error("Invalid Base64 data", line, pos);
            }
        }
        throw parse_error("Unknown literal starting with B", line, pos);
    } else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        return parse_number(s, pos, line);
    } else if (c == 't' || c == 'f' || c == 'n') {
        return parse_literal(s, pos, line);
    } else if (c == '[') {
        return parse_array(s, pos, line);
    } else if (c == '{') {
        return parse_object(s, pos, line);
    } else throw parse_error("Unexpected character in value", line, pos);
}

bool graph::parse_string(const std::string& s, size_t& pos, std::string& out, size_t& line) {
    if (s[pos] != '"') return false;
    ++pos;
    out.clear();
    while (pos < s.size()) {
        char c = s[pos++];
        if (c == '"') return true;
        if (c == '\\' && pos < s.size()) {
            char esc = s[pos++];
            switch (esc) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                default: out += esc; break;
            }
        } else {
            if (c == '\n') ++line;
            out += c;
        }
    }
    throw parse_error("Unterminated string", line, pos);
}

value graph::parse_number(const std::string& s, size_t& pos, size_t& line) {
    size_t start = pos;
    bool is_float = false;
    while (pos < s.size() && (std::isdigit(static_cast<unsigned char>(s[pos])) || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' || s[pos] == '+' || s[pos] == '-')) {
        if (s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E') is_float = true;
        ++pos;
    }
    std::string num = s.substr(start, pos - start);
    try {
        if (is_float) return value(std::stod(num));
        else return value(static_cast<int64_t>(std::stoll(num)));
    } catch (...) {
        throw parse_error("Invalid number", line, start);
    }
}

value graph::parse_literal(const std::string& s, size_t& pos, size_t& line) {
    if (s.compare(pos, 4, "true") == 0) { pos += 4; return value(true); }
    if (s.compare(pos, 5, "false") == 0) { pos += 5; return value(false); }
    if (s.compare(pos, 4, "null") == 0) { pos += 4; return value(); }
    throw parse_error("Invalid literal", line, pos);
}

value graph::parse_array(const std::string& s, size_t& pos, size_t& line) {
    if (s[pos] != '[') throw parse_error("Expected '['", line, pos);
    ++pos;
    graph::skip_whitespace_and_comments(s, pos, line);
    value::array_t arr;
    if (s[pos] == ']') { ++pos; return value(arr); }
    while (true) {
        arr.push_back(parse_value(s, pos, line));
        graph::skip_whitespace_and_comments(s, pos, line);
        if (s[pos] == ']') { ++pos; return value(std::move(arr)); }
        if (s[pos] == ',') { ++pos; graph::skip_whitespace_and_comments(s, pos, line); continue; }
        throw parse_error("Expected ',' or ']'", line, pos);
    }
}

value graph::parse_object(const std::string& s, size_t& pos, size_t& line) {
    if (s[pos] != '{') throw parse_error("Expected '{'", line, pos);
    ++pos;
    graph::skip_whitespace_and_comments(s, pos, line);
    value::object_t obj;
    if (s[pos] == '}') { ++pos; return value(obj); }
    while (true) {
        auto [k, v] = parse_attr(s, pos, line);
        obj[k] = std::move(v);
        while (pos < s.size() && (s[pos] == ',' || std::isspace(static_cast<unsigned char>(s[pos])))) {
            if (s[pos] == ',') ++pos;
            else ++pos;
            graph::skip_whitespace_and_comments(s, pos, line);
        }
        if (pos >= s.size()) throw parse_error("Unexpected end of object", line, pos);
        if (s[pos] == '}') { ++pos; return value(std::move(obj)); }
    }
}

std::pair<std::string, value> graph::parse_attr(const std::string& s, size_t& pos, size_t& line) {
    std::string key;
    if (s[pos] == '"') {
        if (!parse_string(s, pos, key, line)) throw parse_error("Expected attribute key", line, pos);
    } else {
        if (!parse_id(s, pos, key)) throw parse_error("Expected attribute key", line, pos);
    }
    graph::skip_spaces(s, pos, line);
    if (s[pos] != '=') throw parse_error("Expected '='", line, pos);
    ++pos;
    value val = parse_value(s, pos, line);
    return {key, std::move(val)};
}

void graph::skip_whitespace_and_comments(const std::string& s, size_t& pos, size_t& line) {
    while (pos < s.size()) {
        char c = s[pos];
        if (c == '\n') { ++line; ++pos; }
        else if (std::isspace(static_cast<unsigned char>(c))) ++pos;
        else if (c == '/' && pos+1 < s.size() && s[pos+1] == '/') {
            pos += 2;
            while (pos < s.size() && s[pos] != '\n') ++pos;
        } else if (c == '/' && pos+1 < s.size() && s[pos+1] == '*') {
            pos += 2;
            while (pos+1 < s.size() && !(s[pos]=='*' && s[pos+1]=='/')) {
                if (s[pos] == '\n') ++line;
                ++pos;
            }
            if (pos+1 < s.size()) pos += 2;
        } else break;
    }
}

// ---------- Сериализация хелперов ----------
inline void graph::serialize_node(const node::impl& n, std::ostringstream& out) {
    out << "@" << n.type << " " << n.id;
    if (!n.tags.empty()) {
        out << " [";
        for (size_t i = 0; i < n.tags.size(); ++i) {
            if (i) out << ", ";
            out << n.tags[i];
        }
        out << "]";
    }
    if (!n.attributes.empty()) {
        out << " (";
        bool first = true;
        for (const auto& [k, v] : n.attributes) {
            if (!first) out << ", ";
            first = false;
            out << k << "=" << ogon::value_to_agon_string(v);
        }
        out << ")";
    }
}

inline void graph::serialize_edge(const hyperedge& he, std::ostringstream& out) {
    out << "# ";
    if (he.is_bidirectional()) {
        std::vector<endpoint> left_out, right_in;
        for (const endpoint& ep : he.forward_endpoints()) {
            if (ep.role == "out") left_out.push_back(ep);
            else if (ep.role == "in") right_in.push_back(ep);
        }
        out << format_eps(left_out);
        out << " <" << he.backward_type();
        int left_port = 0;
        for (const endpoint& ep : he.backward_endpoints()) {
            if (ep.role == "in") { left_port = ep.port; break; }
        }
        if (left_port != 0) out << ":" << left_port;
        out << "|" << he.forward_type();
        int right_port = 0;
        for (const endpoint& ep : he.forward_endpoints()) {
            if (ep.role == "in") { right_port = ep.port; break; }
        }
        if (right_port != 0) out << ":" << right_port;
        out << "> " << format_eps(right_in);
    } else {
        std::vector<endpoint> left_out, right_in;
        for (const endpoint& ep : he.forward_endpoints()) {
            if (ep.role == "out") left_out.push_back(ep);
            else if (ep.role == "in") right_in.push_back(ep);
        }
        out << format_eps(left_out);
        if (!he.forward_type().empty()) out << " " << he.forward_type();
        out << "> " << format_eps(right_in);
    }
    if (!he.attributes().empty()) {
        out << " (";
        bool first = true;
        for (const auto& [k, v] : he.attributes()) {
            if (!first) out << ", ";
            first = false;
            out << k << "=" << ogon::value_to_agon_string(v);
        }
        out << ")";
    }
    if (!he.tags().empty()) {
        out << " [";
        for (size_t i = 0; i < he.tags().size(); ++i) {
            if (i) out << ", ";
            out << he.tags()[i];
        }
        out << "]";
    }
}

inline std::string graph::format_eps(const std::vector<endpoint>& eps) {
    if (eps.empty()) return "";
    if (eps.size() == 1) {
        const endpoint& ep = eps[0];
        std::string res = ep.node_id;
        if (ep.port != 0) res += ":" + std::to_string(ep.port);
        if (!ep.role.empty() && ep.role != "out" && ep.role != "in")
            res += "@" + ep.role;
        return res;
    }
    std::string res = "(";
    for (size_t i = 0; i < eps.size(); ++i) {
        if (i) res += ", ";
        const endpoint& ep = eps[i];
        res += ep.node_id;
        if (ep.port != 0) res += ":" + std::to_string(ep.port);
        if (!ep.role.empty() && ep.role != "out" && ep.role != "in")
            res += "@" + ep.role;
    }
    res += ")";
    return res;
}

} // namespace ogon