#include <hive/project/gameplay_api.h>
#include <hive/core/log.h>

namespace queen { class World; }

static const hive::LogCategory LogSponza{"SponzaDemo"};

HIVE_GAMEPLAY_EXPORT void HiveGameplayRegister(queen::World& /*world*/)
{
    hive::LogInfo(LogSponza, "Sponza Demo registered");
}

HIVE_GAMEPLAY_EXPORT void HiveGameplayUnregister(queen::World& /*world*/)
{
    hive::LogInfo(LogSponza, "Sponza Demo unregistered");
}

HIVE_GAMEPLAY_EXPORT const char* HiveGameplayVersion()
{
    return "0.1.0";
}
