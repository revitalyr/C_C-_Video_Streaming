module;

#include <iostream>
#include <string>
#include <vector>
#include <source_location>
#include <concepts>
#include <ranges>
import video_streaming.interfaces;

export module test_framework;

export namespace test_framework {

// C++26 concepts для тестирования
template<typename T>
concept Testable = requires(T t) {
    { t.test() } -> void;
};

template<typename T>
concept TestableWithSetup = requires(T t) {
    { t.setup() } -> void;
    { t.test() } -> void;
    { t.teardown() } -> void;
};

// C++26 структура для результатов тестов
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    std::chrono::nanoseconds duration;
    std::source_location location;
    
    constexpr TestResult(
        std::string_view n,
        bool p,
        std::string_view msg,
        std::chrono::nanoseconds d,
        std::source_location loc = std::source_location::current()
    ) : name(n), passed(p), message(msg), duration(d), location(loc) {}
};

// C++26 класс для управления тестами
class TestRunner {
public:
    static TestRunner& instance() {
        static TestRunner runner;
        return runner;
    }
    
    template<Testable T>
    requires std::default_initializable<T>
    constexpr void add_test(std::string_view name) {
        m_tests.emplace_back([name]() {
            T test;
            auto start = std::chrono::high_resolution_clock::now();
            try {
                test.test();
                auto end = std::chrono::high_resolution_clock::now();
                return TestResult(name, true, "PASSED", end - start);
            } catch (const std::exception& e) {
                auto end = std::chrono::high_resolution_clock::now();
                return TestResult(name, false, e.what(), end - start);
            }
        });
    }
    
    template<TestableWithSetup T>
    requires std::default_initializable<T>
    constexpr void add_test_with_setup(std::string_view name) {
        m_tests.emplace_back([name]() {
            T test;
            auto start = std::chrono::high_resolution_clock::now();
            try {
                test.setup();
                test.test();
                test.teardown();
                auto end = std::chrono::high_resolution_clock::now();
                return TestResult(name, true, "PASSED", end - start);
            } catch (const std::exception& e) {
                try {
                    test.teardown(); // Ensure cleanup even if test fails
                } catch (...) {}
                auto end = std::chrono::high_resolution_clock::now();
                return TestResult(name, false, e.what(), end - start);
            }
        });
    }
    
    constexpr int run_all() {
        int passed = 0;
        int failed = 0;
        
        std::cout << "Running " << m_tests.size() << " tests...\n\n";
        
        for (auto& test_func : m_tests) {
            auto result = test_func();
            
            std::cout << "[" << (result.passed ? "PASS" : "FAIL") << "] "
                      << result.name << " (" 
                      << std::chrono::duration_cast<std::chrono::microseconds>(result.duration).count() 
                      << " μs)\n";
            
            if (!result.passed) {
                std::cout << "  Error: " << result.message << "\n";
                std::cout << "  Location: " << result.location.file_name() 
                          << ":" << result.location.line() << "\n";
                failed++;
            } else {
                passed++;
            }
        }
        
        std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
        
        return failed;
    }

private:
    std::vector<std::function<TestResult()>> m_tests;
};

// C++26 макросы для упрощения написания тестов
#define TEST(name, ...) \
    struct name##_test_t { \
        void test(); \
    }; \
    \
    void name##_test_t::test() __VA_ARGS__ \
    \
    namespace { \
        const auto test_##name##_registrar = []() { \
            test_framework::TestRunner::instance().add_test<name##_test_t>(#name); \
        }(); \
    }

#define TEST_SETUP(name, ...) \
    struct name##_test_t { \
        void setup(); \
        void test(); \
        void teardown(); \
    }; \
    \
    void name##_test_t::setup() __VA_ARGS__ \
    void name##_test_t::test() __VA_ARGS__ \
    void name##_test_t::teardown() __VA_ARGS__ \
    \
    namespace { \
        const auto test_##name##_registrar = []() { \
            test_framework::TestRunner::instance().add_test_with_setup<name##_test_t>(#name); \
        }(); \
    }

// C++26 assertion macros
#define EXPECT(condition, message) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error("EXPECT(" #condition ") failed: " message); \
        } \
    } while(false)

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error("ASSERT(" #condition ") failed: " message); \
        } \
    } while(false)

#define EXPECT_EQ(a, b, message) \
    EXPECT((a) == (b), message)

#define ASSERT_EQ(a, b, message) \
    ASSERT((a) == (b), message)

#define EXPECT_TRUE(condition, message) \
    EXPECT((condition), message)

#define ASSERT_TRUE(condition, message) \
    ASSERT((condition), message)

#define EXPECT_FALSE(condition, message) \
    EXPECT(!(condition), message)

#define ASSERT_FALSE(condition, message) \
    ASSERT(!(condition), message)

} // namespace test_framework
