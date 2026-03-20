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
        m_tests.push_back(std::move(test_info));
        return true;
    }

    TestRegistrar::TestRegistrar(const char* suite_name, const char* test_name, const std::function<void()>& func,
                                 const char* file, std::uint_least32_t line, CapabilityMask required_capabilities)
    {
        TestInfo info;
        info.m_suiteName = suite_name;
        info.m_testName = test_name;
        info.m_func = func;
        info.m_file = file;
        info.m_line = line;
        info.m_requiredCapabilities = required_capabilities;

        TestRegistry::GetInstance().Register(std::move(info));
    }
} // namespace larvae
