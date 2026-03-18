package dev.hive.clion.config

import dev.hive.clion.config.model.HiveWorkspaceKind
import java.nio.file.Files
import java.nio.file.Path
import kotlin.io.path.writeText
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

class HiveWorkspaceResolverTest {
    @Test
    fun `detects engine workspace from HiveFeatures json`() {
        val root = createTempWorkspace("hive-plugin-engine")
        root.resolve("HiveFeatures.json").writeText("{}")
        root.resolve("CMakePresets.json").writeText("""{"version":6,"configurePresets":[]}""")

        val workspace = HiveWorkspaceResolver.resolve(root)

        assertEquals(HiveWorkspaceKind.ENGINE, workspace.kind)
        assertEquals(root.resolve("HiveFeatures.json"), workspace.featuresPath)
        assertEquals(root.resolve("CMakePresets.json"), workspace.cmakePresetsPath)
    }

    @Test
    fun `detects game project workspace from HiveProject json`() {
        val root = createTempWorkspace("hive-plugin-game")
        root.resolve("HiveProject.json").writeText(
            """
            {
              "version": 1,
              "projectName": "StandaloneGame",
              "supportedModes": ["Editor", "Headless"],
              "moduleDependencies": {
                "Swarm": true,
                "Terra": false
              },
              "presetNames": {
                "editor": "standalone-game-editor",
                "headless": "standalone-game-headless"
              }
            }
            """.trimIndent(),
        )

        val workspace = HiveWorkspaceResolver.resolve(root)

        assertEquals(HiveWorkspaceKind.GAME_PROJECT, workspace.kind)
        assertNotNull(workspace.projectManifest)
        assertEquals("StandaloneGame", workspace.projectManifest.projectName)
        assertEquals("standalone-game-editor", workspace.projectManifest.presetNames["editor"])
        assertNull(workspace.projectManifestError)
    }

    @Test
    fun `detects game project workspace from project hive without manifest`() {
        val root = createTempWorkspace("hive-plugin-project-file")
        root.resolve("project.hive").writeText("{}")

        val workspace = HiveWorkspaceResolver.resolve(root)

        assertEquals(HiveWorkspaceKind.GAME_PROJECT, workspace.kind)
        assertEquals(root.resolve("project.hive"), workspace.projectFilePath)
        assertNull(workspace.projectManifest)
    }

    @Test
    fun `returns unknown for unrelated workspace`() {
        val root = createTempWorkspace("hive-plugin-unknown")

        val workspace = HiveWorkspaceResolver.resolve(root)

        assertEquals(HiveWorkspaceKind.UNKNOWN, workspace.kind)
    }

    private fun createTempWorkspace(prefix: String): Path =
        Files.createTempDirectory(prefix)
}
