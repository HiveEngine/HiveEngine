#include <hive/hive_config.h>
#include <hive/project/gameplay_api.h>
namespace queen { class World; }
HIVE_GAMEPLAY_EXPORT void HiveGameplayRegister(queen::World&) {
    int x = HiveTestPing();
    (void)x;
}
HIVE_GAMEPLAY_EXPORT void HiveGameplayUnregister(queen::World&) {}
HIVE_GAMEPLAY_EXPORT uint32_t HiveGameplayApiVersion() { return HIVE_GAMEPLAY_API_VERSION; }
HIVE_GAMEPLAY_EXPORT const char* HiveGameplayBuildSignature() { return HIVE_GAMEPLAY_BUILD_SIGNATURE; }
HIVE_GAMEPLAY_EXPORT const char* HiveGameplayVersion() { return "0.0.1"; }
extern "C" __declspec(dllexport) void TestFunction() {}
