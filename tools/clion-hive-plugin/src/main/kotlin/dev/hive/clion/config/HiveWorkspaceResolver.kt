package dev.hive.clion.config

import com.fasterxml.jackson.databind.DeserializationFeature
import com.fasterxml.jackson.databind.ObjectMapper
import com.fasterxml.jackson.module.kotlin.jacksonObjectMapper
import com.fasterxml.jackson.module.kotlin.readValue
import com.intellij.openapi.project.Project
import dev.hive.clion.config.model.HiveProjectManifest
import dev.hive.clion.config.model.HiveWorkspaceContext
import dev.hive.clion.config.model.HiveWorkspaceKind
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import kotlin.io.path.readText

object HiveWorkspaceResolver {
    private val mapper: ObjectMapper = jacksonObjectMapper()
        .disable(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES)

    fun resolve(project: Project): HiveWorkspaceContext =
        resolve(project.basePath?.let(Path::of))

    internal fun resolve(root: Path?): HiveWorkspaceContext {
        if (root == null) {
            return HiveWorkspaceContext(kind = HiveWorkspaceKind.UNKNOWN)
        }

        val featuresPath = existingPath(root.resolve("HiveFeatures.json"))
        val cmakePresetsPath = existingPath(root.resolve("CMakePresets.json"))
        val userPresetsPath = existingPath(root.resolve("CMakeUserPresets.json"))
        val projectManifestPath = existingPath(root.resolve("HiveProject.json"))
        val projectFilePath = existingPath(root.resolve("project.hive"))

        if (featuresPath != null) {
            return HiveWorkspaceContext(
                kind = HiveWorkspaceKind.ENGINE,
                root = root,
                featuresPath = featuresPath,
                cmakePresetsPath = cmakePresetsPath,
                userPresetsPath = userPresetsPath,
            )
        }

        if (projectManifestPath != null || projectFilePath != null) {
            val manifestResult = loadProjectManifest(projectManifestPath)
            return HiveWorkspaceContext(
                kind = HiveWorkspaceKind.GAME_PROJECT,
                root = root,
                cmakePresetsPath = cmakePresetsPath,
                projectManifestPath = projectManifestPath,
                projectFilePath = projectFilePath,
                projectManifest = manifestResult.first,
                projectManifestError = manifestResult.second,
            )
        }

        return HiveWorkspaceContext(
            kind = HiveWorkspaceKind.UNKNOWN,
            root = root,
            cmakePresetsPath = cmakePresetsPath,
            userPresetsPath = userPresetsPath,
        )
    }

    private fun existingPath(path: Path): Path? =
        path.takeIf(Files::exists)

    private fun loadProjectManifest(path: Path?): Pair<HiveProjectManifest?, String?> {
        if (path == null) {
            return Pair(null, null)
        }

        return runCatching {
            mapper.readValue<HiveProjectManifest>(path.readText(StandardCharsets.UTF_8))
        }.fold(
            onSuccess = { Pair(it, null) },
            onFailure = { Pair(null, it.message ?: it::class.java.simpleName) },
        )
    }
}
