#include <hive/core/log.h>
#include <hive/project/gameplay_api.h>

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>

#include <nectar/hive/hive_document.h>
#include <nectar/hive/hive_parser.h>
#include <nectar/hive/hive_value.h>
#include <nectar/hive/hive_writer.h>
#include <nectar/vfs/virtual_filesystem.h>

#include <queen/query/query_term.h>
#include <queen/world/world.h>

#include <waggle/app_context.h>
#include <waggle/project/project_context.h>
#include <waggle/project/project_manager.h>
#include <waggle/runtime_context.h>
#include <waggle/time.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace
{
    static const hive::LogCategory LOG_SPONZA{"SponzaDemo"};
    constexpr const char* kScenarioAssetPath = "headless_simulation.hive";
    constexpr const char* kReportFileName = "headless_report.hive";
    constexpr uint32_t kDefaultEntityCount = 3;
    constexpr uint32_t kDefaultStepCount = 5;
    constexpr float kDefaultInitialPosition = 0.0f;
    constexpr float kDefaultSpawnSpacing = 10.0f;
    constexpr float kDefaultVelocityPerSecond = 60.0f;

    struct HeadlessAgent
    {
        uint32_t m_index{};
    };

    struct SimulationPosition
    {
        float m_value{};
    };

    struct SimulationVelocity
    {
        float m_unitsPerSecond{};
    };

    struct HeadlessScenarioState
    {
        bool m_bootstrapped{false};
        bool m_reportWritten{false};
        bool m_loadedFromAsset{false};
        uint32_t m_entityCount{kDefaultEntityCount};
        uint32_t m_stepCount{kDefaultStepCount};
        float m_initialPosition{kDefaultInitialPosition};
        float m_spawnSpacing{kDefaultSpawnSpacing};
        float m_velocityPerSecond{kDefaultVelocityPerSecond};
        uint64_t m_stopAfterTick{kDefaultStepCount};
    };

    struct HeadlessMetrics
    {
        uint32_t m_entityCount{0};
        float m_positionSum{0.0f};
        float m_minPosition{0.0f};
        float m_maxPosition{0.0f};
        uint64_t m_completedTick{0};
    };

    comb::DefaultAllocator& GetGameplayAllocator()
    {
        static comb::ModuleAllocator s_alloc{"SponzaDemoGameplay", 4 * 1024 * 1024};
        return s_alloc.Get();
    }

    void ClampScenarioState(HeadlessScenarioState& state)
    {
        state.m_entityCount = (std::max)(state.m_entityCount, 1u);
        state.m_stepCount = (std::max)(state.m_stepCount, 1u);
        state.m_stopAfterTick = state.m_stepCount;
    }

    bool LoadScenarioFromAssets(waggle::ProjectManager& projectManager, HeadlessScenarioState& state)
    {
        auto buffer = projectManager.VFS().ReadSync(kScenarioAssetPath);
        if (buffer.IsEmpty())
        {
            hive::LogWarning(LOG_SPONZA, "Scenario asset '{}' was not found, using defaults", kScenarioAssetPath);
            ClampScenarioState(state);
            return false;
        }

        auto& alloc = GetGameplayAllocator();
        const wax::StringView content{reinterpret_cast<const char*>(buffer.Data()), buffer.Size()};
        nectar::HiveParseResult result = nectar::HiveParser::Parse(content, alloc);
        if (!result.m_errors.IsEmpty())
        {
            hive::LogWarning(LOG_SPONZA, "Scenario asset '{}' could not be parsed, using defaults", kScenarioAssetPath);
            ClampScenarioState(state);
            return false;
        }

        const nectar::HiveDocument& doc = result.m_document;
        state.m_entityCount =
            static_cast<uint32_t>(doc.GetInt("simulation", "entity_count", static_cast<int64_t>(state.m_entityCount)));
        state.m_stepCount =
            static_cast<uint32_t>(doc.GetInt("simulation", "step_count", static_cast<int64_t>(state.m_stepCount)));
        state.m_initialPosition =
            static_cast<float>(doc.GetFloat("simulation", "initial_position", state.m_initialPosition));
        state.m_spawnSpacing = static_cast<float>(doc.GetFloat("simulation", "spawn_spacing", state.m_spawnSpacing));
        state.m_velocityPerSecond =
            static_cast<float>(doc.GetFloat("simulation", "velocity_per_second", state.m_velocityPerSecond));
        state.m_loadedFromAsset = true;
        ClampScenarioState(state);
        return true;
    }

    HeadlessMetrics CollectMetrics(queen::World& world, uint64_t completedTick)
    {
        HeadlessMetrics metrics{};
        metrics.m_completedTick = completedTick;

        bool first = true;
        world.Query<queen::Read<HeadlessAgent>, queen::Read<SimulationPosition>>().Each(
            [&](const HeadlessAgent&, const SimulationPosition& position) {
                metrics.m_positionSum += position.m_value;
                if (first)
                {
                    metrics.m_minPosition = position.m_value;
                    metrics.m_maxPosition = position.m_value;
                    first = false;
                }
                else
                {
                    metrics.m_minPosition = (std::min)(metrics.m_minPosition, position.m_value);
                    metrics.m_maxPosition = (std::max)(metrics.m_maxPosition, position.m_value);
                }

                ++metrics.m_entityCount;
            });

        return metrics;
    }

    bool WriteReport(const waggle::ProjectManager& projectManager, const HeadlessScenarioState& scenario,
                     const HeadlessMetrics& metrics)
    {
        auto& alloc = GetGameplayAllocator();
        nectar::HiveDocument report{alloc};

        report.SetValue(
            "scenario", "source",
            nectar::HiveValue::MakeString(alloc, scenario.m_loadedFromAsset ? wax::StringView{kScenarioAssetPath}
                                                                            : wax::StringView{"defaults"}));
        report.SetValue("scenario", "entity_count", nectar::HiveValue::MakeInt(static_cast<int64_t>(scenario.m_entityCount)));
        report.SetValue("scenario", "step_count", nectar::HiveValue::MakeInt(static_cast<int64_t>(scenario.m_stepCount)));
        report.SetValue("scenario", "initial_position", nectar::HiveValue::MakeFloat(scenario.m_initialPosition));
        report.SetValue("scenario", "spawn_spacing", nectar::HiveValue::MakeFloat(scenario.m_spawnSpacing));
        report.SetValue("scenario", "velocity_per_second",
                        nectar::HiveValue::MakeFloat(scenario.m_velocityPerSecond));

        report.SetValue("result", "entity_count", nectar::HiveValue::MakeInt(static_cast<int64_t>(metrics.m_entityCount)));
        report.SetValue("result", "completed_tick",
                        nectar::HiveValue::MakeInt(static_cast<int64_t>(metrics.m_completedTick)));
        report.SetValue("result", "position_sum", nectar::HiveValue::MakeFloat(metrics.m_positionSum));
        report.SetValue("result", "min_position", nectar::HiveValue::MakeFloat(metrics.m_minPosition));
        report.SetValue("result", "max_position", nectar::HiveValue::MakeFloat(metrics.m_maxPosition));

        const wax::String content = nectar::HiveWriter::Write(report, alloc);
        std::filesystem::path reportPath{projectManager.Paths().m_cache.CStr()};
        reportPath /= kReportFileName;

        std::error_code ec;
        std::filesystem::create_directories(reportPath.parent_path(), ec);

        std::ofstream file{reportPath, std::ios::binary};
        if (!file)
        {
            return false;
        }

        file.write(content.CStr(), static_cast<std::streamsize>(content.Size()));
        return file.good();
    }

    void RegisterBootstrapSystem(queen::World& world)
    {
        world.System("SponzaDemo.Bootstrap").Run([](queen::World& runtimeWorld) {
            HeadlessScenarioState* scenario = runtimeWorld.Resource<HeadlessScenarioState>();
            waggle::ProjectContext* projectContext = runtimeWorld.Resource<waggle::ProjectContext>();
            if (scenario == nullptr || scenario->m_bootstrapped || projectContext == nullptr ||
                projectContext->m_manager == nullptr)
            {
                return;
            }

            LoadScenarioFromAssets(*projectContext->m_manager, *scenario);

            for (uint32_t index = 0; index < scenario->m_entityCount; ++index)
            {
                (void)runtimeWorld.Spawn(
                    HeadlessAgent{index},
                    SimulationPosition{scenario->m_initialPosition + static_cast<float>(index) * scenario->m_spawnSpacing},
                    SimulationVelocity{scenario->m_velocityPerSecond});
            }

            scenario->m_bootstrapped = true;
        });
    }

    void RegisterSimulationSystem(queen::World& world)
    {
        world.System<queen::Read<HeadlessAgent>, queen::Write<SimulationPosition>, queen::Read<SimulationVelocity>>(
                 "SponzaDemo.Step")
            .After("SponzaDemo.Bootstrap")
            .EachWithRes<waggle::Time>([](queen::Entity, const HeadlessAgent&, SimulationPosition& position,
                                          const SimulationVelocity& velocity, queen::Res<waggle::Time> time) {
                position.m_value += velocity.m_unitsPerSecond * time->m_dt;
            });
    }

    void RegisterFinalizeSystem(queen::World& world)
    {
        world.System("SponzaDemo.Finalize").After("SponzaDemo.Step").Run([](queen::World& runtimeWorld) {
            HeadlessScenarioState* scenario = runtimeWorld.Resource<HeadlessScenarioState>();
            HeadlessMetrics* metrics = runtimeWorld.Resource<HeadlessMetrics>();
            waggle::Time* time = runtimeWorld.Resource<waggle::Time>();
            waggle::ProjectContext* projectContext = runtimeWorld.Resource<waggle::ProjectContext>();
            waggle::AppContext* appContext = runtimeWorld.Resource<waggle::AppContext>();

            if (scenario == nullptr || metrics == nullptr || time == nullptr || projectContext == nullptr ||
                projectContext->m_manager == nullptr || scenario->m_reportWritten || !scenario->m_bootstrapped ||
                time->m_tick < scenario->m_stopAfterTick)
            {
                return;
            }

            *metrics = CollectMetrics(runtimeWorld, time->m_tick);
            if (!WriteReport(*projectContext->m_manager, *scenario, *metrics))
            {
                hive::LogWarning(LOG_SPONZA, "Failed to write headless simulation report");
            }

            scenario->m_reportWritten = true;

            if (appContext != nullptr && appContext->m_app != nullptr)
            {
                appContext->m_app->RequestStop();
            }
        });
    }
} // namespace

HIVE_GAMEPLAY_EXPORT void HiveGameplayRegister(queen::World& world)
{
    const waggle::RuntimeContext* runtime = world.Resource<waggle::RuntimeContext>();
    if (runtime == nullptr || runtime->m_mode != waggle::EngineMode::HEADLESS)
    {
        hive::LogInfo(LOG_SPONZA, "Sponza Demo registered with headless simulation disabled");
        return;
    }

    world.InsertResource(HeadlessScenarioState{});
    world.InsertResource(HeadlessMetrics{});

    RegisterBootstrapSystem(world);
    RegisterSimulationSystem(world);
    RegisterFinalizeSystem(world);

    hive::LogInfo(LOG_SPONZA, "Sponza Demo registered");
}

HIVE_GAMEPLAY_EXPORT void HiveGameplayUnregister(queen::World& world)
{
    if (world.HasResource<HeadlessMetrics>())
    {
        world.RemoveResource<HeadlessMetrics>();
    }

    if (world.HasResource<HeadlessScenarioState>())
    {
        world.RemoveResource<HeadlessScenarioState>();
    }

    hive::LogInfo(LOG_SPONZA, "Sponza Demo unregistered");
}

HIVE_GAMEPLAY_EXPORT uint32_t HiveGameplayApiVersion()
{
    return HIVE_GAMEPLAY_API_VERSION;
}

HIVE_GAMEPLAY_EXPORT const char* HiveGameplayBuildSignature()
{
    return HIVE_GAMEPLAY_BUILD_SIGNATURE;
}

HIVE_GAMEPLAY_EXPORT const char* HiveGameplayVersion()
{
    return "0.2.0";
}
