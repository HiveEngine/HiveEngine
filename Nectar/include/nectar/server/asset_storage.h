#pragma once

#include <hive/core/assert.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#include <wax/containers/vector.h>
#include <wax/pointers/handle.h>
#include <wax/serialization/byte_span.h>

#include <nectar/core/asset_status.h>
#include <nectar/core/type_id.h>
#include <nectar/server/asset_event.h>
#include <nectar/server/asset_loader.h>

#include <cstddef>
#include <cstdint>

namespace nectar
{
    /// Type-erased interface for per-type asset storage.
    /// Only AssetServer should use this directly.
    class IAssetStorage
    {
    public:
        virtual ~IAssetStorage() = default;

        virtual nectar::TypeId GetTypeId() const noexcept = 0;
        virtual void IncrementRef(uint32_t index) noexcept = 0;
        virtual void DecrementRef(uint32_t index) noexcept = 0;
        virtual uint32_t GetRefCount(uint32_t index) const noexcept = 0;
        virtual AssetStatus GetStatus(uint32_t index) const noexcept = 0;
        virtual void SetStatus(uint32_t index, AssetStatus status) noexcept = 0;
        virtual const AssetErrorInfo* GetError(uint32_t index) const noexcept = 0;
        virtual void SetError(uint32_t index, AssetErrorInfo error) noexcept = 0;
        virtual bool IsHandleValid(uint32_t index, uint32_t generation) const noexcept = 0;

        /// Unload and release a slot (called by GC). Calls loader->Unload if asset present.
        virtual void UnloadSlot(uint32_t index, uint32_t generation) noexcept = 0;

        /// Collect unreferenced assets. gc_grace_frames = countdown before unloading.
        virtual size_t CollectGarbage(uint32_t gcGraceFrames) noexcept = 0;

        /// Load asset from raw data into an existing slot. Returns true on success.
        virtual bool LoadFromData(uint32_t index, uint32_t generation, wax::ByteSpan data,
                                  comb::DefaultAllocator& alloc) noexcept = 0;

        virtual size_t Count() const noexcept = 0;
        virtual size_t Capacity() const noexcept = 0;

        // -- Events --

        /// Drain queued events into a type-erased buffer. Returns number drained.
        virtual size_t DrainEvents(void* outBuffer, size_t maxCount) noexcept = 0;

        // -- GC / Budget --

        virtual void SetPersistent(uint32_t index, bool persistent) noexcept = 0;
        virtual size_t BytesUsed() const noexcept = 0;
        virtual void SetBudget(size_t bytes) noexcept = 0;

        virtual void Destroy(comb::DefaultAllocator& alloc) noexcept = 0;
    };

    /// Concrete typed storage for assets of type T.
    /// Manages slots with generation counters, ref counts, status, and loaded asset pointers.
    template <typename T> class AssetStorageFor final : public IAssetStorage
    {
    public:
        AssetStorageFor(comb::DefaultAllocator& alloc, size_t capacity)
            : m_allocator{&alloc}
            , m_capacity{capacity}
            , m_count{0}
            , m_firstFree{0}
            , m_eventQueue{alloc}
        {
            hive::Assert(capacity > 0, "AssetStorageFor capacity must be > 0");
            hive::Assert(capacity <= UINT32_MAX, "AssetStorageFor capacity exceeds max");

            m_slots = static_cast<Slot*>(alloc.Allocate(sizeof(Slot) * capacity, alignof(Slot)));
            hive::Check(m_slots != nullptr, "Failed to allocate AssetStorageFor slots");

            for (size_t i = 0; i < m_capacity - 1; ++i)
            {
                new (&m_slots[i]) Slot{};
                m_slots[i].m_nextFree = static_cast<uint32_t>(i + 1);
            }
            new (&m_slots[m_capacity - 1]) Slot{};
            m_slots[m_capacity - 1].m_nextFree = UINT32_MAX;
        }

        ~AssetStorageFor() override
        {
            for (size_t i = 0; i < m_capacity; ++i)
            {
                if (m_slots[i].m_alive && m_slots[i].m_asset && m_loader)
                {
                    m_loader->Unload(m_slots[i].m_asset, *m_allocator);
                }
                m_slots[i].~Slot();
            }

            if (m_slots && m_allocator)
            {
                m_allocator->Deallocate(m_slots);
            }
        }

        AssetStorageFor(const AssetStorageFor&) = delete;
        AssetStorageFor& operator=(const AssetStorageFor&) = delete;

        nectar::TypeId GetTypeId() const noexcept override
        {
            return nectar::TypeIdOf<T>();
        }

        void SetLoader(AssetLoader<T>* loader) noexcept
        {
            m_loader = loader;
        }
        [[nodiscard]] AssetLoader<T>* GetLoader() const noexcept
        {
            return m_loader;
        }

        void SetPlaceholder(T* placeholder) noexcept
        {
            m_placeholder = placeholder;
        }
        [[nodiscard]] T* GetPlaceholder() const noexcept
        {
            return m_placeholder;
        }

        /// Allocate a new slot. Returns Handle::Invalid() if full.
        [[nodiscard]] wax::Handle<T> AllocateSlot() noexcept
        {
            if (m_firstFree == UINT32_MAX)
            {
                return wax::Handle<T>::Invalid();
            }

            uint32_t index = m_firstFree;
            Slot& slot = m_slots[index];
            m_firstFree = slot.m_nextFree;

            slot.m_alive = true;
            slot.m_refCount = 0;
            slot.m_status = AssetStatus::NOT_LOADED;
            slot.m_error = AssetErrorInfo{};
            slot.m_asset = nullptr;
            ++m_count;

            return wax::Handle<T>{index, slot.m_generation};
        }

        void SetAsset(wax::Handle<T> handle, T* asset) noexcept
        {
            hive::Assert(!handle.IsNull() && handle.m_index < m_capacity, "Invalid handle in SetAsset");
            Slot& slot = m_slots[handle.m_index];
            hive::Assert(slot.m_alive && slot.m_generation == handle.m_generation, "Stale handle in SetAsset");
            slot.m_asset = asset;
            if (asset && m_loader)
                m_bytesUsed += m_loader->SizeOf(asset);
        }

        [[nodiscard]] T* GetAsset(wax::Handle<T> handle) const noexcept
        {
            if (handle.IsNull() || handle.m_index >= m_capacity)
                return nullptr;
            const Slot& slot = m_slots[handle.m_index];
            if (!slot.m_alive || slot.m_generation != handle.m_generation)
                return nullptr;
            return slot.m_asset;
        }

        /// Returns the loaded asset if ready, or the placeholder otherwise.
        [[nodiscard]] T* GetAssetOrPlaceholder(wax::Handle<T> handle) const noexcept
        {
            if (handle.IsNull() || handle.m_index >= m_capacity)
                return m_placeholder;
            const Slot& slot = m_slots[handle.m_index];
            if (!slot.m_alive || slot.m_generation != handle.m_generation)
                return m_placeholder;
            if (slot.m_status == AssetStatus::READY && slot.m_asset)
                return slot.m_asset;
            return m_placeholder;
        }

        // -- IAssetStorage interface --

        void IncrementRef(uint32_t index) noexcept override
        {
            hive::Assert(index < m_capacity && m_slots[index].m_alive, "IncrementRef on dead slot");
            ++m_slots[index].m_refCount;
        }

        void DecrementRef(uint32_t index) noexcept override
        {
            hive::Assert(index < m_capacity && m_slots[index].m_alive, "DecrementRef on dead slot");
            hive::Assert(m_slots[index].m_refCount > 0, "DecrementRef below zero");
            --m_slots[index].m_refCount;
        }

        uint32_t GetRefCount(uint32_t index) const noexcept override
        {
            if (index >= m_capacity || !m_slots[index].m_alive)
                return 0;
            return m_slots[index].m_refCount;
        }

        AssetStatus GetStatus(uint32_t index) const noexcept override
        {
            if (index >= m_capacity || !m_slots[index].m_alive)
                return AssetStatus::NOT_LOADED;
            return m_slots[index].m_status;
        }

        void SetStatus(uint32_t index, AssetStatus status) noexcept override
        {
            hive::Assert(index < m_capacity && m_slots[index].m_alive, "SetStatus on dead slot");
            auto old = m_slots[index].m_status;
            m_slots[index].m_status = status;

            if (status == AssetStatus::READY && old != AssetStatus::READY)
                EmitEvent(AssetEventKind::LOADED, index, m_slots[index].m_generation);
            else if (status == AssetStatus::Failed && old != AssetStatus::Failed)
                EmitEvent(AssetEventKind::Failed, index, m_slots[index].m_generation);
        }

        const AssetErrorInfo* GetError(uint32_t index) const noexcept override
        {
            if (index >= m_capacity || !m_slots[index].m_alive)
                return nullptr;
            return &m_slots[index].m_error;
        }

        void SetError(uint32_t index, AssetErrorInfo error) noexcept override
        {
            hive::Assert(index < m_capacity && m_slots[index].m_alive, "SetError on dead slot");
            m_slots[index].m_error = static_cast<AssetErrorInfo&&>(error);
        }

        bool IsHandleValid(uint32_t index, uint32_t generation) const noexcept override
        {
            if (index >= m_capacity)
                return false;
            return m_slots[index].m_alive && m_slots[index].m_generation == generation;
        }

        void UnloadSlot(uint32_t index, uint32_t generation) noexcept override
        {
            if (index >= m_capacity)
                return;
            Slot& slot = m_slots[index];
            if (!slot.m_alive || slot.m_generation != generation)
                return;

            EmitEvent(AssetEventKind::UNLOADED, index, generation);

            if (slot.m_asset && m_loader)
            {
                size_t sz = m_loader->SizeOf(slot.m_asset);
                if (sz <= m_bytesUsed)
                    m_bytesUsed -= sz;
                m_loader->Unload(slot.m_asset, *m_allocator);
            }

            slot.m_asset = nullptr;
            slot.m_alive = false;
            slot.m_status = AssetStatus::UNLOADED;
            slot.m_error = AssetErrorInfo{};
            slot.m_refCount = 0;
            slot.m_gcCountdown = 0;
            slot.m_persistent = false;
            ++slot.m_generation;
            slot.m_nextFree = m_firstFree;
            m_firstFree = index;
            --m_count;
        }

        size_t CollectGarbage(uint32_t gcGraceFrames) noexcept override
        {
            size_t collected = 0;
            bool overBudget = (m_budget > 0 && m_bytesUsed > m_budget);

            for (size_t i = 0; i < m_capacity; ++i)
            {
                Slot& slot = m_slots[i];
                if (!slot.m_alive)
                    continue;

                // Reset countdown if asset got re-referenced
                if (slot.m_refCount > 0 && slot.m_gcCountdown > 0)
                {
                    slot.m_gcCountdown = 0;
                    continue;
                }

                if (slot.m_refCount != 0 || slot.m_status != AssetStatus::READY)
                    continue;
                if (slot.m_persistent)
                    continue;

                // Over budget → immediate unload (no grace period)
                if (overBudget)
                {
                    UnloadSlot(static_cast<uint32_t>(i), slot.m_generation);
                    ++collected;
                    overBudget = (m_budget > 0 && m_bytesUsed > m_budget);
                    continue;
                }

                // Grace period logic
                if (gcGraceFrames == 0)
                {
                    UnloadSlot(static_cast<uint32_t>(i), slot.m_generation);
                    ++collected;
                    continue;
                }

                if (slot.m_gcCountdown == 0)
                {
                    slot.m_gcCountdown = gcGraceFrames;
                }
                else
                {
                    --slot.m_gcCountdown;
                    if (slot.m_gcCountdown == 0)
                    {
                        UnloadSlot(static_cast<uint32_t>(i), slot.m_generation);
                        ++collected;
                    }
                }
            }
            return collected;
        }

        bool LoadFromData(uint32_t index, uint32_t generation, wax::ByteSpan data,
                          comb::DefaultAllocator& alloc) noexcept override
        {
            if (index >= m_capacity || !m_slots[index].m_alive || m_slots[index].m_generation != generation)
                return false;
            if (!m_loader)
                return false;

            T* asset = m_loader->Load(data, alloc);
            if (!asset)
                return false;

            m_slots[index].m_asset = asset;
            m_bytesUsed += m_loader->SizeOf(asset);
            return true;
        }

        size_t Count() const noexcept override
        {
            return m_count;
        }
        size_t Capacity() const noexcept override
        {
            return m_capacity;
        }

        // -- Events --

        size_t DrainEvents(void* outBuffer, size_t maxCount) noexcept override
        {
            size_t count = m_eventQueue.Size() < maxCount ? m_eventQueue.Size() : maxCount;
            auto* dst = static_cast<AssetEvent<T>*>(outBuffer);
            for (size_t i = 0; i < count; ++i)
                dst[i] = m_eventQueue[i];

            // Remove drained events (shift remainder)
            if (count == m_eventQueue.Size())
            {
                m_eventQueue.Clear();
            }
            else
            {
                wax::Vector<AssetEvent<T>> remaining{*m_allocator};
                for (size_t i = count; i < m_eventQueue.Size(); ++i)
                    remaining.PushBack(m_eventQueue[i]);
                m_eventQueue = static_cast<wax::Vector<AssetEvent<T>>&&>(remaining);
            }

            return count;
        }

        // -- GC / Budget --

        void SetPersistent(uint32_t index, bool persistent) noexcept override
        {
            if (index < m_capacity && m_slots[index].m_alive)
                m_slots[index].m_persistent = persistent;
        }

        size_t BytesUsed() const noexcept override
        {
            return m_bytesUsed;
        }

        void SetBudget(size_t bytes) noexcept override
        {
            m_budget = bytes;
        }

        void Destroy(comb::DefaultAllocator& alloc) noexcept override
        {
            comb::Delete(alloc, this);
        }

        /// Reload an asset in-place. Swaps old → new, emits Reloaded event.
        bool ReloadAsset(wax::Handle<T> handle, wax::ByteSpan data)
        {
            if (handle.IsNull() || handle.m_index >= m_capacity)
                return false;
            Slot& slot = m_slots[handle.m_index];
            if (!slot.m_alive || slot.m_generation != handle.m_generation)
                return false;
            if (!m_loader)
                return false;

            T* newAsset = m_loader->Load(data, *m_allocator);
            if (!newAsset)
                return false;

            // Unload old
            if (slot.m_asset)
            {
                size_t oldSz = m_loader->SizeOf(slot.m_asset);
                if (oldSz <= m_bytesUsed)
                    m_bytesUsed -= oldSz;
                m_loader->Unload(slot.m_asset, *m_allocator);
            }

            slot.m_asset = newAsset;
            slot.m_status = AssetStatus::READY;
            m_bytesUsed += m_loader->SizeOf(newAsset);
            EmitEvent(AssetEventKind::RELOADED, handle.m_index, handle.m_generation);
            return true;
        }

    private:
        struct Slot
        {
            uint32_t m_generation{0};
            uint32_t m_nextFree{UINT32_MAX};
            uint32_t m_refCount{0};
            uint32_t m_gcCountdown{0};
            bool m_alive{false};
            bool m_persistent{false};
            AssetStatus m_status{AssetStatus::NOT_LOADED};
            AssetErrorInfo m_error{};
            T* m_asset{nullptr};
        };

        void EmitEvent(AssetEventKind kind, uint32_t index, uint32_t generation)
        {
            m_eventQueue.PushBack(AssetEvent<T>{kind, wax::Handle<T>{index, generation}});
        }

        comb::DefaultAllocator* m_allocator;
        Slot* m_slots;
        size_t m_capacity;
        size_t m_count;
        uint32_t m_firstFree;
        AssetLoader<T>* m_loader{nullptr};
        T* m_placeholder{nullptr};
        wax::Vector<AssetEvent<T>> m_eventQueue;
        size_t m_bytesUsed{0};
        size_t m_budget{0};
    };
} // namespace nectar
