#pragma once

#include <stdexcept>
#include <string>

namespace ogon {

class parse_error : public std::runtime_error {
public:
    parse_error(const std::string& msg, size_t line, size_t col)
        : std::runtime_error(
              "OGON parse error at line " + std::to_string(line) +
              ", column " + std::to_string(col) + ": " + msg),
          line_(line), col_(col) {}

    size_t line() const noexcept { return line_; }
    size_t column() const noexcept { return col_; }

private:
    size_t line_;
    size_t col_;
};

class type_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class graph_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}