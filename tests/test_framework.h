#pragma once
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace testing {

struct TestCase { std::string name; std::function<void()> fn; };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> t; return t; }
struct Registrar { Registrar(const std::string& n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };
struct Fail { std::string msg; };

inline int runAll() {
    const char* green = "\033[32m";
    const char* red = "\033[31m";
    const char* reset = "\033[0m";
    int failed = 0;
    for (auto& t : registry()) {
        try { t.fn(); std::cout << green << "[PASS] " << t.name << reset << "\n"; }
        catch (const Fail& f) { std::cout << red << "[FAIL] " << t.name << ": " << f.msg << reset << "\n"; failed++; }
        catch (const std::exception& e) { std::cout << red << "[FAIL] " << t.name << ": " << e.what() << reset << "\n"; failed++; }
    }
    const char* summaryColor = failed ? red : green;
    std::cout << summaryColor << (registry().size() - failed) << "/" << registry().size() << " passed" << reset << "\n";
    return failed;
}

} // namespace testing

#define TEST(name) void name(); static testing::Registrar reg_##name(#name, name); void name()
#define ASSERT_TRUE(c) do { if (!(c)) throw testing::Fail{"ASSERT_TRUE failed: " #c}; } while (0)
#define ASSERT_EQ(a, b) do { if (!((a) == (b))) throw testing::Fail{#a " != " #b}; } while (0)