#include <larvae/precomp.h>
#include <larvae/test_registry.h>

namespace larvae
{
    TestRegistry& TestRegistry::GetInstance()
    {
        static TestRegistry instance;
        return instance;
    }

    bool TestRegistry::Register(TestInfo&& test_info)
    {
        tests_.push_back(std::move(test_info));
        return true;
    }

    TestRegistrar::TestRegistrar(
        const char* suite_name,
        const char* test_name,
        const std::function<void()> &func,
        const char* file,
        std::uint_least32_t line)
    {
        TestInfo info;
        info.suite_name = suite_name;
        info.test_name = test_name;
        info.func = func;
        info.file = file;
        info.line = line;

        TestRegistry::GetInstance().Register(std::move(info));
    }
}
