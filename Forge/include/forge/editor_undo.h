#pragma once

#include <functional>
#include <string>
#include <vector>

namespace forge
{
    class EditorUndoManager
    {
    public:
        using UndoFn = std::function<void()>;

        void Push(UndoFn undo, UndoFn redo);
        [[nodiscard]] bool Undo();
        [[nodiscard]] bool Redo();

        [[nodiscard]] bool CanUndo() const noexcept { return !m_undoStack.empty(); }
        [[nodiscard]] bool CanRedo() const noexcept { return !m_redoStack.empty(); }

        void Clear() noexcept;

    private:
        struct Command
        {
            UndoFn m_undo;
            UndoFn m_redo;
        };

        std::vector<Command> m_undoStack;
        std::vector<Command> m_redoStack;
    };
} // namespace forge
