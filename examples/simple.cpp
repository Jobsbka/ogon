#include <ogon.hpp>
#include <iostream>
#include <sstream>

int main() {
    using namespace ogon;

    try {
        graph g;
        g.set_metadata("author", "ArxTech");
        g.set_metadata("version", "1.1");

        auto mesh = g.add_node("Mesh", "1");
        mesh["source"] = value(std::string("asset://models/hero.fbx"), value_t::ref);

        auto player = g.add_node("Player", "5");
        player["health"] = 100;
        value pos = value::object_t{};
        pos["x"] = 0; pos["y"] = 0; pos["z"] = 10;
        player["position"] = pos;

        auto texture = g.add_node("Texture", "tex1");
        std::vector<uint8_t> raw_png = {0x89, 0x50, 0x4E, 0x47};
        texture["data"] = value(raw_png);

        auto marker = g.add_node("Marker", "m1");
        marker.set_tag({"EditorHidden", "Debug"});
        marker["note"] = std::string("checkpoint");

        auto e_bidir = g.add_edge();
        e_bidir.set_bidirectional(true);
        e_bidir.set_forward_type("sync");
        e_bidir.set_backward_type("sync");
        e_bidir.add_forward_endpoint(endpoint("1", 0, "out"));
        e_bidir.add_forward_endpoint(endpoint("5", 2, "in"));
        e_bidir.add_backward_endpoint(endpoint("5", 0, "out"));
        e_bidir.add_backward_endpoint(endpoint("1", 2, "in"));
        e_bidir.set_tags({"important", "low_latency"});

        value physics = value::object_t{};
        physics["gravity"] = -9.81;
        physics["substeps"] = 4;
        g.set_data_block("PhysicsSettings", physics);

        table_block loot;
        loot.name = "LootTable";
        loot.columns = {"Item", "Weight", "MinLevel"};
        loot.rows = {
            {value(std::string("Sword")), value(10), value(5)},
            {value(std::string("Shield")), value(5), value(1)},
            {value(std::string("Potion")), value(20), value(0)}
        };
        g.set_table("LootTable", loot);

        g.define_template("EnemySpawner", {"type", "count"},
            R"(@Enemy "{{type}}_{{count}}" (species="{{type}}", count={{count}}) [AI])");
        std::unordered_map<std::string, value> args;
        args["type"] = std::string("Orc");
        args["count"] = 3;
        g.instantiate_template("EnemySpawner", args);

        std::cout << g.to_agon(true) << std::endl;

        std::string stream_data = R"(@event "Connected" (id=42)
*||*
@event "Disconnected" (id=42)
)";
        std::stringstream ss(stream_data);
        message_stream ms(ss);
        graph msg;
        while (ms.read_next(msg)) {
            std::cout << "Message:\n" << msg.to_agon() << std::endl;
        }

        // Исправленная схема: теперь это валидный блок @schema
        std::string schema_body = R"(
node "Constant" {
    value: int required
    @optional tag: string
}
)";
        type_registry registry = parse_schema_block(schema_body);
        const schema_node_def* const_schema = registry.get_node_schema("Constant");
        if (const_schema) {
            std::cout << "Schema for Constant loaded." << std::endl;
        }

        map_resolver resolver;
        resolver.add_handler("ref://", [](const std::string& uri) -> std::optional<value> {
            return value(std::string("resolved:" + uri));
        });
        g.resolve_references(resolver);
        std::cout << "Resolved mesh source: " << mesh["source"].get_string() << std::endl;

    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}