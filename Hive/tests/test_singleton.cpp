#include <hive/utils/singleton.h>

#include <larvae/larvae.h>

namespace
{

    struct TestService : public hive::Singleton<TestService>
    {
        int m_value{42};
    };

    // Singleton lifecycle

    auto t_not_init = larvae::RegisterTest("HiveSingleton", "NotInitializedByDefault", []() {
        larvae::AssertFalse(TestService::IsInitialized());
    });

    auto t_create_and_get = larvae::RegisterTest("HiveSingleton", "CreateAndGetInstance", []() {
        {
            TestService svc;
            larvae::AssertTrue(TestService::IsInitialized());
            larvae::AssertEqual(&TestService::GetInstance(), &svc);
            larvae::AssertEqual(TestService::GetInstance().m_value, 42);
        }
        larvae::AssertFalse(TestService::IsInitialized());
    });

    auto t_destroy_clears = larvae::RegisterTest("HiveSingleton", "DestroyResetsInstance", []() {
        {
            TestService svc;
            larvae::AssertTrue(TestService::IsInitialized());
        }
        larvae::AssertFalse(TestService::IsInitialized());
    });

    auto t_second_ctor_ignored = larvae::RegisterTest("HiveSingleton", "SecondConstructorIgnored", []() {
        TestService first;
        first.m_value = 1;
        {
            TestService second;
            second.m_value = 2;
            // First instance wins
            larvae::AssertEqual(&TestService::GetInstance(), &first);
            larvae::AssertEqual(TestService::GetInstance().m_value, 1);
        }
        // First is still alive
        larvae::AssertTrue(TestService::IsInitialized());
        larvae::AssertEqual(TestService::GetInstance().m_value, 1);
    });

} // namespace
