package dev.hive.clion.config

import com.intellij.ui.components.JBCheckBox
import com.intellij.ui.components.JBLabel
import com.intellij.util.ui.JBFont
import java.awt.Component
import java.awt.Font
import java.awt.event.ItemEvent
import javax.swing.JComboBox
import javax.swing.JComponent
import javax.swing.JList
import javax.swing.JRadioButton
import javax.swing.DefaultListCellRenderer

fun createSelectorRadio(label: String, onSelect: () -> Unit): JRadioButton =
    JRadioButton(label).apply {
        isOpaque = false
        font = JBFont.label().deriveFont(Font.PLAIN)
        addItemListener {
            if (it.stateChange == ItemEvent.SELECTED) {
                onSelect()
            }
        }
    }

data class FeatureControl(
    val feature: dev.hive.clion.config.model.FeatureDefinition,
    val editor: FeatureEditor,
    val descriptionLabel: JBLabel,
    val noteLabel: JBLabel,
)

sealed class FeatureEditor(open val component: JComponent) {
    data class BooleanEditor(val checkBox: JBCheckBox) : FeatureEditor(checkBox)
    data class EnumEditor(override val component: JComponent, val combo: JComboBox<FeatureOptionLabel>) :
        FeatureEditor(component)
}

data class FeatureOptionLabel(
    val value: String,
    val label: String,
) {
    override fun toString(): String = label
}

class FeatureOptionRenderer : DefaultListCellRenderer() {
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
