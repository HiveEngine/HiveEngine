package dev.hive.clion.config

import com.intellij.openapi.project.Project
import dev.hive.clion.config.model.HiveWorkspaceKind
import javax.swing.JComponent

class HiveToolWindowPanel(project: Project) {
    private val workspace = HiveWorkspaceResolver.resolve(project)

    val component: JComponent = when (workspace.kind) {
        HiveWorkspaceKind.ENGINE -> EngineWorkspacePanel(project, workspace).component
        HiveWorkspaceKind.GAME_PROJECT -> GameProjectWorkspacePanel(project, workspace).component
        HiveWorkspaceKind.UNKNOWN -> UnknownWorkspacePanel(project, workspace).component
    }
}
