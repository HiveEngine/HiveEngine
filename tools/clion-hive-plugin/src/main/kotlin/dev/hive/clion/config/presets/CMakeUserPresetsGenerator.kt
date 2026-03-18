package dev.hive.clion.config.presets

import com.fasterxml.jackson.annotation.JsonInclude
import com.fasterxml.jackson.databind.DeserializationFeature
import com.fasterxml.jackson.databind.ObjectMapper
import com.fasterxml.jackson.databind.SerializationFeature
import com.fasterxml.jackson.module.kotlin.jacksonObjectMapper
import com.fasterxml.jackson.module.kotlin.readValue
import com.intellij.notification.NotificationGroupManager
import com.intellij.notification.NotificationType
import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.project.Project
import com.intellij.openapi.vfs.LocalFileSystem
import dev.hive.clion.config.model.FeatureAvailability
import dev.hive.clion.config.model.FeatureDefinition
import dev.hive.clion.config.model.HiveFeaturesFile
import dev.hive.clion.config.model.HiveUiState
import dev.hive.clion.config.model.CompilerFamily
import dev.hive.clion.config.model.ToolchainPlatform
import dev.hive.clion.config.model.ToolchainSnapshot
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import kotlin.io.path.invariantSeparatorsPathString
import kotlin.io.path.readText

object CMakeUserPresetsGenerator {
    private const val CONFIGURE_PRESET_VERSION = 6
    private const val ROOT_PRESETS_FILE = "CMakePresets.json"
    private const val USER_PRESETS_FILE = "CMakeUserPresets.json"
    private val mapper: ObjectMapper = jacksonObjectMapper()
        .disable(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES)
        .enable(SerializationFeature.INDENT_OUTPUT)
        .setSerializationInclusion(JsonInclude.Include.NON_NULL)

    fun loadCatalog(project: Project): HiveFeaturesFile {
        val root = project.basePath ?: error("Project has no base path")
        val featuresPath = Path.of(root, "HiveFeatures.json")
        return mapper.readValue(featuresPath.readText(StandardCharsets.UTF_8))
    }

    fun generate(project: Project, catalog: HiveFeaturesFile, uiState: HiveUiState, toolchain: ToolchainSnapshot): Path {
        val root = project.basePath ?: error("Project has no base path")
        val presetsPath = Path.of(root, USER_PRESETS_FILE)
        val availableBasePresets = loadBasePresetNames(root)
        val document = buildDocument(catalog, uiState, toolchain, availableBasePresets)

        Files.writeString(presetsPath, mapper.writeValueAsString(document) + System.lineSeparator(), StandardCharsets.UTF_8)
        refresh(path = presetsPath)
        return presetsPath
    }

    internal fun buildDocument(
        catalog: HiveFeaturesFile,
        uiState: HiveUiState,
        toolchain: ToolchainSnapshot,
        availableBasePresets: Set<String>,
    ): UserPresetsDocument {
        val featuresById = catalog.categories.flatMap { it.features }.associateBy { it.id }
        val inheritedBase = resolveBasePreset(toolchain, availableBasePresets)
        return UserPresetsDocument(
            version = CONFIGURE_PRESET_VERSION,
            configurePresets = catalog.engineModes.map { mode ->
                val config = uiState.modes.getValue(mode)
                UserConfigurePreset(
                    name = presetNameFor(mode),
                    inherits = inheritedBase,
                    displayName = "HiveEngine $mode",
                    description = catalog.presetDefaults[mode]?.description,
                    binaryDir = "\${sourceDir}/out/build/\${presetName}",
                    cacheVariables = linkedMapOf<String, String>().apply {
                        put("CMAKE_BUILD_TYPE", config.buildConfig)
                        put("HIVE_ENGINE_MODE", mode)
                        REQUIRED_FEATURE_IDS.forEach { featureId ->
                            val feature = featuresById[featureId] ?: return@forEach
                            feature.cmakeVar?.let { cmakeVar ->
                                put(cmakeVar, toCMakeValue(feature, config.features[feature.id]))
                            }
                        }
                    },
                    vendor = mapOf(
                        "dev.hive/HiveConfig" to linkedMapOf(
                            "toolchainPlatform" to toolchain.platform.name,
                            "compilerFamily" to toolchain.compilerFamily.name,
                        ),
                    ),
                )
            },
        )
    }

    fun computeAvailability(
        mode: String,
        feature: FeatureDefinition,
        values: Map<String, String>,
        toolchain: ToolchainSnapshot,
    ): FeatureAvailability {
        if (mode == "Headless" && feature.id in HEADLESS_DISABLED_FEATURES) {
            return FeatureAvailability(false, "Headless mode forces graphics options to OFF.")
        }

        val allowedPlatforms = feature.platforms
        if (allowedPlatforms.isNotEmpty()) {
            val supported = when (toolchain.platform) {
                ToolchainPlatform.WINDOWS -> "Windows" in allowedPlatforms
                ToolchainPlatform.LINUX -> "Linux" in allowedPlatforms
                ToolchainPlatform.MACOS -> "macOS" in allowedPlatforms || "MacOS" in allowedPlatforms || "Darwin" in allowedPlatforms
                ToolchainPlatform.UNKNOWN -> false
            }
            if (!supported) {
                return FeatureAvailability(false, "Unavailable on ${toolchain.summary()}.")
            }
        }

        if (feature.id == "enableSanitizers" && !toolchain.supportsSanitizers) {
            return FeatureAvailability(false, "Sanitizers are available on Linux with LLVM or GCC.")
        }

        if (feature.id == "sanitizerType") {
            if (!toolchain.supportsSanitizers || values["enableSanitizers"] != "true") {
                return FeatureAvailability(false, "Enable sanitizers first.")
            }

            val allowedOptions = feature.options.mapNotNullTo(linkedSetOf()) { option ->
                when (option.value) {
                    "Memory" -> option.value.takeIf { toolchain.supportsMemorySanitizer }
                    else -> option.value
                }
            }
            return FeatureAvailability(true, allowedOptionValues = allowedOptions)
        }

        if (feature.requires.any { values[it] != "true" }) {
            return FeatureAvailability(false, "Depends on: ${feature.requires.joinToString(", ")}.")
        }

        return FeatureAvailability(true)
    }

    fun normalize(catalog: HiveFeaturesFile, uiState: HiveUiState, toolchain: ToolchainSnapshot) {
        val features = catalog.categories.flatMap { it.features }
        for ((mode, config) in uiState.modes) {
            for (feature in features) {
                val availability = computeAvailability(mode, feature, config.features, toolchain)
                if (!availability.enabled) {
                    config.features[feature.id] = forcedValueFor(feature, mode)
                    continue
                }

                if (feature.type == "bool" && config.features[feature.id] == "true") {
                    feature.excludes.forEach { excludedId ->
                        config.features[excludedId] = "false"
                    }
                }

                if (feature.type == "enum") {
                    val currentValue = config.features[feature.id] ?: feature.defaultValueFor(mode)
                    val allowedOptions = availability.allowedOptionValues
                    if (allowedOptions != null && currentValue !in allowedOptions) {
                        config.features[feature.id] = allowedOptions.firstOrNull() ?: feature.defaultValueFor(mode)
                    }
                }
            }
        }
    }

    fun notifySuccess(project: Project, path: Path) {
        NotificationGroupManager.getInstance()
            .getNotificationGroup("Hive Config")
            .createNotification(
                "$USER_PRESETS_FILE regenerated",
                path.invariantSeparatorsPathString,
                NotificationType.INFORMATION,
            )
            .notify(project)
    }

    fun notifyFailure(project: Project, error: Throwable) {
        NotificationGroupManager.getInstance()
            .getNotificationGroup("Hive Config")
            .createNotification(
                "Failed to generate $USER_PRESETS_FILE",
                error.message ?: error::class.java.simpleName,
                NotificationType.ERROR,
            )
            .notify(project)
    }

    private fun loadBasePresetNames(root: String): Set<String> {
        val presetsPath = Path.of(root, ROOT_PRESETS_FILE)
        require(Files.exists(presetsPath)) {
            "$ROOT_PRESETS_FILE is missing from the project root."
        }

        val document = mapper.readValue<RootPresetsDocument>(presetsPath.readText(StandardCharsets.UTF_8))
        return document.configurePresets.mapTo(linkedSetOf()) { it.name }
    }

    private fun resolveBasePreset(toolchain: ToolchainSnapshot, availableBasePresets: Set<String>): String {
        val presetName = when (toolchain.platform) {
            ToolchainPlatform.WINDOWS -> when (toolchain.compilerFamily) {
                CompilerFamily.LLVM -> "llvm-windows-base"
                CompilerFamily.MSVC -> "msvc-windows-base"
                else -> error("Compiler family ${toolchain.compilerFamily.name} is unsupported on Windows.")
            }
            ToolchainPlatform.LINUX -> when (toolchain.compilerFamily) {
                CompilerFamily.LLVM -> "llvm-linux-base"
                CompilerFamily.GCC -> "gcc-linux-base"
                else -> error("Compiler family ${toolchain.compilerFamily.name} is unsupported on Linux.")
            }
            ToolchainPlatform.MACOS -> error("macOS is not supported by this Hive preset generator.")
            ToolchainPlatform.UNKNOWN -> error("Unable to detect the target platform from the active CLion toolchain.")
        }

        require(presetName in availableBasePresets) {
            "$ROOT_PRESETS_FILE is missing required base preset '$presetName'."
        }
        return presetName
    }

    private fun forcedValueFor(feature: FeatureDefinition, mode: String): String =
        if (feature.type == "bool") "false" else feature.defaultValueFor(mode)

    private fun presetNameFor(mode: String): String = "hive-${mode.lowercase()}"

    private fun toCMakeValue(feature: FeatureDefinition, value: String?): String =
        when (feature.type) {
            "bool" -> if (value == "true") "ON" else "OFF"
            else -> value ?: ""
        }

    private fun refresh(path: Path) {
        val file = LocalFileSystem.getInstance().refreshAndFindFileByPath(path.invariantSeparatorsPathString) ?: return
        ApplicationManager.getApplication().invokeLater {
            file.refresh(false, false)
            FileDocumentManager.getInstance().reloadFiles(file)
        }
    }

    private val HEADLESS_DISABLED_FEATURES = setOf("vulkan", "d3d12", "glfw", "imgui")
}

data class UserPresetsDocument(
    val version: Int,
    val configurePresets: List<UserConfigurePreset>,
)

data class UserConfigurePreset(
    val name: String,
    val inherits: String,
    val displayName: String,
    val description: String? = null,
    val binaryDir: String,
    val cacheVariables: LinkedHashMap<String, String>,
    val vendor: Map<String, Any>? = null,
)

data class RootPresetsDocument(
    val configurePresets: List<RootConfigurePreset> = emptyList(),
)

data class RootConfigurePreset(
    val name: String = "",
)

private val REQUIRED_FEATURE_IDS = listOf(
    "vulkan",
    "d3d12",
    "glfw",
    "imgui",
    "logging",
    "asserts",
    "profiler",
    "memDebug",
    "console",
    "hotReload",
    "enableSanitizers",
    "sanitizerType",
)
