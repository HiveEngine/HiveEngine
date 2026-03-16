package dev.hive.clion.config

import com.intellij.notification.NotificationGroupManager
import com.intellij.notification.NotificationType
import com.intellij.openapi.fileEditor.OpenFileDescriptor
import com.intellij.openapi.project.Project
import com.intellij.openapi.vfs.LocalFileSystem
import com.intellij.ui.ColorUtil
import com.intellij.ui.JBColor
import com.intellij.ui.ScrollPaneFactory
import com.intellij.ui.components.JBLabel
import com.intellij.util.ui.JBFont
import com.intellij.util.ui.JBUI
import java.awt.BorderLayout
import java.awt.Component
import java.awt.Dimension
import java.awt.GridBagConstraints
import java.awt.Insets
import java.nio.file.Path
import javax.swing.BorderFactory
import javax.swing.BoxLayout
import javax.swing.JComponent
import javax.swing.JPanel
import kotlin.io.path.invariantSeparatorsPathString

fun buildScrollableContent(content: JPanel, bottomBar: JComponent?): JComponent {
    val scrollPane = ScrollPaneFactory.createScrollPane(content, true)
    scrollPane.border = JBUI.Borders.empty()
    scrollPane.viewport.isOpaque = false
    scrollPane.isOpaque = false

    return JPanel(BorderLayout()).apply {
        add(scrollPane, BorderLayout.CENTER)
        bottomBar?.let { add(compactWidth(it), BorderLayout.SOUTH) }
    }
}

fun createBottomBarPanel(links: JPanel, trailing: JComponent?): JComponent =
    JPanel(BorderLayout()).apply {
        border = JBUI.Borders.empty(8, 10, 10, 10)
        add(links, BorderLayout.WEST)
        trailing?.let { add(it, BorderLayout.EAST) }
    }

fun createCardPanel(): JPanel =
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

fun <T : JComponent> compactWidth(component: T): T =
    component.apply {
        alignmentX = Component.LEFT_ALIGNMENT
        maximumSize = Dimension(JBUI.scale(MAX_CONTENT_WIDTH), Int.MAX_VALUE)
    }

fun secondaryLabel(text: String): JBLabel =
    JBLabel(text).apply {
        foreground = JBColor.GRAY
        font = JBFont.smallOrNewUiMedium()
    }

fun pathLabel(path: Path): JBLabel =
    secondaryLabel(path.invariantSeparatorsPathString)

fun addFormRow(panel: JPanel, row: Int, label: String, component: JComponent) {
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

fun openFile(project: Project, path: Path) {
    val virtualFile = LocalFileSystem.getInstance().refreshAndFindFileByPath(path.invariantSeparatorsPathString) ?: return
    OpenFileDescriptor(project, virtualFile).navigate(true)
}

fun notifyError(project: Project, title: String, content: String) {
    NotificationGroupManager.getInstance()
        .getNotificationGroup("Hive Config")
        .createNotification(title, content, NotificationType.ERROR)
        .notify(project)
}

private fun cardBackground() =
    if (isDarkTheme()) {
        ColorUtil.shift(JBUI.CurrentTheme.ToolWindow.background(), 1.08)
    } else {
        ColorUtil.shift(JBUI.CurrentTheme.ToolWindow.background(), 0.985)
    }

private fun cardBorderColor() =
    if (isDarkTheme()) {
        ColorUtil.shift(cardBackground(), 1.18)
    } else {
        ColorUtil.shift(cardBackground(), 0.92)
    }

private fun isDarkTheme(): Boolean =
    !JBColor.isBright()

private const val MAX_CONTENT_WIDTH = 430
