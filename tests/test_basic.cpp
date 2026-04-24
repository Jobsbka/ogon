// tests/test_basic.cpp
#include <ogon.hpp>
#include <iostream>
#include <cassert>

int main() {
    using namespace ogon;

    try {
        // 1. Build graph manually
        std::cout << "1. Building graph..." << std::endl;
        graph g;
        g.set_metadata("author", "Test");

        auto constant = g.add_node("Constant", "1");
        constant["value"] = 42;

        auto variable = g.add_node("Variable", "2");
        variable["name"] = "x";

        auto edge = g.connect("1", "2", "dataflow");
        std::string ag = g.to_agon();
        std::cout << "Serialized:\n" << ag << std::endl;

        // 2. Parse back
        std::cout << "2. Parsing..." << std::endl;
        graph g2 = graph::parse(ag);

        // 3. Verify nodes
        std::cout << "3. Verifying nodes..." << std::endl;
        assert(g2.node_by_id("1").valid());
        assert(g2.node_by_id("2").valid());
        assert(g2.node_by_id("1").type() == "Constant");
        assert(g2.node_by_id("1")["value"].get_int64() == 42);
        assert(g2.node_by_id("2")["name"].get_string() == "x");
        std::cout << "   nodes OK" << std::endl;

        // 4. Verify edges
        std::cout << "4. Verifying edges..." << std::endl;
        bool edge_found = false;
        for (auto e : g2.edge_range()) {
            if (e.forward_type() == "dataflow") {
                auto& fwd = e.forward_endpoints();
                assert(fwd.size() == 2);
                assert(fwd[0].node_id == "1" && fwd[0].role == "out");
                assert(fwd[1].node_id == "2" && fwd[1].role == "in");
                edge_found = true;
            }
        }
        assert(edge_found);
        std::cout << "   edges OK" << std::endl;

        // 5. Value types
        std::cout << "5. Value types..." << std::endl;
        value v_int(10);
        assert(v_int.is_int());
        value v_arr = {1,2,3};
        assert(v_arr.is_array());
        assert(v_arr.size() == 3);
        std::cout << "   values OK" << std::endl;

        // 6. String escaping roundtrip
        std::cout << "6. String escaping..." << std::endl;
        graph g3;
        auto n = g3.add_node("Test", "t1");
        n["path"] = std::string("C:\\folder\\file");
        n["quote"] = std::string("He said \"hello\"");
        std::string ag3 = g3.to_agon();
        graph g4 = graph::parse(ag3);
        assert(g4.node_by_id("t1")["path"].get_string() == "C:\\folder\\file");
        assert(g4.node_by_id("t1")["quote"].get_string() == "He said \"hello\"");
        std::cout << "   strings OK" << std::endl;

        // 7. Binary BASE64 roundtrip
        std::cout << "7. Binary data..." << std::endl;
        std::vector<uint8_t> bin_data = {0x89, 0x50, 0x4E, 0x47};
        value v_bin(bin_data);
        assert(v_bin.is_binary());
        std::string bin_serialized = ogon::value_to_agon_string(v_bin);
        assert(bin_serialized.find("BASE64:") == 0);
        // Парсинг через граф
        graph g_bin = graph::parse("@Img 1 (data=" + bin_serialized + ")");
        auto img = g_bin.node_by_id("1");
        assert(img["data"].is_binary());
        const auto& bin_parsed = img["data"].get_binary();
        assert(bin_parsed == bin_data);
        std::cout << "   binary OK" << std::endl;

        // 8. Templates
        std::cout << "8. Templates..." << std::endl;
        graph g5;
        g5.define_template("MyNode2", {"name"},
                           R"(@Node "n1" (label="{{name}}"))");
        g5.instantiate_template("MyNode2", {{"name", std::string("hello")}});
        auto node_n1 = g5.node_by_id("n1");
        assert(node_n1.valid());
        assert(node_n1["label"].get_string() == "hello");
        std::cout << "   templates OK" << std::endl;

        // 9. Multiple separate tag brackets
        std::cout << "9. Multiple tags..." << std::endl;
        graph g6 = graph::parse(R"(@Foo 1 [Editor] [Hidden] (x=5))");
        auto foo = g6.node_by_id("1");
        assert(foo.tags().size() == 2);
        assert(foo.tags()[0] == "Editor" && foo.tags()[1] == "Hidden");
        std::cout << "   tags OK" << std::endl;

        // 10. Lenient parsing (полная копия из test_full_spec)
        std::cout << "10. Lenient parsing..." << std::endl;
        {
            std::string broken = R"(
@Good 1
@Bad (oops
@Good 2 (value=42)
@AlsoBad missing id
# 1 broken> 2
@Good 3
)";
            auto g = graph::parse(broken, true);
            // Должны быть Good 1, Good 2, Good 3
            assert(g.node_by_id("1").valid());
            assert(g.node_by_id("2").valid());
            assert(g.node_by_id("2")["value"].get_int64() == 42);
            assert(g.node_by_id("3").valid());
        }
        std::cout << "   lenient OK" << std::endl;

        std::cout << "All basic tests passed (including new features)!" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        return 2;
    }
}