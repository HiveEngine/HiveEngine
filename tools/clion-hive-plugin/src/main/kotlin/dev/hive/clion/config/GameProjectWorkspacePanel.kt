package dev.hive.clion.config

import com.intellij.openapi.project.Project
import com.intellij.ui.JBColor
import com.intellij.ui.TitledSeparator
import com.intellij.ui.components.ActionLink
import com.intellij.ui.components.JBLabel
import com.intellij.util.ui.JBFont
import com.intellij.util.ui.JBUI
import dev.hive.clion.config.model.HiveWorkspaceContext
import dev.hive.clion.config.toolchain.ToolchainDetection
import java.awt.Component
import java.awt.FlowLayout
import java.awt.Font
import java.awt.GridBagLayout
import java.nio.file.Files
import java.nio.file.Path
import javax.swing.Box
import javax.swing.BoxLayout
import javax.swing.JComponent
import javax.swing.JPanel

class GameProjectWorkspacePanel(
    private val project: Project,
    private val workspace: HiveWorkspaceContext,
) {
    val component: JComponent

    private var toolchain = ToolchainDetection.detect(project)
    private val manifest = workspace.projectManifest

    init {
        val content = JPanel()
        content.layout = BoxLayout(content, BoxLayout.Y_AXIS)
        content.isOpaque = false
        content.border = JBUI.Borders.empty(10)
        content.add(compactWidth(createSummaryCard()))
        content.add(Box.createVerticalStrut(JBUI.scale(12)))
        content.add(compactWidth(createPresetCard()))
        content.add(Box.createVerticalStrut(JBUI.scale(12)))
        content.add(compactWidth(createFilesCard()))

        component = buildScrollableContent(content, createBottomBar())
    }

    private fun createSummaryCard(): JComponent {
        val card = createCardPanel()
        val title = JBLabel("Hive Config")
        title.font = JBFont.h2().asBold()
        card.add(title)

        val subtitle = secondaryLabel("Game project workspace detected. Use scaffolded project presets to build and run the launcher with the project gameplay module.")
        subtitle.border = JBUI.Borders.emptyTop(2)
        card.add(subtitle)
        card.add(Box.createVerticalStrut(JBUI.scale(12)))

        val controls = JPanel(GridBagLayout())
        controls.isOpaque = false
        var row = 0
        addFormRow(controls, row++, "Workspace", JBLabel("Game Project"))
        addFormRow(controls, row++, "Project", JBLabel(manifest?.projectName?.ifBlank { project.name } ?: project.name))
        addFormRow(controls, row++, "Modes", JBLabel(supportedModesLabel()))
        addFormRow(controls, row++, "Modules", JBLabel(enabledModulesLabel()))
        workspace.projectManifestPath?.let { addFormRow(controls, row++, "Manifest", pathLabel(it)) }
        workspace.projectFilePath?.let { addFormRow(controls, row++, "Project File", pathLabel(it)) }

        val detailsPanel = JPanel()
        detailsPanel.layout = BoxLayout(detailsPanel, BoxLayout.Y_AXIS)
        detailsPanel.isOpaque = false
        detailsPanel.border = JBUI.Borders.emptyTop(4)

        val toolchainLabel = JBLabel(toolchain.summary())
        toolchainLabel.font = JBFont.label().deriveFont(Font.BOLD)
        val toolchainSourceLabel = secondaryLabel("${toolchain.toolchainName} (${toolchain.detectionSource})")
        detailsPanel.add(toolchainLabel)
        detailsPanel.add(toolchainSourceLabel)

        manifestStatusLabel()?.let {
            detailsPanel.add(Box.createVerticalStrut(JBUI.scale(6)))
            detailsPanel.add(it)
        }

        card.add(controls)
        card.add(Box.createVerticalStrut(JBUI.scale(8)))
        card.add(TitledSeparator("Workspace Context"))
        card.add(Box.createVerticalStrut(JBUI.scale(4)))
        card.add(detailsPanel)

        return card
    }

    private fun createPresetCard(): JComponent {
        val card = createCardPanel()
        val title = JBLabel("Launcher Presets")
        title.font = JBFont.label().deriveFont(Font.BOLD)
        card.add(title)

        val subtitle = secondaryLabel("Configure presets come from HiveProject.json. Build and run presets are derived as `<preset>-build` and `<preset>-run`.")
        subtitle.border = JBUI.Borders.emptyTop(2)
        card.add(subtitle)
        card.add(Box.createVerticalStrut(JBUI.scale(10)))

        if (manifest == null || manifest.presetNames.isEmpty()) {
            card.add(secondaryLabel("No preset metadata available. Regenerate the project scaffold to restore HiveProject.json."))
            return card
        }

        manifest.supportedModes.forEachIndexed { index, mode ->
            val presetName = manifest.presetNames[mode.lowercase()]
            val row = JPanel()
            row.layout = BoxLayout(row, BoxLayout.Y_AXIS)
            row.isOpaque = false

            val header = JBLabel(mode)
            header.font = JBFont.label().deriveFont(Font.BOLD)
            row.add(header)
            row.add(secondaryLabel("Configure: ${presetName ?: "missing"}"))
            row.add(secondaryLabel("Build: ${presetName?.plus("-build") ?: "missing"}"))
            row.add(secondaryLabel("Run: ${presetName?.plus("-run") ?: "missing"}"))
            card.add(row)
            if (index != manifest.supportedModes.lastIndex) {
                card.add(Box.createVerticalStrut(JBUI.scale(10)))
            }
        }

        return card
    }

    private fun createFilesCard(): JComponent {
        val card = createCardPanel()
        val title = JBLabel("Project Files")
        title.font = JBFont.label().deriveFont(Font.BOLD)
        card.add(title)

        val subtitle = secondaryLabel("These files define the project contract used by CLion and the launcher.")
        subtitle.border = JBUI.Borders.emptyTop(2)
        card.add(subtitle)
        card.add(Box.createVerticalStrut(JBUI.scale(10)))

        addFileLink(card, "CMakePresets.json", workspace.cmakePresetsPath)
        addFileLink(card, "HiveProject.json", workspace.projectManifestPath)
        addFileLink(card, "project.hive", workspace.projectFilePath)

        if (workspace.cmakePresetsPath == null && workspace.projectManifestPath == null && workspace.projectFilePath == null) {
            card.add(secondaryLabel("No known Hive project files were found in the workspace root."))
        }

        return card
    }

    private fun createBottomBar(): JComponent {
        val links = JPanel(FlowLayout(FlowLayout.LEFT, JBUI.scale(12), 0))
        links.isOpaque = false
        workspace.cmakePresetsPath?.let { path ->
            links.add(ActionLink("Open CMakePresets.json") { openFile(project, path) })
        }
        workspace.projectManifestPath?.let { path ->
            links.add(ActionLink("Open HiveProject.json") { openFile(project, path) })
        }
        workspace.projectFilePath?.let { path ->
            links.add(ActionLink("Open project.hive") { openFile(project, path) })
        }
        return createBottomBarPanel(links, null)
    }

    private fun supportedModesLabel(): String =
        manifest?.supportedModes?.takeIf { it.isNotEmpty() }?.joinToString(", ") ?: "Unknown"

    private fun enabledModulesLabel(): String =
        manifest?.moduleDependencies
            ?.filterValues { it }
            ?.keys
            ?.takeIf { it.isNotEmpty() }
            ?.joinToString(", ")
            ?: "None"

    private fun manifestStatusLabel(): JBLabel? {
        workspace.projectManifestError?.let { error ->
            return secondaryLabel("HiveProject.json could not be parsed: $error").apply {
                foreground = JBColor.namedColor("Component.warningForeground", JBColor.GRAY)
            }
        }

        if (workspace.projectManifestPath == null) {
            return secondaryLabel("HiveProject.json is missing. Project presets cannot be summarized precisely.").apply {
                foreground = JBColor.namedColor("Component.warningForeground", JBColor.GRAY)
            }
        }

        return null
    }

    private fun addFileLink(card: JPanel, label: String, path: Path?) {
        if (path == null || !Files.exists(path)) {
            return
        }

        val row = JPanel(FlowLayout(FlowLayout.LEFT, JBUI.scale(8), 0))
        row.isOpaque = false
        row.alignmentX = Component.LEFT_ALIGNMENT
        row.add(JBLabel(label))
        row.add(ActionLink(path.fileName.toString()) { openFile(project, path) })
        card.add(row)
        card.add(Box.createVerticalStrut(JBUI.scale(6)))
    }
}
