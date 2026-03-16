package dev.hive.clion.config

import com.intellij.openapi.project.Project
import com.intellij.ui.JBColor
import com.intellij.ui.TitledSeparator
import com.intellij.ui.components.ActionLink
import com.intellij.ui.components.JBCheckBox
import com.intellij.ui.components.JBLabel
import com.intellij.util.ui.JBFont
import com.intellij.util.ui.JBUI
import dev.hive.clion.config.model.CompilerFamily
import dev.hive.clion.config.model.FeatureCategory
import dev.hive.clion.config.model.FeatureDefinition
import dev.hive.clion.config.model.HiveFeaturesFile
import dev.hive.clion.config.model.HiveUiState
import dev.hive.clion.config.model.HiveWorkspaceContext
import dev.hive.clion.config.model.ModeConfiguration
import dev.hive.clion.config.model.ToolchainPlatform
import dev.hive.clion.config.presets.CMakeUserPresetsGenerator
import dev.hive.clion.config.state.HiveProjectSettingsService
import dev.hive.clion.config.toolchain.ToolchainDetection
import java.awt.BorderLayout
import java.awt.Component
import java.awt.Dimension
import java.awt.FlowLayout
import java.awt.Font
import java.awt.GridBagLayout
import java.awt.event.ItemEvent
import javax.swing.Box
import javax.swing.BoxLayout
import javax.swing.ButtonGroup
import javax.swing.JButton
import javax.swing.JComboBox
import javax.swing.DefaultComboBoxModel
import javax.swing.JComponent
import javax.swing.JPanel

class EngineWorkspacePanel(
    private val project: Project,
    private val workspace: HiveWorkspaceContext,
) {
    val component: JComponent

    private val settingsService = HiveProjectSettingsService.getInstance(project)
    private val catalog: HiveFeaturesFile
    private val uiState: HiveUiState
    private var toolchain = ToolchainDetection.detect(project)
    private var selectedMode: String
    private var updatingUi = false

    private val modeSelectorPanel = JPanel(FlowLayout(FlowLayout.LEFT, JBUI.scale(8), 0))
    private val buildSelectorPanel = JPanel(FlowLayout(FlowLayout.LEFT, JBUI.scale(8), 0))
    private val toolchainLabel = JBLabel()
    private val toolchainSourceLabel = JBLabel()
    private val basePresetLabel = JBLabel()
    private val capabilityLabel = JBLabel()
    private val groupsPanel = JPanel()
    private val featureControls = linkedMapOf<String, FeatureControl>()
    private val modeButtons = linkedMapOf<String, javax.swing.JRadioButton>()
    private val buildButtons = linkedMapOf<String, javax.swing.JRadioButton>()

    init {
        catalog = loadCatalogOrFallback()
        uiState = settingsService.loadUiState(catalog)
        selectedMode = uiState.selectedMode.ifBlank { catalog.engineModes.firstOrNull().orEmpty() }
        uiState.selectedMode = selectedMode
        CMakeUserPresetsGenerator.normalize(catalog, uiState, toolchain)

        configureSelectors()

        groupsPanel.layout = BoxLayout(groupsPanel, BoxLayout.Y_AXIS)
        groupsPanel.isOpaque = false
        rebuildFeatureGroups()

        val content = JPanel()
        content.layout = BoxLayout(content, BoxLayout.Y_AXIS)
        content.isOpaque = false
        content.border = JBUI.Borders.empty(10)
        content.add(compactWidth(createSummaryCard()))
        content.add(Box.createVerticalStrut(JBUI.scale(12)))
        content.add(groupsPanel)

        component = buildScrollableContent(content, createBottomBar())

        updateHeader()
        reloadMode(selectedMode)
    }

    private fun configureSelectors() {
        modeSelectorPanel.isOpaque = false
        buildSelectorPanel.isOpaque = false

        val modeGroup = ButtonGroup()
        catalog.engineModes.forEach { mode ->
            val button = createSelectorRadio(mode) {
                if (!updatingUi) {
                    selectedMode = mode
                    uiState.selectedMode = selectedMode
                    reloadMode(selectedMode)
                    persistState()
                }
            }
            modeGroup.add(button)
            modeButtons[mode] = button
            modeSelectorPanel.add(button)
        }

        val buildGroup = ButtonGroup()
        catalog.buildConfigs.forEach { build ->
            val button = createSelectorRadio(build) {
                if (!updatingUi) {
                    currentModeConfig().buildConfig = build
                    persistState()
                }
            }
            buildGroup.add(button)
            buildButtons[build] = button
            buildSelectorPanel.add(button)
        }
    }

    private fun loadCatalogOrFallback(): HiveFeaturesFile =
        runCatching { CMakeUserPresetsGenerator.loadCatalog(project) }
            .getOrElse { error ->
                notifyError(project, "Failed to load HiveFeatures.json", error.message ?: error::class.java.simpleName)
                HiveFeaturesFile()
            }

    private fun createSummaryCard(): JComponent {
        val card = createCardPanel()

        val title = JBLabel("Hive Config")
        title.font = JBFont.h2().asBold()
        card.add(title)

        val subtitle = secondaryLabel("Engine workspace detected. Generate `CMakeUserPresets.json` from `HiveFeatures.json` using base presets from `CMakePresets.json`.")
        subtitle.border = JBUI.Borders.emptyTop(2)
        card.add(subtitle)
        card.add(Box.createVerticalStrut(JBUI.scale(12)))

        val controls = JPanel(GridBagLayout())
        controls.isOpaque = false
        var row = 0
        addFormRow(controls, row++, "Workspace", JBLabel("Engine"))
        addFormRow(controls, row++, "Mode", modeSelectorPanel)
        addFormRow(controls, row++, "Build", buildSelectorPanel)
        workspace.featuresPath?.let { addFormRow(controls, row++, "Features", pathLabel(it)) }

        val detailsPanel = JPanel()
        detailsPanel.layout = BoxLayout(detailsPanel, BoxLayout.Y_AXIS)
        detailsPanel.isOpaque = false
        detailsPanel.border = JBUI.Borders.emptyTop(4)

        toolchainLabel.font = JBFont.label().deriveFont(Font.BOLD)
        detailsPanel.add(toolchainLabel)
        detailsPanel.add(toolchainSourceLabel)
        detailsPanel.add(basePresetLabel)
        detailsPanel.add(capabilityLabel)

        card.add(controls)
        card.add(Box.createVerticalStrut(JBUI.scale(8)))
        card.add(TitledSeparator("Active Toolchain"))
        card.add(Box.createVerticalStrut(JBUI.scale(4)))
        card.add(detailsPanel)

        return card
    }

    private fun createBottomBar(): JComponent {
        val links = JPanel(FlowLayout(FlowLayout.LEFT, JBUI.scale(12), 0))
        links.isOpaque = false
        links.add(ActionLink("Reset current mode") { resetCurrentMode() })
        links.add(ActionLink("Refresh toolchain") {
            toolchain = ToolchainDetection.detect(project)
            CMakeUserPresetsGenerator.normalize(catalog, uiState, toolchain)
            updateHeader()
            reloadMode(selectedMode)
            persistState()
        })
        workspace.cmakePresetsPath?.let { path ->
            links.add(ActionLink("Open CMakePresets.json") { openFile(project, path) })
        }
        workspace.userPresetsPath?.let { path ->
            links.add(ActionLink("Open CMakeUserPresets.json") { openFile(project, path) })
        }

        val applyButton = JButton("Apply")
        applyButton.preferredSize = Dimension(JBUI.scale(84), applyButton.preferredSize.height)
        applyButton.addActionListener { apply() }
        return createBottomBarPanel(links, applyButton)
    }

    private fun rebuildFeatureGroups() {
        groupsPanel.removeAll()
        featureControls.clear()

        catalog.categories.forEachIndexed { index, category ->
            groupsPanel.add(compactWidth(createCategoryPanel(category)))
            if (index != catalog.categories.lastIndex) {
                groupsPanel.add(Box.createVerticalStrut(JBUI.scale(12)))
            }
        }
    }

    private fun createCategoryPanel(category: FeatureCategory): JPanel {
        val card = createCardPanel()

        val title = JBLabel(category.name)
        title.font = JBFont.label().deriveFont(Font.BOLD)
        card.add(title)

        category.description?.takeIf { it.isNotBlank() }?.let { description ->
            val subtitle = secondaryLabel(description)
            subtitle.border = JBUI.Borders.emptyTop(2)
            card.add(subtitle)
        }

        card.add(Box.createVerticalStrut(JBUI.scale(10)))

        category.features.forEachIndexed { featureIndex, feature ->
            val row = createFeatureRow(category, feature)
            row.alignmentX = Component.LEFT_ALIGNMENT
            card.add(row)
            if (featureIndex != category.features.lastIndex) {
                card.add(Box.createVerticalStrut(JBUI.scale(10)))
            }
        }

        return card
    }

    private fun createFeatureRow(category: FeatureCategory, feature: FeatureDefinition): JPanel {
        val descriptionLabel = secondaryLabel(feature.description.orEmpty())
        descriptionLabel.border = JBUI.Borders.emptyLeft(24)

        val noteLabel = secondaryLabel("")
        noteLabel.border = JBUI.Borders.emptyLeft(24)
        noteLabel.foreground = JBColor.namedColor("Component.infoForeground", JBColor.GRAY)

        val editor = when (feature.type) {
            "enum" -> createEnumEditor(feature)
            else -> createBooleanEditor(feature)
        }

        val container = JPanel()
        container.layout = BoxLayout(container, BoxLayout.Y_AXIS)
        container.isOpaque = false
        container.add(editor.component)
        if (feature.description != null) {
            container.add(Box.createVerticalStrut(JBUI.scale(2)))
            container.add(descriptionLabel)
        }
        container.add(Box.createVerticalStrut(JBUI.scale(2)))
        container.add(noteLabel)
        container.toolTipText = feature.description ?: category.description

        featureControls[feature.id] = FeatureControl(feature, editor, descriptionLabel, noteLabel)
        return container
    }

    private fun createBooleanEditor(feature: FeatureDefinition): FeatureEditor.BooleanEditor {
        val checkBox = JBCheckBox(feature.name)
        checkBox.font = JBFont.label().deriveFont(Font.PLAIN)
        checkBox.toolTipText = feature.description
        checkBox.addItemListener {
            if (!updatingUi) {
                currentModeConfig().features[feature.id] = checkBox.isSelected.toString()
                CMakeUserPresetsGenerator.normalize(catalog, uiState, toolchain)
                reloadMode(selectedMode)
                persistState()
            }
        }
        return FeatureEditor.BooleanEditor(checkBox)
    }

    private fun createEnumEditor(feature: FeatureDefinition): FeatureEditor.EnumEditor {
        val combo = JComboBox<FeatureOptionLabel>()
        combo.renderer = FeatureOptionRenderer()
        combo.toolTipText = feature.description
        combo.preferredSize = Dimension(JBUI.scale(180), combo.preferredSize.height)
        combo.addItemListener {
            if (!updatingUi && it.stateChange == ItemEvent.SELECTED) {
                val value = (combo.selectedItem as? FeatureOptionLabel)?.value ?: return@addItemListener
                currentModeConfig().features[feature.id] = value
                CMakeUserPresetsGenerator.normalize(catalog, uiState, toolchain)
                reloadMode(selectedMode)
                persistState()
            }
        }

        val panel = JPanel(BorderLayout(JBUI.scale(12), 0))
        panel.isOpaque = false
        panel.add(JBLabel(feature.name), BorderLayout.WEST)
        panel.add(combo, BorderLayout.CENTER)
        return FeatureEditor.EnumEditor(panel, combo)
    }

    private fun reloadMode(mode: String) {
        if (mode.isBlank()) {
            return
        }

        updatingUi = true
        try {
            val config = uiState.modes.getValue(mode)
            modeButtons[mode]?.isSelected = true
            buildButtons[config.buildConfig]?.isSelected = true

            featureControls.forEach { (featureId, control) ->
                val availability = CMakeUserPresetsGenerator.computeAvailability(mode, control.feature, config.features, toolchain)
                when (val editor = control.editor) {
                    is FeatureEditor.BooleanEditor -> {
                        editor.checkBox.isSelected = config.features[featureId] == "true"
                        editor.checkBox.isEnabled = availability.enabled
                    }

                    is FeatureEditor.EnumEditor -> {
                        val items = control.feature.options
                            .filter { availability.allowedOptionValues == null || it.value in availability.allowedOptionValues }
                            .map { FeatureOptionLabel(it.value, it.name) }
                            .toTypedArray()
                        editor.combo.model = DefaultComboBoxModel(items)
                        editor.combo.selectedItem = items.firstOrNull { it.value == config.features[featureId] }
                            ?: items.firstOrNull()
                        editor.combo.isEnabled = availability.enabled
                    }
                }

                control.descriptionLabel.isVisible = !control.feature.description.isNullOrBlank()
                val note = availability.reason?.takeIf { it != control.feature.description }.orEmpty()
                control.noteLabel.text = note
                control.noteLabel.isVisible = note.isNotBlank()
            }
        } finally {
            updatingUi = false
        }

        groupsPanel.revalidate()
        groupsPanel.repaint()
    }

    private fun resetCurrentMode() {
        val mode = selectedMode
        val config = uiState.modes.getValue(mode)
        config.buildConfig = catalog.presetDefaults[mode]?.buildType ?: catalog.buildConfigs.firstOrNull() ?: "Debug"
        catalog.categories.flatMap { it.features }.forEach { feature ->
            config.features[feature.id] = feature.defaultValueFor(mode)
        }
        CMakeUserPresetsGenerator.normalize(catalog, uiState, toolchain)
        reloadMode(mode)
        persistState()
    }

    private fun currentModeConfig(): ModeConfiguration =
        uiState.modes.getValue(selectedMode)

    private fun updateHeader() {
        toolchainLabel.text = toolchain.summary()
        toolchainSourceLabel.text = "${toolchain.toolchainName} (${toolchain.detectionSource})"
        basePresetLabel.text = "Base preset: ${suggestedBasePreset()}"
        capabilityLabel.text = buildString {
            append("D3D12: ")
            append(if (toolchain.supportsD3D12) "supported" else "Windows only")
            append("  |  Sanitizers: ")
            append(if (toolchain.supportsSanitizers) "supported" else "Linux with LLVM/GCC")
            append("  |  MSAN: ")
            append(if (toolchain.supportsMemorySanitizer) "supported" else "LLVM Linux only")
        }

        toolchainSourceLabel.foreground = JBColor.GRAY
        basePresetLabel.foreground = JBColor.GRAY
        capabilityLabel.foreground = JBColor.GRAY
    }

    private fun suggestedBasePreset(): String =
        when (toolchain.platform) {
            ToolchainPlatform.WINDOWS -> when (toolchain.compilerFamily) {
                CompilerFamily.LLVM -> "llvm-windows-base"
                CompilerFamily.MSVC -> "msvc-windows-base"
                else -> "unsupported"
            }

            ToolchainPlatform.LINUX -> when (toolchain.compilerFamily) {
                CompilerFamily.LLVM -> "llvm-linux-base"
                CompilerFamily.GCC -> "gcc-linux-base"
                else -> "unsupported"
            }

            else -> "unsupported"
        }

    private fun persistState() {
        settingsService.saveUiState(uiState)
    }

    private fun apply() {
        runCatching {
            persistState()
            val path = CMakeUserPresetsGenerator.generate(project, catalog, uiState, toolchain)
            CMakeUserPresetsGenerator.notifySuccess(project, path)
        }.onFailure {
            CMakeUserPresetsGenerator.notifyFailure(project, it)
        }
    }
}
