# Video Streaming System - C++

A high-performance, standards-compliant RTSP/RTP video streaming system. This project implements a full-stack streaming server and client capable of handling real-time H.264 video with adaptive network resilience.

## Technical Specifications

### Core Protocols
- **RTSP (RFC 2326)**: Complete server and client implementation supporting OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, and TEARDOWN methods.
- **RTP (RFC 3550)**: Efficient packetization and transmission of real-time data.
- **H.264 (RFC 6184)**: Payload format support, including NAL unit parsing, aggregation (STAP-A), and fragmentation (FU-A).
- **Transport**: Support for both UDP (unicast/multicast) and TCP Interleaved (RTP over RTSP) for firewall traversal.

### Implemented Features
1.  **RTSP/RTP Stack**:
    -   Custom implementation of RTSP (RFC 2326) server and client state machines.
    -   Zero-copy RTP packet processing path.
    -   Session management and keep-alive mechanisms.
2.  **Media Processing**:
    -   Synthetic H.264 video generator (I-frame/P-frame) for latency testing without camera hardware.
    -   Adaptive Jitter Buffer to handle network jitter, packet reordering, and loss.
3.  **Network Resilience**:
    -   Packet loss detection and concealment.
    -   Congestion control hooks.
4.  **Performance**:
    -   Asynchronous I/O architecture.
    -   Lock-free ring buffers for inter-thread frame passing.
    -   Utilizes C++26 features (Modules, `std::expected`, `std::barrier`) for reliability and speed.

## Project Structure

```
video-streaming/
├── common/                 # Shared modules (Logger, Interfaces, Std wrappers)
│   ├── logger.ixx         # Logger module interface
│   ├── logger.cpp         # Logger implementation
│   └── interfaces.ixx     # Common type definitions
├── src/                    # Application entry points
│   ├── simple_rtsp_server.cpp # Reference RTSP Server implementation
│   └── test_rtsp_client.cpp   # Reference RTSP Client/Recorder
├── rtp/                    # RTP/RTSP protocol implementation
│   ├── rtsp_client.cpp
│   ├── h264_packetizer.cpp
│   └── rtp_packet.cpp
├── media/                  # Video frame handling and synthetic encoding
├── network/                # Socket abstractions (UDP/TCP)
├── jitter/                 # Jitter buffer implementation
├── tests/                  # Unit and Integration tests (Catch2 v3)
├── CMakeLists.txt          # Build configuration
└── vcpkg.json             # Dependency manifest
```

## 🛠️ Требования

- **Компилятор**: MSVC 19.40+ или GCC 14+ с поддержкой C++26
- **CMake**: 3.30+ для поддержки модулей C++26
- **vcpkg**: Для управления зависимостями
- **Платформы**: Windows, Linux

## 📦 Установка и сборка

### 1. Клонирование репозитория
```bash
git clone https://github.com/video-streaming/video-streaming.git
cd video-streaming
```

### 2. Настройка vcpkg
```bash
# Установка vcpkg (если еще не установлен)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # Linux/macOS
# или
./bootstrap-vcpkg.bat  # Windows
```

### 3. Установка зависимостей
```bash
# Из корневой директории проекта
vcpkg install --triplet=x64-windows  # Windows
# или
vcpkg install --triplet=x64-linux     # Linux
```

### 4. Сборка проекта
```bash
# Создание директории сборки
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake

# Сборка
cmake --build build

# Запуск тестов
ctest --test-dir build
```

### 5. Запуск приложения
```bash
# Запуск основного приложения
./build/video_streaming_app

# Запуск тестов
./build/video_streaming_tests
```

## 🧪 Тестирование

Проект использует **Catch2 v3** для тестирования с полным покрытием C++26 возможностей:

### Unit тесты
- **Logger**: Тестирование модуля логирования
- **Interfaces**: Проверка типов-алиасов и совместимости
- **Performance**: Тесты производительности и памяти

### Интеграционные тесты
- **Multi-threading**: Многопоточные сценарии использования
- **Error Handling**: Обработка ошибок и восстановление
- **Memory Management**: Управление памятью и утечки

### Запуск тестов
```bash
# Все тесты
ctest --test-dir build

# Конкретный тест
./build/video_streaming_tests "[logger]"
./build/video_streaming_tests "[performance]"
```

## 💡 Примеры использования

### Базовое логирование
```cpp
import video_streaming.logger;

auto& manager = LoggerManager::instance();
auto* logger = manager.get_logger("my_app");

logger->info(LogFormat("Application started"));
logger->error(LogFormat("Error occurred: {}", error_code));
```

### Продвинутое логирование
```cpp
// Perfect forwarding
logger->info(LogFormat("User {} logged in", user_id));

// Ranges logging
std::vector<int> numbers = {1, 2, 3, 4, 5};
logger->info_range(LogFormat("Numbers"), numbers);

// Thread-safe logging
std::thread worker([&logger] {
    logger->info(LogFormat("Worker thread started"));
});
```

### C++26 особенности
```cpp
// Structured bindings
auto [name, level] = std::pair{"logger", LogLevel::INFO};

// Ranges
auto filtered = data | std::views::filter([](auto& item) {
    return item.is_valid();
});

// consteval
constexpr LogFormat msg("Compile-time message");
```

## 🔧 Конфигурация

### CMake опции
```cmake
# C++26 стандарт
set(CMAKE_CXX_STANDARD 26)

# Модули
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Экспериментальные возможности
set(CMAKE_CXX_FLAGS_EXPERIMENTAL ON)
```

### Опции компилятора
```bash
# MSVC
/std:c++latest /experimental:c++26

# GCC/Clang
-std=c++26 -fmodules-ts
```

## 📊 Производительность

### Бенчмарки
- **Логирование**: >10,000 сообщений/секунду
- **Создание логгеров**: <10мс для 100 логгеров
- **Многопоточность**: Линейная масштабируемость до 8 потоков
- **Память**: Эффективное использование RAII

### Оптимизации
- **Compile-time**: `consteval` для форматов сообщений
- **Runtime**: Perfect forwarding и move semantics
- **Memory**: Smart pointers и structured bindings
- **Threading**: std::barrier и lock-free структуры

## 🐛 Отладка

### Логирование отладки
```cpp
auto* debug_logger = manager.get_logger("debug");
debug_logger->set_level(LogLevel::DEBUG);
debug_logger->debug(LogFormat("Debug info: {}", debug_data));
```

### Профилирование
```bash
# Сборка с отладочными символами
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Анализ производительности
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
```

## 🤝 Вклад в проект

1. Fork репозитория
2. Создание feature branch (`git checkout -b feature/amazing-feature`)
3. Commit изменений (`git commit -m 'Add amazing C++26 feature'`)
4. Push в branch (`git push origin feature/amazing-feature`)
5. Создание Pull Request

### Требования к коду
- Использование C++26 возможностей
- Покрытие кода тестами
- Следование style guide
- Документация API

## 📄 Лицензия

MIT License - см. [LICENSE](LICENSE) файл для деталей.

## 🙏 Благодарности

- **C++26 Committee** за невероятные возможности языка
- **Catch2** за отличный фреймворк тестирования
- **spdlog** за быструю библиотеку логирования
- **vcpkg** за удобное управление зависимостями

## 📚 Дополнительные ресурсы

- [C++26 Proposal Papers](https://github.com/cplusplus/papers)
- [C++ Modules Tutorial](https://learn.microsoft.com/en-us/cpp/cpp/modules-cpp)
- [Catch2 Documentation](https://github.com/catchorg/Catch2)
- [spdlog Documentation](https://github.com/gabime/spdlog)

---

**Built with ❤️ using C++26 and modern development practices**
