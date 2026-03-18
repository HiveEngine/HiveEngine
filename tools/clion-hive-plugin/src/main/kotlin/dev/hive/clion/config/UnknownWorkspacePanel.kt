package dev.hive.clion.config

import com.intellij.openapi.project.Project
import com.intellij.ui.components.ActionLink
import com.intellij.ui.components.JBLabel
import com.intellij.util.ui.JBFont
import com.intellij.util.ui.JBUI
import dev.hive.clion.config.model.HiveWorkspaceContext
import java.awt.FlowLayout
import java.awt.GridBagLayout
import javax.swing.Box
import javax.swing.BoxLayout
import javax.swing.JComponent
import javax.swing.JPanel

class UnknownWorkspacePanel(
    private val project: Project,
    private val workspace: HiveWorkspaceContext,
) {
    val component: JComponent

    init {
        val content = JPanel()
        content.layout = BoxLayout(content, BoxLayout.Y_AXIS)
        content.isOpaque = false
        content.border = JBUI.Borders.empty(10)
        content.add(compactWidth(createSummaryCard()))
        component = buildScrollableContent(content, createBottomBar())
    }

    private fun createSummaryCard(): JComponent {
        val card = createCardPanel()

        val title = JBLabel("Hive Config")
        title.font = JBFont.h2().asBold()
        card.add(title)

        val subtitle = secondaryLabel("No Hive engine or Hive game-project metadata was detected for this CLion workspace.")
        subtitle.border = JBUI.Borders.emptyTop(2)
        card.add(subtitle)
        card.add(Box.createVerticalStrut(JBUI.scale(12)))

        val controls = JPanel(GridBagLayout())
        controls.isOpaque = false
        var row = 0
        addFormRow(controls, row++, "Workspace", JBLabel("Unknown"))
        workspace.root?.let { addFormRow(controls, row++, "Root", pathLabel(it)) }
        addFormRow(controls, row++, "Engine Mode", JBLabel("Requires HiveFeatures.json"))
        addFormRow(controls, row++, "Game Mode", JBLabel("Requires project.hive or HiveProject.json"))

        card.add(controls)
        card.add(Box.createVerticalStrut(JBUI.scale(8)))
        card.add(secondaryLabel("Engine workspaces: HiveFeatures.json, CMakePresets.json"))
        card.add(secondaryLabel("Game projects: project.hive, HiveProject.json, CMakePresets.json"))

        return card
    }

    private fun createBottomBar(): JComponent {
        val links = JPanel(FlowLayout(FlowLayout.LEFT, JBUI.scale(12), 0))
        links.isOpaque = false
        workspace.cmakePresetsPath?.let { path ->
            links.add(ActionLink("Open CMakePresets.json") { openFile(project, path) })
        }
        return createBottomBarPanel(links, null)
    }
}
