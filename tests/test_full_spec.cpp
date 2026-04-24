#include <ogon.hpp>
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
    using namespace ogon;

    try {
        // 1. Простой @data
        std::cout << "Test 1: @data block..." << std::endl;
        {
            auto g = graph::parse(R"(@data "Settings" { volume = 0.8, fullscreen = true })");
            auto* data = g.get_data_block("Settings");
            assert(data != nullptr);
            assert((*data)["volume"].get_double() == 0.8);
            assert((*data)["fullscreen"].get_bool() == true);
        }
        std::cout << "  passed." << std::endl;

        // 2. @data и узел
        std::cout << "Test 2: @data + node..." << std::endl;
        {
            auto g = graph::parse(R"(@data "Test" { x = 5 }
@Node 1 (type="light"))");
            auto* data = g.get_data_block("Test");
            assert(data != nullptr);
            assert((*data)["x"].get_int64() == 5);
            auto node1 = g.node_by_id("1");
            assert(node1.valid());
            assert(node1["type"].get_string() == "light");
        }
        std::cout << "  passed." << std::endl;

        // 3. #!version + @data + node
        std::cout << "Test 3: #!version + @data + node..." << std::endl;
        {
            auto g = graph::parse(R"(#!version 1.0
@data "Settings" { volume = 0.8, fullscreen = true }
@Node 1 (type="light")
)");
            assert(g.get_metadata("version") == "1.0");
            auto* data = g.get_data_block("Settings");
            assert(data != nullptr);
            assert((*data)["volume"].get_double() == 0.8);
            assert(g.node_by_id("1").valid());
        }
        std::cout << "  passed." << std::endl;

        // 4. @table
        std::cout << "Test 4: @table..." << std::endl;
        {
            auto g = graph::parse(R"(
@table "Items" {
    columns = ["Name", "Price"]
    rows = [ ["Axe", 10], ["Shield", 5] ]
}
)");
            auto* tbl = g.get_table("Items");
            assert(tbl != nullptr);
            assert(tbl->columns.size() == 2);
            assert(tbl->columns[0] == "Name");
            assert(tbl->rows.size() == 2);
            assert(tbl->rows[0][0].get_string() == "Axe");
            assert(tbl->rows[0][1].get_int64() == 10);
        }
        std::cout << "  passed." << std::endl;

        // 5. bidirectional edge with ports and tags
        std::cout << "Test 5: bidirectional edge..." << std::endl;
        {
            auto g = graph::parse(R"(@A 1
@B 2
# 1:1 <sync:2|sync:3> 2:2 [fast]
)");
            bool found = false;
            for (auto e : g.edge_range()) {
                if (e.is_bidirectional()) {
                    found = true;
                    assert(e.tags().size() == 1 && e.tags()[0] == "fast");
                    assert(e.forward_type() == "sync");
                    assert(e.backward_type() == "sync");

                    const auto& fwd = e.forward_endpoints();
                    assert(fwd.size() == 2);
                    assert(fwd[0].node_id == "1" && fwd[0].role == "out" && fwd[0].port == 1);
                    assert(fwd[1].node_id == "2" && fwd[1].role == "in" && fwd[1].port == 3);

                    const auto& bwd = e.backward_endpoints();
                    assert(bwd.size() == 2);
                    assert(bwd[0].node_id == "2" && bwd[0].role == "out" && bwd[0].port == 2);
                    assert(bwd[1].node_id == "1" && bwd[1].role == "in" && bwd[1].port == 2);
                    break;
                }
            }
            assert(found);
        }
        std::cout << "  passed." << std::endl;

        // 6. message stream
        std::cout << "Test 6: message stream..." << std::endl;
        {
            std::string stream = R"(@event "Jump" (height=2.5)
*||*
@event "Land" (surface="grass")
)";
            std::stringstream ss(stream);
            message_stream ms(ss);
            graph msg1, msg2;
            assert(ms.read_next(msg1));
            assert(msg1.node_by_id("Jump").valid());
            assert(msg1.node_by_id("Jump")["height"].get_double() == 2.5);
            assert(ms.read_next(msg2));
            assert(msg2.node_by_id("Land").valid());
            assert(msg2.node_by_id("Land")["surface"].get_string() == "grass");
            assert(!ms.read_next(msg2));
        }
        std::cout << "  passed." << std::endl;

        // ================== NEW EXTENDED TESTS ==================

        // 7. BASE64 binary data
        std::cout << "Test 7: BASE64 binary..." << std::endl;
        {
            auto g = graph::parse(R"(@Img 1 (data=BASE64:iVBORw0KGgo=))");  // минимальная строка Base64
            auto img = g.node_by_id("1");
            assert(img["data"].is_binary());
            const auto& bin = img["data"].get_binary();
            // iVBORw0KGgo= декодируется в \x89\x50\x4E\x47\x0D\x0A\x1A\x0A
            assert(bin.size() == 8);
            assert(bin[0] == 0x89 && bin[1] == 0x50 && bin[2] == 0x4E && bin[3] == 0x47);
        }
        std::cout << "  passed." << std::endl;

        // 8. Templates: @template / @use
        std::cout << "Test 8: Templates..." << std::endl;
        {
            std::string source = R"(
@template "Spawn" (type, count) {
@Enemy "enemy_001" (species="{{type}}", count={{count}})
}
@use "Spawn" (type="Orc", count=5)
)";
            auto g = graph::parse(source);
            auto enemy = g.node_by_id("enemy_001");
            assert(enemy.valid());
            assert(enemy["species"].get_string() == "Orc");
            assert(enemy["count"].get_int64() == 5);
        }
        std::cout << "  passed." << std::endl;

        // 9. Schema blocks are silently skipped during graph parsing
        std::cout << "Test 9: Schema skipping..." << std::endl;
        {
            auto g = graph::parse(R"(
@schema "MySchema" { name: string required }
@Node 1
)");
            // Не должно быть ошибок
            assert(g.node_by_id("1").valid());
        }
        std::cout << "  passed." << std::endl;

        // 10. Resolver: ref:// resolution
        std::cout << "Test 10: Reference resolution..." << std::endl;
        {
            auto g = graph::parse(R"(@Node 1 (link="ref://some_node"))");
            map_resolver res;
            res.add_handler("ref://", [](const std::string& uri) -> std::optional<value> {
                return value(std::string("resolved:" + uri));
            });
            g.resolve_references(res);
            auto n = g.node_by_id("1");
            // Атрибут link должен стать строкой "resolved:ref://some_node"
            assert(n["link"].get_string() == "resolved:ref://some_node");
        }
        std::cout << "  passed." << std::endl;

        // 11. Multiple separate tag brackets
        std::cout << "Test 11: Multiple tag brackets..." << std::endl;
        {
            auto g = graph::parse(R"(@Obj 1 [MainCamera] [EditorHidden] (name="cam"))");
            auto node = g.node_by_id("1");
            assert(node.tags().size() == 2);
            assert(node.tags()[0] == "MainCamera");
            assert(node.tags()[1] == "EditorHidden");
        }
        std::cout << "  passed." << std::endl;

        // 12. Spaces as attribute delimiters (пробелы вместо запятых)
        std::cout << "Test 12: Space separated attributes..." << std::endl;
        {
            auto g = graph::parse(R"(@Box 1 (width=10 height=20 color="red"))");
            auto box = g.node_by_id("1");
            assert(box["width"].get_int64() == 10);
            assert(box["height"].get_int64() == 20);
            assert(box["color"].get_string() == "red");
        }
        std::cout << "  passed." << std::endl;

        // 13. Lenient parsing with error recovery
        std::cout << "Test 13: Lenient parsing..." << std::endl;
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
        std::cout << "  passed." << std::endl;

        // 14. Узлы без id в @include обрабатываются как шаблоны
        // (проверяем через @include, но для теста создадим отдельный файл? Пропустим,
        // т.к. файловая система тестов не гарантирует наличие файла. Можно проверить
        // через ручное добавление узла без id, которое должно создавать шаблон.)
        std::cout << "Test 14: Node without id becomes template..." << std::endl;
        {
            auto g = graph::parse(R"(@MyTemplateClass)");
            // узел не создан, вместо этого определён шаблон с именем типа
            assert(g.get_template("MyTemplateClass") != nullptr);
        }
        std::cout << "  passed." << std::endl;

        std::cout << "All extended tests passed (including new features)!" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed with exception: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}