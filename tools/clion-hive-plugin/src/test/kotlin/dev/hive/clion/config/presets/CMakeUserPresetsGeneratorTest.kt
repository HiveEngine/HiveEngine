package dev.hive.clion.config.presets

import dev.hive.clion.config.model.CompilerFamily
import dev.hive.clion.config.model.FeatureCategory
import dev.hive.clion.config.model.FeatureDefinition
import dev.hive.clion.config.model.FeatureOption
import dev.hive.clion.config.model.HiveFeaturesFile
import dev.hive.clion.config.model.HiveUiState
import dev.hive.clion.config.model.ModeConfiguration
import dev.hive.clion.config.model.PresetDefaults
import dev.hive.clion.config.model.ToolchainPlatform
import dev.hive.clion.config.model.ToolchainSnapshot
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class CMakeUserPresetsGeneratorTest {
    @Test
    fun `maps Windows LLVM to llvm-windows-base`() {
        val catalog = testCatalog()
        val uiState = defaultUiState(catalog)

        val document = CMakeUserPresetsGenerator.buildDocument(
            catalog = catalog,
            uiState = uiState,
            toolchain = ToolchainSnapshot(ToolchainPlatform.WINDOWS, CompilerFamily.LLVM, "LLVM", "test"),
            availableBasePresets = setOf("llvm-windows-base", "msvc-windows-base", "llvm-linux-base", "gcc-linux-base"),
        )

        assertEquals("llvm-windows-base", document.configurePresets.first { it.name == "hive-editor" }.inherits)
    }

    @Test
    fun `forces headless graphics off and maps Linux GCC to gcc-linux-base`() {
        val catalog = testCatalog()
        val uiState = defaultUiState(catalog)
        val headless = uiState.modes.getValue("Headless")
        headless.features["vulkan"] = "true"
        headless.features["d3d12"] = "true"
        headless.features["glfw"] = "true"
        headless.features["imgui"] = "true"

        val toolchain = ToolchainSnapshot(ToolchainPlatform.LINUX, CompilerFamily.GCC, "GCC", "test")
        CMakeUserPresetsGenerator.normalize(catalog, uiState, toolchain)

        val document = CMakeUserPresetsGenerator.buildDocument(
            catalog = catalog,
            uiState = uiState,
            toolchain = toolchain,
            availableBasePresets = setOf("llvm-windows-base", "msvc-windows-base", "llvm-linux-base", "gcc-linux-base"),
        )

        val preset = document.configurePresets.first { it.name == "hive-headless" }
        assertEquals("gcc-linux-base", preset.inherits)
        assertEquals("OFF", preset.cacheVariables.getValue("HIVE_FEATURE_VULKAN"))
        assertEquals("OFF", preset.cacheVariables.getValue("HIVE_FEATURE_D3D12"))
        assertEquals("OFF", preset.cacheVariables.getValue("HIVE_FEATURE_GLFW"))
        assertEquals("OFF", preset.cacheVariables.getValue("HIVE_FEATURE_IMGUI"))
    }

    @Test
    fun `fails with actionable message when root presets are missing the active toolchain base`() {
        val catalog = testCatalog()
        val uiState = defaultUiState(catalog)

        val error = assertFailsWith<IllegalArgumentException> {
            CMakeUserPresetsGenerator.buildDocument(
                catalog = catalog,
                uiState = uiState,
                toolchain = ToolchainSnapshot(ToolchainPlatform.WINDOWS, CompilerFamily.LLVM, "LLVM", "test"),
                availableBasePresets = emptySet(),
            )
        }

        assertEquals("CMakePresets.json is missing required base preset 'llvm-windows-base'.", error.message)
    }

    private fun defaultUiState(catalog: HiveFeaturesFile): HiveUiState {
        val modes = linkedMapOf<String, ModeConfiguration>()
        for (mode in catalog.engineModes) {
            modes[mode] = ModeConfiguration(
                buildConfig = catalog.presetDefaults[mode]?.buildType ?: "Debug",
                features = catalog.categories.flatMap { it.features }
                    .associate { it.id to it.defaultValueFor(mode) }
                    .toMutableMap(),
            )
        }
        return HiveUiState("Editor", modes)
    }

    private fun testCatalog(): HiveFeaturesFile =
        HiveFeaturesFile(
            version = 1,
            engineModes = listOf("Editor", "Game", "Headless"),
            buildConfigs = listOf("Debug", "Release", "Profile", "Retail"),
            categories = listOf(
                FeatureCategory(
                    id = "graphics",
                    name = "Graphics",
                    features = listOf(
                        boolFeature("vulkan", "HIVE_FEATURE_VULKAN", defaults = mapOf("Editor" to true, "Game" to true, "Headless" to false)),
                        boolFeature(
                            "d3d12",
                            "HIVE_FEATURE_D3D12",
                            platforms = listOf("Windows"),
                            defaults = mapOf("Editor" to false, "Game" to false, "Headless" to false),
                        ),
                        boolFeature("glfw", "HIVE_FEATURE_GLFW", defaults = mapOf("Editor" to true, "Game" to true, "Headless" to false)),
                        boolFeature("imgui", "HIVE_FEATURE_IMGUI", requires = listOf("glfw"), defaults = mapOf("Editor" to true, "Game" to false, "Headless" to false)),
                    ),
                ),
                FeatureCategory(
                    id = "development",
                    name = "Development",
                    features = listOf(
                        boolFeature("logging", "HIVE_FEATURE_LOGGING", defaults = mapOf("Editor" to true, "Game" to true, "Headless" to true)),
                        boolFeature("asserts", "HIVE_FEATURE_ASSERTS", defaults = mapOf("Editor" to true, "Game" to true, "Headless" to true)),
                        boolFeature("profiler", "HIVE_FEATURE_PROFILER", defaults = mapOf("Editor" to true, "Game" to true, "Headless" to true)),
                        boolFeature("memDebug", "HIVE_FEATURE_MEM_DEBUG", defaults = mapOf("Editor" to true, "Game" to false, "Headless" to false)),
                        boolFeature("console", "HIVE_FEATURE_CONSOLE", defaults = mapOf("Editor" to true, "Game" to false, "Headless" to true)),
                        boolFeature("hotReload", "HIVE_FEATURE_HOT_RELOAD", defaults = mapOf("Editor" to true, "Game" to false, "Headless" to false)),
                    ),
                ),
                FeatureCategory(
                    id = "sanitizers",
                    name = "Sanitizers",
                    features = listOf(
                        boolFeature("enableSanitizers", "HIVE_ENABLE_SANITIZERS", defaults = mapOf("Editor" to false, "Game" to false, "Headless" to false)),
                        enumFeature(
                            id = "sanitizerType",
                            cmakeVar = "HIVE_SANITIZER_TYPE",
                            defaults = mapOf("Editor" to "Address", "Game" to "Address", "Headless" to "Address"),
                            options = listOf("Address", "UndefinedBehavior", "Thread", "Memory"),
                            requires = listOf("enableSanitizers"),
                        ),
                    ),
                ),
            ),
            presetDefaults = mapOf(
                "Editor" to PresetDefaults("Debug", "Editor"),
                "Game" to PresetDefaults("Release", "Game"),
                "Headless" to PresetDefaults("Debug", "Headless"),
            ),
        )

    private fun boolFeature(
        id: String,
        cmakeVar: String,
        platforms: List<String> = emptyList(),
        requires: List<String> = emptyList(),
        defaults: Map<String, Boolean>,
    ): FeatureDefinition =
        FeatureDefinition(
            id = id,
            name = id,
            cmakeVar = cmakeVar,
            type = "bool",
            platforms = platforms,
            requires = requires,
            defaults = defaults,
        )

    private fun enumFeature(
        id: String,
        cmakeVar: String,
        defaults: Map<String, String>,
        options: List<String>,
        requires: List<String> = emptyList(),
    ): FeatureDefinition =
        FeatureDefinition(
            id = id,
            name = id,
            cmakeVar = cmakeVar,
            type = "enum",
            requires = requires,
            defaults = defaults,
            options = options.map { FeatureOption(it, it, it) },
        )
}
