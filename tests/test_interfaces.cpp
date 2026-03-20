#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <ranges>
#include <algorithm>
#include <format>

import video_streaming.interfaces;

using namespace video_streaming;

// C++26 тесты для интерфейсного модуля
TEST_CASE("Basic Type Aliases", "[interfaces]") {
    
    SECTION("String Type") {
        String str = "Hello, C++26!";
        REQUIRE(str == "Hello, C++26!");
        REQUIRE(str.size() == 14);
        
        // C++26: Perfect forwarding для String
        String moved_str = std::move(str);
        REQUIRE(moved_str == "Hello, C++26!");
        REQUIRE(str.empty()); // str был перемещен
    }
    
    SECTION("Vector Type") {
        Vector<int> vec = {1, 2, 3, 4, 5};
        REQUIRE(vec.size() == 5);
        REQUIRE(vec[0] == 1);
        REQUIRE(vec[4] == 5);
        
        // C++26: Использование ranges с Vector
        auto even_numbers = vec | std::views::filter([](int n) { return n % 2 == 0; });
        Vector<int> evens(even_numbers.begin(), even_numbers.end());
        REQUIRE(evens == Vector<int>{2, 4});
    }
    
    SECTION("UnorderedMap Type") {
        UnorderedMap<String, int> map = {
            {"one", 1},
            {"two", 2},
            {"three", 3}
        };
        
        REQUIRE(map.size() == 3);
        REQUIRE(map["one"] == 1);
        REQUIRE(map["two"] == 2);
        REQUIRE(map["three"] == 3);
        
        // C++26: Итерация по unordered_map с structured bindings
        for (const auto& [key, value] : map) {
            REQUIRE(!key.empty());
            REQUIRE(value > 0);
        }
    }
    
    SECTION("UniquePtr Type") {
        auto ptr = UniquePtr<int>(new int(42));
        REQUIRE(*ptr == 42);
        
        // C++26: Perfect forwarding для UniquePtr
        auto moved_ptr = std::move(ptr);
        REQUIRE(*moved_ptr == 42);
        REQUIRE(ptr == nullptr);
    }
    
    SECTION("SharedPtr Type") {
        auto ptr1 = SharedPtr<int>(new int(42));
        auto ptr2 = ptr1; // Копирование
        
        REQUIRE(ptr1.use_count() == 2);
        REQUIRE(ptr2.use_count() == 2);
        REQUIRE(*ptr1 == 42);
        REQUIRE(*ptr2 == 42);
        
        // C++26: Weak pointer для проверки циклических ссылок
        auto weak_ptr = WeakPtr<int>(ptr1);
        REQUIRE(!weak_ptr.expired());
        REQUIRE(weak_ptr.lock() != nullptr);
    }
}

TEST_CASE("C++26 Advanced Features", "[interfaces][c++26]") {
    
    SECTION("Template Type Deduction") {
        // C++26: Автоматическое выведение типов для шаблонных алиасов
        Vector numbers = {1, 2, 3, 4, 5};
        static_assert(std::is_same_v<decltype(numbers), Vector<int>>);
        
        auto map = UnorderedMap<String, int>{{"test", 42}};
        static_assert(std::is_same_v<decltype(map), UnorderedMap<String, int>>);
    }
    
    SECTION("Ranges Integration") {
        Vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        
        // C++26: Композиция ranges с нашими типами
        auto result = numbers 
            | std::views::filter([](int n) { return n % 2 == 0; })
            | std::views::transform([](int n) { return n * n; })
            | std::views::take(3);
        
        // C++20 ranges: use common view to match iterator/sentinel types for vector constructor
        auto common = result | std::views::common;
        Vector<int> squares(common.begin(), common.end());
        REQUIRE(squares == Vector<int>{4, 16, 36});
    }
    
    SECTION("Perfect Forwarding") {
        // C++26: Perfect forwarding для всех типов
        auto test_forwarding = [](auto&& arg) -> decltype(auto) {
            return std::forward<decltype(arg)>(arg);
        };
        
        String str = "test";
        auto& ref = test_forwarding(str);
        REQUIRE(&ref == &str);
        
        const String const_str = "const";
        auto&& const_ref = test_forwarding(const_str);
        REQUIRE(const_ref == "const");
    }
    
    SECTION("Memory Management") {
        // C++26: Современное управление памятью
        {
            auto unique = UniquePtr<Vector<int>>(new Vector<int>{1, 2, 3});
            REQUIRE(unique->size() == 3);
            
            // C++26: make_unique с perfect forwarding
            auto unique2 = UniquePtr<Vector<String>>(new Vector<String>{"a", "b", "c"});
            REQUIRE(unique2->size() == 3);
        }
        
        {
            auto shared = SharedPtr<String>(new String("shared"));
            REQUIRE(*shared == "shared");
            
            // C++26: make_shared с perfect forwarding
            auto shared2 = SharedPtr<UnorderedMap<int, String>>(
                new UnorderedMap<int, String>{{1, "one"}, {2, "two"}}
            );
            REQUIRE(shared2->size() == 2);
        }
    }
}

TEST_CASE("Type Safety and Constraints", "[interfaces][safety]") {
    
    SECTION("Type Compatibility") {
        // C++26: Проверка совместимости типов
        String str = "hello";
        Vector<String> strings = {"a", "b", "c"};
        
        // Должно компилироваться без проблем
        strings.push_back(str);
        REQUIRE(strings.size() == 4);
        REQUIRE(strings.back() == "hello");
    }
    
    SECTION("Move Semantics") {
        // C++26: Проверка move семантики
        Vector<String> strings;
        strings.reserve(100); // Избегаем реаллокаций
        
        for (int i = 0; i < 10; ++i) {
            strings.push_back(std::format("item_{}", i));
        }
        
        REQUIRE(strings.size() == 10);
        
        // Move операция
        Vector<String> moved_strings = std::move(strings);
        REQUIRE(moved_strings.size() == 10);
        REQUIRE(strings.empty()); // strings был перемещен
    }
    
    SECTION("Copy Semantics") {
        // C++26: Проверка copy семантики для SharedPtr
        auto original = SharedPtr<int>(new int(42));
        REQUIRE(original.use_count() == 1);
        
        {
            auto copy = original; // Copy
            REQUIRE(original.use_count() == 2);
            REQUIRE(copy.use_count() == 2);
            REQUIRE(*original == *copy);
        }
        
        REQUIRE(original.use_count() == 1); // copy уничтожен
    }
}

// C++26: Compile-time тесты
TEST_CASE("Compile-time Features", "[interfaces][constexpr]") {
    
    SECTION("Constexpr Operations") {
        // C++26: constexpr операции с нашими типами (transient allocation check)
        static_assert( []() consteval {
            Vector<int> v = {1, 2, 3};
            return v.size();
        }() == 3);
        
        // C++26: constexpr алгоритмы
        static_assert( []() consteval {
            Vector<int> v = {1, 2, 3};
            int total = 0;
            for (int n : v) total += n;
            return total;
        }() == 6);
    }
    
    SECTION("Template Constraints") {
        // C++26: Проверка constraint'ов
        static_assert(std::is_same_v<String, std::string>);
        static_assert(std::is_same_v<Vector<int>, std::vector<int>>);
        static_assert(std::is_same_v<UnorderedMap<String, int>, std::unordered_map<std::string, int>>);
    }
}
