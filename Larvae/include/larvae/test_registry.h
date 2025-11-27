#pragma once

#include <larvae/test_info.h>
#include <vector>
#include <memory>
#include <source_location>

namespace larvae
{
    class TestRegistry
    {
    public:
        static TestRegistry& GetInstance();
        bool Register(TestInfo&& test_info);
        const std::vector<TestInfo>& GetTests() const { return tests_; }
        void Clear() { tests_.clear(); }

    private:
        TestRegistry() = default;
        std::vector<TestInfo> tests_;
    };

    // Auto-registers tests at static initialization time
    class TestRegistrar
    {
    public:
        TestRegistrar(const char* suite_name,
                      const char* test_name,
                      const std::function<void()> &func,
                      const char* file,
                      std::uint_least32_t line);
    };

    // Helper to create and register a simple test
    // Usage: static auto t1 = larvae::RegisterTest("Suite", "Test", []() { /* test code */ });
    inline TestRegistrar RegisterTest(
        const char* suite_name,
        const char* test_name,
        std::function<void()> test_body,
        const std::source_location& loc = std::source_location::current())
    {
        return {suite_name, test_name, std::move(test_body), loc.file_name(), loc.line()};
    }

    // Helper template for tests with fixtures
    // Usage: static auto t2 = larvae::RegisterTestWithFixture<MyFixture>("Suite", "Test", [](MyFixture& f) { /* test */ });
    template<typename FixtureClass>
    TestRegistrar RegisterTestWithFixture(
        const char* suite_name,
        const char* test_name,
        std::function<void(FixtureClass&)> test_body,
        const std::source_location& loc = std::source_location::current())
    {
        auto wrapped_test = [test_body = std::move(test_body)]() {
            FixtureClass fixture;
            fixture.SetUp();
            test_body(fixture);
            fixture.TearDown();
        };

        return {suite_name, test_name, std::move(wrapped_test), loc.file_name(), loc.line()};
    }
}
