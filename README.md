# transport_table

Приложение информационного табло для транспорта: показывает маршрут, время, погоду и контент. Состоит из клиентского Qt/QML экрана и набора C++ gRPC-сервисов (шлюз + сервисы погоды, маршрутов и контента), взаимодействующих через Protobuf/gRPC.

## Стек

- **Язык:** C++20
- **UI-клиент (vehicle-screen):** Qt6 (Core, Gui, Qml, Quick, QuickControls2, Protobuf, Grpc)
- **Бэкенд-сервисы:** C++ + gRPC/Protobuf
- **Сборка:** CMake (≥3.25) + Conan 2
- **IPC:** gRPC, Protocol Buffers

## Зависимости

- grpc/1.67.1
- nlohmann_json/3.11.3
- cpr/1.10.5 (HTTP-клиент)
- pugixml/1.14 (парсинг XML)
- gtest/1.14.0 (тесты)
- protobuf (build requirement)
- libpqxx (для content-service, PostgreSQL)
- absl, ZLIB (транзитивные зависимости gRPC/Protobuf)

## Команды для сборки

```bash
# Установка зависимостей через Conan
conan install . --output-folder=build --build=missing

# Конфигурация CMake с тулчейном от Conan
cmake --preset conan-release   # или -B build -S . -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake

# Сборка
cmake --build build
```

Для сборки `vehicle-screen` дополнительно требуется Qt6 с модулями Protobuf/Grpc (устанавливается отдельно от Qt, не через Conan).

## Структура проекта

```
transport_table/
├── CMakeLists.txt          # корневой билд-скрипт, объединяет все подмодули
├── conanfile.py            # описание зависимостей Conan
├── proto/                  # общие .proto-схемы (board, common, content, route, weather)
├── gateway/                # gRPC-шлюз (маршрутизация между сервисами и клиентом)
├── services/
│   ├── weather/            # сервис погоды (cpr + nlohmann_json)
│   ├── route/               # сервис маршрутов
│   └── content/             # сервис контента (libpqxx + cpr + pugixml)
└── vehicle-screen/         # Qt6/QML клиент (экран табло)
    ├── qml/                # QML-интерфейс
    └── src/                # C++ бэкенд клиента (backend, board_client, time_manager)
```
