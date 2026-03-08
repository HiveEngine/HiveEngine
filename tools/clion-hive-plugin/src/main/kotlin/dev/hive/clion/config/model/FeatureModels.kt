package dev.hive.clion.config.model

data class HiveFeaturesFile(
    val version: Int = 1,
    val engineModes: List<String> = emptyList(),
    val buildConfigs: List<String> = emptyList(),
    val categories: List<FeatureCategory> = emptyList(),
    val presetDefaults: Map<String, PresetDefaults> = emptyMap(),
)

data class FeatureCategory(
    val id: String = "",
    val name: String = "",
    val description: String? = null,
    val features: List<FeatureDefinition> = emptyList(),
)

data class FeatureDefinition(
    val id: String = "",
    val name: String = "",
    val description: String? = null,
    val cmakeVar: String? = null,
    val macro: String? = null,
    val type: String = "bool",
    val excludes: List<String> = emptyList(),
    val requires: List<String> = emptyList(),
    val platforms: List<String> = emptyList(),
    val defaults: Map<String, Any?> = emptyMap(),
    val options: List<FeatureOption> = emptyList(),
) {
    fun defaultValueFor(mode: String): String {
        val value = defaults[mode]
        return when (value) {
            is Boolean -> value.toString()
            null -> if (type == "bool") "false" else options.firstOrNull()?.value.orEmpty()
            else -> value.toString()
        }
    }
}

data class FeatureOption(
    val value: String = "",
    val name: String = "",
    val description: String? = null,
)

data class PresetDefaults(
    val buildType: String = "Debug",
    val description: String = "",
)

data class ModeConfiguration(
    var buildConfig: String,
    val features: MutableMap<String, String>,
)

data class HiveUiState(
    var selectedMode: String,
    val modes: MutableMap<String, ModeConfiguration>,
)

enum class ToolchainPlatform {
    WINDOWS,
    LINUX,
    MACOS,
    UNKNOWN,
}

enum class CompilerFamily {
    LLVM,
    GCC,
    MSVC,
    UNKNOWN,
}

data class ToolchainSnapshot(
    val platform: ToolchainPlatform,
    val compilerFamily: CompilerFamily,
    val toolchainName: String,
    val detectionSource: String,
) {
    val supportsD3D12: Boolean
        get() = platform == ToolchainPlatform.WINDOWS

    val supportsSanitizers: Boolean
        get() = platform == ToolchainPlatform.LINUX && compilerFamily in setOf(CompilerFamily.LLVM, CompilerFamily.GCC)

    val supportsMemorySanitizer: Boolean
        get() = platform == ToolchainPlatform.LINUX && compilerFamily == CompilerFamily.LLVM

    fun summary(): String {
        val platformName = when (platform) {
            ToolchainPlatform.WINDOWS -> "Windows"
            ToolchainPlatform.LINUX -> "Linux"
            ToolchainPlatform.MACOS -> "macOS"
            ToolchainPlatform.UNKNOWN -> "Unknown OS"
        }
        val compilerName = when (compilerFamily) {
            CompilerFamily.LLVM -> "LLVM/Clang"
            CompilerFamily.GCC -> "GCC"
            CompilerFamily.MSVC -> "MSVC"
            CompilerFamily.UNKNOWN -> "Unknown compiler"
        }
        return "$platformName - $compilerName"
    }
}

data class FeatureAvailability(
    val enabled: Boolean,
    val reason: String? = null,
    val allowedOptionValues: Set<String>? = null,
)
