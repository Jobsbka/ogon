#pragma once

// Main header for OGON library

#include <ogon/value.hpp>
#include <ogon/node.hpp>
#include <ogon/hyperedge.hpp>
#include <ogon/graph.hpp>
#include <ogon/error.hpp>
#include <ogon/literals.hpp>
#include <ogon/message_stream.hpp>
#include <ogon/base64.hpp>
#include <ogon/resolver.hpp>
#include <ogon/template.hpp>
#include <ogon/schema_parser.hpp>
#include <ogon/value_serialization.hpp> 

namespace ogon {
// Basic version info
inline const char* version() noexcept { return "1.0.0"; }
}