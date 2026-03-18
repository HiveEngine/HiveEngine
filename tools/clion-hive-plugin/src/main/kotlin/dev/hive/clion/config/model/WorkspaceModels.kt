package dev.hive.clion.config.model

import java.nio.file.Path

enum class HiveWorkspaceKind {
    ENGINE,
    GAME_PROJECT,
    UNKNOWN,
}

data class HiveProjectManifest(
    val version: Int = 1,
    val projectName: String = "",
    val supportedModes: List<String> = emptyList(),
    val moduleDependencies: Map<String, Boolean> = emptyMap(),
    val presetNames: Map<String, String> = emptyMap(),
)

data class HiveWorkspaceContext(
    val kind: HiveWorkspaceKind,
    val root: Path? = null,
    val featuresPath: Path? = null,
    val cmakePresetsPath: Path? = null,
    val userPresetsPath: Path? = null,
    val projectManifestPath: Path? = null,
    val projectFilePath: Path? = null,
    val projectManifest: HiveProjectManifest? = null,
    val projectManifestError: String? = null,
)
