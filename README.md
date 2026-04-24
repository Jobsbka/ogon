OGON — Open Graph Object Notation

**Header-only библиотека C++17 для работы с графовым форматом данных**

OGON — это минималистичный, человекочитаемый и машинно-дружественный формат для описания графов, конфигураций и сообщений.  
Библиотека предоставляет:

- лёгкий парсер и сериализатор
- потоковый разбор сообщений
- встроенную систему шаблонов
- разрешение ссылок (URI)
- валидацию типов через схемы

---

## Краткий обзор формата

OGON (Open Graph Object Notation) разработан как единый способ представления вычислительных графов, конфигураций, потоков сообщений и ресурсов.  
Все данные представляются в виде **узлов** с атрибутами и **гиперрёбер** между ними.

Минимальный пример:

```ogon
@Constant 1 (value=42)
@Variable 2 (name="x")
# 1 dataflow> 2
```

## Синтаксис

### Узлы

```
@Тип id [тег1 тег2] (атрибут=значение, ...)
```

- `Тип` – идентификатор (начинается с буквы или `_`).
- `id` – число или строка в двойных кавычках. Если `id` опущен, узел считается шаблоном и не создаётся.
- `[теги]` – необязательные метки. Можно несколько блоков `[ ]`, они объединяются.
- `(атрибуты)` – пары `ключ=значение`, разделённые запятыми или пробелами.

Значения могут быть:
- числами (целыми / с плавающей точкой)
- `true`, `false`, `null`
- строками в двойных кавычках (с escape-последовательностями `\"`, `\\`, `\n`, `\r`, `\t`)
- массивами `[1, 2, 3]`
- объектами `{a=1, b="text"}`
- бинарными данными (BASE64)
- ссылками (`ref://`, `asset://`, `file://`, `http://` и т.п.)

Примеры узлов:

```ogon
@Player "hero" [MainCharacter] (health=100, position={x=0, y=0, z=10})
@Constant 1 (value=3.14)
@Mesh model1 (source="asset://meshes/car.obj")
@Img 1 (data=BASE64:iVBORw0KGgo=)
```

### Рёбра

Однонаправленное ребро:
```
# источники тип> стоки (атрибуты) [теги]
```
Двунаправленное ребро:
```
# источники <обратный_тип[:порт]|прямой_тип[:порт]> стоки (атрибуты) [теги]
```

Конечная точка записывается как:
- `NodeId`
- `NodeId:порт`
- `NodeId@role`
- `NodeId:порт@role`

Если точек несколько, они группируются в `( )`.

Примеры:
```ogon
# (1, 2) control> 3
# 5 <sync:0|sync:1> 6
# 1:1 <event:1|event:2> 2:2 [fast] (priority=high)
```

### Блоки данных

```ogon
@data "Имя" { ключ = значение, ... }
```

Допускаются вложенные объекты.
```ogon
@data "Settings" {
    volume = 0.8
    fullscreen = true
    layers = ["Default", "UI"]
}
```

### Таблицы

```ogon
@table "Имя" {
    columns = ["столбец1", "столбец2", ...]
    rows = [
        [значение, значение, ...],
        ...
    ]
}
```

### Метаданные

Строки, начинающиеся с `#!`:
```ogon
#!author "ArxTeam"
#!version 1.2
```
Ключ – идентификатор, значение – всё до конца строки (без кавычек).

### Шаблоны

Объявление:
```ogon
@template "Имя" (параметр1, параметр2) {
    @Тип "{{параметр1}}_{{параметр2}}" (поле={{параметр2}})
}
```
Подстановка:
```ogon
@use "Имя" (параметр1=значение, параметр2=значение)
```
Внутри тела шаблона двойные фигурные скобки `{{var}}` заменяются на переданное значение.

### Схемы

Блоки `@schema` игнорируются при построении графа, но могут быть разобраны отдельно (см. API):

```ogon
@schema "MyNode" {
    value: int required
    @optional name: string
}
```

### Комментарии

- Строчные `//`  
- Блочные `/* ... */`  

Комментарии корректно обрабатываются внутри строк и могут располагаться в любом месте.

### Бинарные данные и ссылки

- Бинарные данные кодируются в Base64 с префиксом `BASE64:` внутри строки или как отдельный токен:
  ```
  @Img 1 (data="BASE64:iVBORw0KGgo=")
  @Img 2 (data=BASE64:iVBORw0KGgo=)
  ```
- Ссылки (ref) определяются префиксами:
  `ref://`, `asset://`, `file://`, `bundle://`, `http://`, `https://`, `var://`.
  Значение сохраняется как строка-ссылка и может быть разрешено позже.

---

## API библиотеки C++

### Подключение

Библиотека header-only. Достаточно включить основной заголовок:

```cpp
#include <ogon.hpp>
```

Все классы находятся в пространстве имён `ogon`.

---

### Значение `value`

Универсальный тип данных, который может хранить:
- `null`
- `int64_t`
- `double`
- `bool`
- `std::string`
- `std::vector<uint8_t>` (binary)
- `std::vector<value>` (array)
- `std::unordered_map<std::string, value>` (object)
- ссылку (ref)

**Конструирование:**
```cpp
value v_int(42);
value v_double(3.14);
value v_bool(true);
value v_str("hello");
value v_ref("asset://path", value_t::ref);
value v_binary(std::vector<uint8_t>{0x89, 0x50});
value v_arr = {1, 2, 3};
value v_obj = {{"x", 10}, {"y", 20}};
```

**Проверка типа:**
```cpp
v.is_int(); v.is_double(); v.is_bool(); v.is_string();
v.is_array(); v.is_object(); v.is_binary(); v.is_ref();
v.type() // возвращает enum value_t
```

**Доступ к данным:**
```cpp
v.get_int64(); v.get_double(); v.get_bool();
v.get_string();        // возвращает строку (или URI для ref)
v.get_binary();        // const std::vector<uint8_t>&
v.get<value::array_t>();
v.get<value::object_t>();
```

**Работа с массивами и объектами:**
```cpp
v[0] = 5;                    // доступ по индексу (массив)
v["key"] = "value";          // доступ по ключу (объект)
v.push_back(value);          // добавить в массив
v.contains("key");           // проверить наличие ключа
v.size();                    // количество элементов
for (auto& item : v) { ... } // итерация (только массив)
```

---

### Узел `node`

Предоставляет доступ к типу, идентификатору и атрибутам.

```cpp
node n = graph.add_node("Constant", "1");
n.type();          // "Constant"
n.id();            // "1"
n["value"] = 42;
n.tags();          // const std::vector<std::string>&
n.set_tag({"tag1", "tag2"});
n.add_tag("tag3");
n.has_attribute("key");
n.remove_attribute("key");
// итерация по атрибутам:
for (auto it = n.attributes_begin(); it != n.attributes_end(); ++it) {
    auto& [key, val] = *it;
}
```

---

### Гиперребро `hyperedge`

```cpp
auto e = graph.add_edge();
e.set_bidirectional(true);
e.set_forward_type("sync");
e.set_backward_type("sync");
e.add_forward_endpoint(endpoint("1", 0, "out"));
e.add_forward_endpoint(endpoint("2", 2, "in"));
e.add_backward_endpoint(endpoint("2", 0, "out"));
e.add_backward_endpoint(endpoint("1", 2, "in"));
e.set_tags({"important"});
e["priority"] = 5;
```

**Структура `endpoint`:**
```cpp
struct endpoint {
    std::string node_id;
    int port = 0;
    std::string role; // "in", "out", "inout"
};
```

---

### Граф `graph`

Центральный класс для построения и парсинга данных.

**Создание и наполнение:**
```cpp
graph g;
g.set_metadata("author", "Me");
g.add_node(...);
g.add_edge();
g.connect("1", "2", "dataflow"); // быстрое однонаправленное ребро
g.set_data_block("Settings", v_obj);
g.set_table("Items", table_block{...});
g.define_template("MyTpl", {"p1", "p2"}, R"( ... )");
g.instantiate_template("MyTpl", {{"p1", 5}, {"p2", "text"}});
g.resolve_references(resolver);
```

**Навигация:**
```cpp
node n = g.node_by_id("1");
for (auto node : g.node_range()) { ... }
for (auto edge : g.edge_range()) { ... }
```

**Сериализация:**
```cpp
std::string text = g.to_agon();           // компактный вывод
std::string text = g.to_agon(true);       // с секционными комментариями
g.save("file.ogon");
```

**Парсинг:**
```cpp
graph g = graph::parse(agon_string);
graph g = graph::parse(agon_string, true);     // lenient — пропуск ошибочных строк
graph g = graph::parse_file("file.ogon");
graph g = graph::parse_file("file.ogon", true);
g.load("file.ogon");                            // загрузка в существующий граф
```

---

### Потоковая передача сообщений

Поддерживает разделение потока на фреймы по строке-разделителю `*||*`:

```cpp
std::stringstream ss("...");
message_stream ms(ss);
graph msg;
while (ms.read_next(msg)) {
    // обработать сообщение
}
```

---

### Разрешение ссылок

```cpp
map_resolver resolver;
resolver.add_handler("ref://", [](const std::string& uri) -> std::optional<value> {
    return value("resolved_" + uri);
});
g.resolve_references(resolver); // все атрибуты-ссылки заменяются
```

---

### Шаблоны (API)

Встроенная в граф система шаблонов:

```cpp
g.define_template("Spawn", {"type", "count"}, 
    R"(@Enemy "{{type}}_{{count}}" (type="{{type}}", count={{count}}))");
std::unordered_map<std::string, value> args;
args["type"] = std::string("Orc");
args["count"] = 3;
g.instantiate_template("Spawn", args);
```

Парсер также поддерживает директивы `@template` и `@use`.

---

### Схемы

Схемы можно разбирать из строки:

```cpp
std::string schema_src = R"(
node "Constant" {
    value: int required
    @optional name: string
}
)";
type_registry reg = parse_schema_block(schema_src);
const schema_node_def* s = reg.get_node_schema("Constant");
```

В графе блоки `@schema` игнорируются.

---

### Base64

Утилиты для кодирования/декодирования:

```cpp
std::vector<uint8_t> data = {0x89, 0x50, 0x4E, 0x47};
std::string encoded = base64_encode(data);           // "iVBORw0KGgo="
std::vector<uint8_t> decoded = base64_decode(encoded);
```

---

### Ошибки

- `parse_error` – ошибка разбора, содержит текст, номер строки и столбца.
- `type_error` – ошибка приведения типа значения.
- `graph_error` – ошибка структуры графа (дубликат узла, отсутствие шаблона и т.д.).

---

## Сборка и установка

### Требования

- Компилятор с поддержкой C++17
- CMake ≥ 3.14

### Интеграция

Библиотека header-only. Достаточно скопировать папку `include/ogon` в проект или добавить через CMake:

```cmake
add_subdirectory(ogon)
target_link_libraries(my_target PRIVATE ogon)
```

### Сборка примеров и тестов

```bash
git clone https://github.com/your/ogon.git
cd ogon
cmake -B build -DOGON_BUILD_EXAMPLES=ON -DOGON_BUILD_TESTS=ON
cmake --build build
cd build && ctest
```

---

## Примеры

### 1. Создание графа и сериализация

```cpp
#include <ogon.hpp>
#include <iostream>

int main() {
    using namespace ogon;
    graph g;
    auto c = g.add_node("Constant", "1");
    c["value"] = 42;
    auto v = g.add_node("Variable", "2");
    v["name"] = "x";
    g.connect("1", "2", "dataflow");
    std::cout << g.to_agon() << std::endl;
    return 0;
}
```

Вывод:
```ogon
@Constant 1 (value=42)
@Variable 2 (name="x")
# 1 dataflow> 2
```

### 2. Парсинг из строки

```cpp
graph g = graph::parse(R"(
@Player 1 (health=100)
@Enemy 2 (health=50)
# 1 attack> 2
)");
auto player = g.node_by_id("1");
std::cout << player["health"].get_int64() << std::endl; // 100
```

### 3. Поток сообщений

```cpp
std::string data = R"(@event "Jump" (height=2.5)
*||*
@event "Land" (surface="grass")
)";
std::istringstream stream(data);
message_stream ms(stream);
graph msg;
while (ms.read_next(msg)) {
    // обработать msg
}
```

Больше примеров в папке `examples/`.

---

## Лицензия

OGON распространяется под лицензией **MIT**. 
