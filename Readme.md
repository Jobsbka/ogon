OGON — Open Graph Object Notation



\*\*Header-only библиотека C++17 для работы с графовым форматом данных\*\*



```



OGON — это минималистичный, человекочитаемый и машинно-дружественный формат для описания графов, конфигураций и сообщений.  

Библиотека предоставляет:



\- лёгкий парсер и сериализатор

\- потоковый разбор сообщений

\- встроенную систему шаблонов

\- разрешение ссылок (URI)

\- валидацию типов через схемы



\---



\## Оглавление



1\. \[Краткий обзор формата](#краткий-обзор-формата)

2\. \[Синтаксис](#синтаксис)

&#x20;  - \[Узлы](#узлы)

&#x20;  - \[Рёбра](#рёбра)

&#x20;  - \[Блоки данных](#блоки-данных)

&#x20;  - \[Таблицы](#таблицы)

&#x20;  - \[Метаданные](#метаданные)

&#x20;  - \[Шаблоны](#шаблоны)

&#x20;  - \[Схемы](#схемы)

&#x20;  - \[Комментарии](#комментарии)

&#x20;  - \[Бинарные данные и ссылки](#бинарные-данные-и-ссылки)

3\. \[API библиотеки C++](#api-библиотеки-c)

&#x20;  - \[Подключение](#подключение)

&#x20;  - \[Значение `value`](#значение-value)

&#x20;  - \[Узел `node`](#узел-node)

&#x20;  - \[Гиперребро `hyperedge`](#гиперребро-hyperedge)

&#x20;  - \[Граф `graph`](#граф-graph)

&#x20;  - \[Потоковая передача сообщений](#потоковая-передача-сообщений)

&#x20;  - \[Разрешение ссылок](#разрешение-ссылок)

&#x20;  - \[Шаблоны](#шаблоны-api)

&#x20;  - \[Схемы](#схемы-api)

&#x20;  - \[Base64](#base64)

&#x20;  - \[Ошибки](#ошибки)

4\. \[Сборка и установка](#сборка-и-установка)

5\. \[Примеры](#примеры)

6\. \[Лицензия](#лицензия)



\---



\## Краткий обзор формата



OGON (Open Graph Object Notation) разработан как единый способ представления вычислительных графов, конфигураций, потоков сообщений и ресурсов.  

Все данные представляются в виде \*\*узлов\*\* с атрибутами и \*\*гиперрёбер\*\* между ними.



Минимальный пример:



```ogon

@Constant 1 (value=42)

@Variable 2 (name="x")

\# 1 dataflow> 2

```



\## Синтаксис



\### Узлы



```

@Тип id \[тег1 тег2] (атрибут=значение, ...)

```



\- `Тип` – идентификатор (начинается с буквы или `\_`).

\- `id` – число или строка в двойных кавычках. Если `id` опущен, узел считается шаблоном и не создаётся.

\- `\[теги]` – необязательные метки. Можно несколько блоков `\[ ]`, они объединяются.

\- `(атрибуты)` – пары `ключ=значение`, разделённые запятыми или пробелами.



Значения могут быть:

\- числами (целыми / с плавающей точкой)

\- `true`, `false`, `null`

\- строками в двойных кавычках (с escape-последовательностями `\\"`, `\\\\`, `\\n`, `\\r`, `\\t`)

\- массивами `\[1, 2, 3]`

\- объектами `{a=1, b="text"}`

\- бинарными данными (BASE64)

\- ссылками (`ref://`, `asset://`, `file://`, `http://` и т.п.)



Примеры узлов:



```ogon

@Player "hero" \[MainCharacter] (health=100, position={x=0, y=0, z=10})

@Constant 1 (value=3.14)

@Mesh model1 (source="asset://meshes/car.obj")

@Img 1 (data=BASE64:iVBORw0KGgo=)

```



\### Рёбра



Однонаправленное ребро:

```

\# источники тип> стоки (атрибуты) \[теги]

```

Двунаправленное ребро:

```

\# источники <обратный\_тип\[:порт]|прямой\_тип\[:порт]> стоки (атрибуты) \[теги]

```



Конечная точка записывается как:

\- `NodeId`

\- `NodeId:порт`

\- `NodeId@role`

\- `NodeId:порт@role`



Если точек несколько, они группируются в `( )`.



Примеры:

```ogon

\# (1, 2) control> 3

\# 5 <sync:0|sync:1> 6

\# 1:1 <event:1|event:2> 2:2 \[fast] (priority=high)

```



\### Блоки данных



```ogon

@data "Имя" { ключ = значение, ... }

```



Допускаются вложенные объекты.

```ogon

@data "Settings" {

&#x20;   volume = 0.8

&#x20;   fullscreen = true

&#x20;   layers = \["Default", "UI"]

}

```



\### Таблицы



```ogon

@table "Имя" {

&#x20;   columns = \["столбец1", "столбец2", ...]

&#x20;   rows = \[

&#x20;       \[значение, значение, ...],

&#x20;       ...

&#x20;   ]

}

```



\### Метаданные



Строки, начинающиеся с `#!`:

```ogon

\#!author "ArxTeam"

\#!version 1.2

```

Ключ – идентификатор, значение – всё до конца строки (без кавычек).



\### Шаблоны



Объявление:

```ogon

@template "Имя" (параметр1, параметр2) {

&#x20;   @Тип "{{параметр1}}\_{{параметр2}}" (поле={{параметр2}})

}

```

Подстановка:

```ogon

@use "Имя" (параметр1=значение, параметр2=значение)

```

Внутри тела шаблона двойные фигурные скобки `{{var}}` заменяются на переданное значение.



\### Схемы



Блоки `@schema` игнорируются при построении графа, но могут быть разобраны отдельно (см. API):



```ogon

@schema "MyNode" {

&#x20;   value: int required

&#x20;   @optional name: string

}

```



\### Комментарии



\- Строчные `//`  

\- Блочные `/\* ... \*/`  



Комментарии корректно обрабатываются внутри строк и могут располагаться в любом месте.



\### Бинарные данные и ссылки



\- Бинарные данные кодируются в Base64 с префиксом `BASE64:` внутри строки или как отдельный токен:

&#x20; ```

&#x20; @Img 1 (data="BASE64:iVBORw0KGgo=")

&#x20; @Img 2 (data=BASE64:iVBORw0KGgo=)

&#x20; ```

\- Ссылки (ref) определяются префиксами:

&#x20; `ref://`, `asset://`, `file://`, `bundle://`, `http://`, `https://`, `var://`.

&#x20; Значение сохраняется как строка-ссылка и может быть разрешено позже.



\---



\## API библиотеки C++



\### Подключение



Библиотека header-only. Достаточно включить основной заголовок:



```cpp

\#include <ogon.hpp>

```



Все классы находятся в пространстве имён `ogon`.



\---



\### Значение `value`



Универсальный тип данных, который может хранить:

\- `null`

\- `int64\_t`

\- `double`

\- `bool`

\- `std::string`

\- `std::vector<uint8\_t>` (binary)

\- `std::vector<value>` (array)

\- `std::unordered\_map<std::string, value>` (object)

\- ссылку (ref)



\*\*Конструирование:\*\*

```cpp

value v\_int(42);

value v\_double(3.14);

value v\_bool(true);

value v\_str("hello");

value v\_ref("asset://path", value\_t::ref);

value v\_binary(std::vector<uint8\_t>{0x89, 0x50});

value v\_arr = {1, 2, 3};

value v\_obj = {{"x", 10}, {"y", 20}};

```



\*\*Проверка типа:\*\*

```cpp

v.is\_int(); v.is\_double(); v.is\_bool(); v.is\_string();

v.is\_array(); v.is\_object(); v.is\_binary(); v.is\_ref();

v.type() // возвращает enum value\_t

```



\*\*Доступ к данным:\*\*

```cpp

v.get\_int64(); v.get\_double(); v.get\_bool();

v.get\_string();        // возвращает строку (или URI для ref)

v.get\_binary();        // const std::vector<uint8\_t>\&

v.get<value::array\_t>();

v.get<value::object\_t>();

```



\*\*Работа с массивами и объектами:\*\*

```cpp

v\[0] = 5;                    // доступ по индексу (массив)

v\["key"] = "value";          // доступ по ключу (объект)

v.push\_back(value);          // добавить в массив

v.contains("key");           // проверить наличие ключа

v.size();                    // количество элементов

for (auto\& item : v) { ... } // итерация (только массив)

```



\---



\### Узел `node`



Предоставляет доступ к типу, идентификатору и атрибутам.



```cpp

node n = graph.add\_node("Constant", "1");

n.type();          // "Constant"

n.id();            // "1"

n\["value"] = 42;

n.tags();          // const std::vector<std::string>\&

n.set\_tag({"tag1", "tag2"});

n.add\_tag("tag3");

n.has\_attribute("key");

n.remove\_attribute("key");

// итерация по атрибутам:

for (auto it = n.attributes\_begin(); it != n.attributes\_end(); ++it) {

&#x20;   auto\& \[key, val] = \*it;

}

```



\---



\### Гиперребро `hyperedge`



```cpp

auto e = graph.add\_edge();

e.set\_bidirectional(true);

e.set\_forward\_type("sync");

e.set\_backward\_type("sync");

e.add\_forward\_endpoint(endpoint("1", 0, "out"));

e.add\_forward\_endpoint(endpoint("2", 2, "in"));

e.add\_backward\_endpoint(endpoint("2", 0, "out"));

e.add\_backward\_endpoint(endpoint("1", 2, "in"));

e.set\_tags({"important"});

e\["priority"] = 5;

```



\*\*Структура `endpoint`:\*\*

```cpp

struct endpoint {

&#x20;   std::string node\_id;

&#x20;   int port = 0;

&#x20;   std::string role; // "in", "out", "inout"

};

```



\---



\### Граф `graph`



Центральный класс для построения и парсинга данных.



\*\*Создание и наполнение:\*\*

```cpp

graph g;

g.set\_metadata("author", "Me");

g.add\_node(...);

g.add\_edge();

g.connect("1", "2", "dataflow"); // быстрое однонаправленное ребро

g.set\_data\_block("Settings", v\_obj);

g.set\_table("Items", table\_block{...});

g.define\_template("MyTpl", {"p1", "p2"}, R"( ... )");

g.instantiate\_template("MyTpl", {{"p1", 5}, {"p2", "text"}});

g.resolve\_references(resolver);

```



\*\*Навигация:\*\*

```cpp

node n = g.node\_by\_id("1");

for (auto node : g.node\_range()) { ... }

for (auto edge : g.edge\_range()) { ... }

```



\*\*Сериализация:\*\*

```cpp

std::string text = g.to\_agon();           // компактный вывод

std::string text = g.to\_agon(true);       // с секционными комментариями

g.save("file.ogon");

```



\*\*Парсинг:\*\*

```cpp

graph g = graph::parse(agon\_string);

graph g = graph::parse(agon\_string, true);     // lenient — пропуск ошибочных строк

graph g = graph::parse\_file("file.ogon");

graph g = graph::parse\_file("file.ogon", true);

g.load("file.ogon");                            // загрузка в существующий граф

```



\---



\### Потоковая передача сообщений



Поддерживает разделение потока на фреймы по строке-разделителю `\*||\*`:



```cpp

std::stringstream ss("...");

message\_stream ms(ss);

graph msg;

while (ms.read\_next(msg)) {

&#x20;   // обработать сообщение

}

```



\---



\### Разрешение ссылок



```cpp

map\_resolver resolver;

resolver.add\_handler("ref://", \[](const std::string\& uri) -> std::optional<value> {

&#x20;   return value("resolved\_" + uri);

});

g.resolve\_references(resolver); // все атрибуты-ссылки заменяются

```



\---



\### Шаблоны (API)



Встроенная в граф система шаблонов:



```cpp

g.define\_template("Spawn", {"type", "count"}, 

&#x20;   R"(@Enemy "{{type}}\_{{count}}" (type="{{type}}", count={{count}}))");

std::unordered\_map<std::string, value> args;

args\["type"] = std::string("Orc");

args\["count"] = 3;

g.instantiate\_template("Spawn", args);

```



Парсер также поддерживает директивы `@template` и `@use`.



\---



\### Схемы



Схемы можно разбирать из строки:



```cpp

std::string schema\_src = R"(

node "Constant" {

&#x20;   value: int required

&#x20;   @optional name: string

}

)";

type\_registry reg = parse\_schema\_block(schema\_src);

const schema\_node\_def\* s = reg.get\_node\_schema("Constant");

```



В графе блоки `@schema` игнорируются.



\---



\### Base64



Утилиты для кодирования/декодирования:



```cpp

std::vector<uint8\_t> data = {0x89, 0x50, 0x4E, 0x47};

std::string encoded = base64\_encode(data);           // "iVBORw0KGgo="

std::vector<uint8\_t> decoded = base64\_decode(encoded);

```



\---



\### Ошибки



\- `parse\_error` – ошибка разбора, содержит текст, номер строки и столбца.

\- `type\_error` – ошибка приведения типа значения.

\- `graph\_error` – ошибка структуры графа (дубликат узла, отсутствие шаблона и т.д.).



\---



\## Сборка и установка



\### Требования



\- Компилятор с поддержкой C++17

\- CMake ≥ 3.14



\### Интеграция



Библиотека header-only. Достаточно скопировать папку `include/ogon` в проект или добавить через CMake:



```cmake

add\_subdirectory(ogon)

target\_link\_libraries(my\_target PRIVATE ogon)

```



\### Сборка примеров и тестов



```bash

git clone https://github.com/your/ogon.git

cd ogon

cmake -B build -DOGON\_BUILD\_EXAMPLES=ON -DOGON\_BUILD\_TESTS=ON

cmake --build build

cd build \&\& ctest

```



\---



\## Примеры



\### 1. Создание графа и сериализация



```cpp

\#include <ogon.hpp>

\#include <iostream>



int main() {

&#x20;   using namespace ogon;

&#x20;   graph g;

&#x20;   auto c = g.add\_node("Constant", "1");

&#x20;   c\["value"] = 42;

&#x20;   auto v = g.add\_node("Variable", "2");

&#x20;   v\["name"] = "x";

&#x20;   g.connect("1", "2", "dataflow");

&#x20;   std::cout << g.to\_agon() << std::endl;

&#x20;   return 0;

}

```



Вывод:

```ogon

@Constant 1 (value=42)

@Variable 2 (name="x")

\# 1 dataflow> 2

```



\### 2. Парсинг из строки



```cpp

graph g = graph::parse(R"(

@Player 1 (health=100)

@Enemy 2 (health=50)

\# 1 attack> 2

)");

auto player = g.node\_by\_id("1");

std::cout << player\["health"].get\_int64() << std::endl; // 100

```



\### 3. Поток сообщений



```cpp

std::string data = R"(@event "Jump" (height=2.5)

\*||\*

@event "Land" (surface="grass")

)";

std::istringstream stream(data);

message\_stream ms(stream);

graph msg;

while (ms.read\_next(msg)) {

&#x20;   // обработать msg

}

```



Больше примеров в папке `examples/`.



\---



\## Лицензия



OGON распространяется под лицензией \*\*MIT\*\*. 

```

