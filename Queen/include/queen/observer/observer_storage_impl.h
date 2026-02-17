#pragma once

#include <queen/observer/observer_storage.h>
#include <queen/world/world.h>

namespace queen
{
    template<comb::Allocator Allocator>
    void ObserverStorage<Allocator>::Trigger(TriggerType trigger, TypeId component_id,
                                            World& world, Entity entity, const void* component)
    {
        ObserverKey key{trigger, component_id};

        auto* indices = lookup_.Find(key);
        if (indices == nullptr)
        {
            return;
        }

        for (size_t i = 0; i < indices->Size(); ++i)
        {
            uint32_t idx = (*indices)[i];
            if (idx < observers_.Size())
            {
                auto& observer = observers_[idx];

                // Check filter components before invoking
                if (observer.HasFilters())
                {
                    bool matches = true;
                    for (uint8_t f = 0; f < observer.FilterCount(); ++f)
                    {
                        if (!world.HasComponent(entity, observer.FilterId(f)))
                        {
                            matches = false;
                            break;
                        }
                    }
                    if (!matches) continue;
                }

                observer.Invoke(world, entity, component);
            }
        }
    }
}
