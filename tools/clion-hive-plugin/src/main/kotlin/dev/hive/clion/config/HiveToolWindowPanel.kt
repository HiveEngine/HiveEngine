package dev.hive.clion.config

import com.intellij.notification.NotificationGroupManager
import com.intellij.notification.NotificationType
import com.intellij.openapi.project.Project
import com.intellij.ui.ColorUtil
import com.intellij.ui.JBColor
import com.intellij.ui.ScrollPaneFactory
import com.intellij.ui.TitledSeparator
import com.intellij.ui.components.ActionLink
import com.intellij.ui.components.JBCheckBox
import com.intellij.ui.components.JBLabel
import com.intellij.util.ui.JBFont
import com.intellij.util.ui.JBUI
import com.intellij.util.ui.StartupUiUtil
import dev.hive.clion.config.model.CompilerFamily
import dev.hive.clion.config.model.FeatureCategory
import dev.hive.clion.config.model.FeatureDefinition
import dev.hive.clion.config.model.HiveFeaturesFile
import dev.hive.clion.config.model.HiveUiState
import dev.hive.clion.config.model.ToolchainPlatform
import dev.hive.clion.config.presets.CMakeUserPresetsGenerator
import dev.hive.clion.config.state.HiveProjectSettingsService
import dev.hive.clion.config.toolchain.ToolchainDetection
import java.awt.BorderLayout
import java.awt.Component
import java.awt.Dimension
import java.awt.FlowLayout
import java.awt.Font
import java.awt.GridBagConstraints
import java.awt.GridBagLayout
import java.awt.Insets
import java.awt.event.ItemEvent
import javax.swing.BorderFactory
import javax.swing.Box
import javax.swing.BoxLayout
import javax.swing.DefaultComboBoxModel
import javax.swing.DefaultListCellRenderer
import javax.swing.ButtonGroup
import javax.swing.JButton
import javax.swing.JComboBox
import javax.swing.JComponent
import javax.swing.JList
import javax.swing.JPanel
import javax.swing.JRadioButton

class HiveToolWindowPanel(private val project: Project) {
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
    private val modeButtons = linkedMapOf<String, JRadioButton>()
    private val buildButtons = linkedMapOf<String, JRadioButton>()

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

        val scrollPane = ScrollPaneFactory.createScrollPane(content, true)
        scrollPane.border = JBUI.Borders.empty()
        scrollPane.viewport.isOpaque = false
        scrollPane.isOpaque = false

        val root = JPanel(BorderLayout())
        root.add(scrollPane, BorderLayout.CENTER)
        root.add(compactWidth(createBottomBar()), BorderLayout.SOUTH)
        component = root

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
                notifyError("Failed to load HiveFeatures.json", error.message ?: error::class.java.simpleName)
                HiveFeaturesFile()
            }

    private fun createSummaryCard(): JComponent {
        val card = createCardPanel()

        val title = JBLabel("Hive Config")
        title.font = JBFont.h2().asBold()
        card.add(title)

        val subtitle = secondaryLabel("Generate `CMakeUserPresets.json` from `HiveFeatures.json`.")
        subtitle.border = JBUI.Borders.emptyTop(2)
        card.add(subtitle)
        card.add(Box.createVerticalStrut(JBUI.scale(12)))

        val controls = JPanel(GridBagLayout())
        controls.isOpaque = false
        var row = 0
        addFormRow(controls, row++, "Mode", modeSelectorPanel)
        addFormRow(controls, row++, "Build", buildSelectorPanel)

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
        val panel = JPanel(BorderLayout())
        panel.border = JBUI.Borders.empty(8, 10, 10, 10)

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

        val applyButton = JButton("Apply")
        applyButton.preferredSize = Dimension(JBUI.scale(84), applyButton.preferredSize.height)
        applyButton.addActionListener { apply() }

        panel.add(links, BorderLayout.WEST)
        panel.add(applyButton, BorderLayout.EAST)
        return panel
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
                        editor.combo.selectedItem = items.firstOrNull { it.value == config.features[featureId] } ?: items.firstOrNull()
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

    private fun currentModeConfig() = uiState.modes.getValue(selectedMode)

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

    private fun notifyError(title: String, content: String) {
        NotificationGroupManager.getInstance()
            .getNotificationGroup("Hive Config")
            .createNotification(title, content, NotificationType.ERROR)
            .notify(project)
    }

    private fun createCardPanel(): JPanel =
        JPanel().apply {
            layout = BoxLayout(this, BoxLayout.Y_AXIS)
            isOpaque = true
            background = cardBackground()
            alignmentX = Component.LEFT_ALIGNMENT
            border = BorderFactory.createCompoundBorder(
                JBUI.Borders.customLine(cardBorderColor(), 1),
                JBUI.Borders.empty(10),
            )
        }

    private fun <T : JComponent> compactWidth(component: T): T =
        component.apply {
            alignmentX = Component.LEFT_ALIGNMENT
            maximumSize = Dimension(JBUI.scale(MAX_CONTENT_WIDTH), Int.MAX_VALUE)
        }

    private fun createSelectorRadio(label: String, onSelect: () -> Unit): JRadioButton =
        JRadioButton(label).apply {
            isOpaque = false
            font = JBFont.label().deriveFont(Font.PLAIN)
            addItemListener {
                if (it.stateChange == ItemEvent.SELECTED) {
                    onSelect()
                }
            }
        }

    private fun secondaryLabel(text: String): JBLabel =
        JBLabel(text).apply {
            foreground = JBColor.GRAY
            font = JBFont.smallOrNewUiMedium()
        }

    private fun addFormRow(panel: JPanel, row: Int, label: String, component: JComponent) {
        val labelConstraints = GridBagConstraints().apply {
            gridx = 0
            gridy = row
            anchor = GridBagConstraints.WEST
            insets = Insets(0, 0, JBUI.scale(8), JBUI.scale(12))
        }
        val fieldConstraints = GridBagConstraints().apply {
            gridx = 1
            gridy = row
            weightx = 1.0
            fill = GridBagConstraints.HORIZONTAL
            insets = Insets(0, 0, JBUI.scale(8), 0)
        }
        panel.add(JBLabel(label), labelConstraints)
        panel.add(component, fieldConstraints)
    }

    private fun cardBackground() =
        if (StartupUiUtil.isUnderDarcula) {
            ColorUtil.shift(JBUI.CurrentTheme.ToolWindow.background(), 1.08)
        } else {
            ColorUtil.shift(JBUI.CurrentTheme.ToolWindow.background(), 0.985)
        }

    private fun cardBorderColor() =
        if (StartupUiUtil.isUnderDarcula) {
            ColorUtil.shift(cardBackground(), 1.18)
        } else {
            ColorUtil.shift(cardBackground(), 0.92)
        }

    companion object {
        private const val MAX_CONTENT_WIDTH = 430
    }
}

private data class FeatureControl(
    val feature: FeatureDefinition,
    val editor: FeatureEditor,
    val descriptionLabel: JBLabel,
    val noteLabel: JBLabel,
)

private sealed class FeatureEditor(open val component: JComponent) {
    data class BooleanEditor(val checkBox: JBCheckBox) : FeatureEditor(checkBox)
    data class EnumEditor(override val component: JComponent, val combo: JComboBox<FeatureOptionLabel>) : FeatureEditor(component)
}

private data class FeatureOptionLabel(
    val value: String,
    val label: String,
) {
    override fun toString(): String = label
}

private class FeatureOptionRenderer : DefaultListCellRenderer() {
    override fun getListCellRendererComponent(
        list: JList<*>?,
        value: Any?,
        index: Int,
        isSelected: Boolean,
        cellHasFocus: Boolean,
    ): Component {
        val component = super.getListCellRendererComponent(list, value, index, isSelected, cellHasFocus)
        text = (value as? FeatureOptionLabel)?.label ?: value?.toString().orEmpty()
        return component
    }
}
