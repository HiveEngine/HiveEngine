#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>

namespace nectar
{
    /// Normalize a VFS path:
    /// - Backslashes -> forward slashes
    /// - Lowercase
    /// - Resolve . and ..
    /// - No double slashes
    /// - No trailing slash
    /// - No leading slash (paths are relative to mount points)
    [[nodiscard]] HIVE_API wax::String NormalizePath(wax::StringView path, comb::DefaultAllocator& alloc);

    /// Extract parent directory: "textures/hero.png" -> "textures"
    /// Returns empty view if no parent.
    [[nodiscard]] HIVE_API wax::StringView PathParent(wax::StringView path);

    /// Extract filename: "textures/hero.png" -> "hero.png"
    [[nodiscard]] HIVE_API wax::StringView PathFilename(wax::StringView path);

    /// Extract extension: "hero.png" -> ".png"
    /// Returns empty view if no extension.
    [[nodiscard]] HIVE_API wax::StringView PathExtension(wax::StringView path);
} // namespace nectar
