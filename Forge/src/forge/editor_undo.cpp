#include <forge/editor_undo.h>

namespace forge
{
    void EditorUndoManager::Push(UndoFn undo, UndoFn redo)
    {
        m_redoStack.clear();
        m_undoStack.push_back({std::move(undo), std::move(redo)});
    }

    bool EditorUndoManager::Undo()
    {
        if (m_undoStack.empty())
            return false;

        auto cmd = std::move(m_undoStack.back());
        m_undoStack.pop_back();
        cmd.m_undo();
        m_redoStack.push_back(std::move(cmd));
        return true;
    }

    bool EditorUndoManager::Redo()
    {
        if (m_redoStack.empty())
            return false;

        auto cmd = std::move(m_redoStack.back());
        m_redoStack.pop_back();
        cmd.m_redo();
        m_undoStack.push_back(std::move(cmd));
        return true;
    }

    void EditorUndoManager::Clear() noexcept
    {
        m_undoStack.clear();
        m_redoStack.clear();
    }
} // namespace forge
