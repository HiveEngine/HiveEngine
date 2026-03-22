#pragma once

namespace waggle
{
    // User-facing tag: marks an entity as explicitly disabled.
    struct Disabled
    {
    };

    // Propagated cache: present when the entity itself or any ancestor has Disabled.
    // Maintained automatically by observers — do not add/remove manually.
    struct HierarchyDisabled
    {
    };
} // namespace waggle
