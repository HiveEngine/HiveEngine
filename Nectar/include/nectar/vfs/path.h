#pragma once

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Normalize a VFS path:
    /// - Backslashes -> forward slashes
    /// - Lowercase
    /// - Resolve . and ..
    /// - No double slashes
    /// - No trailing slash
    /// - No leading slash (paths are relative to mount points)
    [[nodiscard]] wax::String<> NormalizePath(wax::StringView path, comb::DefaultAllocator& alloc);

    /// Extract parent directory: "textures/hero.png" -> "textures"
    /// Returns empty view if no parent.
    [[nodiscard]] wax::StringView PathParent(wax::StringView path);

    /// Extract filename: "textures/hero.png" -> "hero.png"
    [[nodiscard]] wax::StringView PathFilename(wax::StringView path);

    /// Extract extension: "hero.png" -> ".png"
    /// Returns empty view if no extension.
    [[nodiscard]] wax::StringView PathExtension(wax::StringView path);
}
