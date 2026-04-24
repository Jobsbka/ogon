#pragma once

#include <ogon/graph.hpp>
#include <string>
#include <istream>
#include <sstream>

namespace ogon {

class message_stream {
public:
    explicit message_stream(std::istream& input) : input_(input) {}

    // Читает следующий фрейм до разделителя "*||*" и парсит как граф
    bool read_next(graph& g) {
        std::string line;
        std::ostringstream frame;
        while (std::getline(input_, line)) {
            // Убираем возможный '\r' в конце строки (Windows)
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line == "*||*") {
                break;
            }
            frame << line << '\n';
        }
        std::string data = frame.str();
        if (data.empty()) return false;
        try {
            g = graph::parse(data);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool eof() const { return input_.eof(); }

private:
    std::istream& input_;
};

} // namespace ogon