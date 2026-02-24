#pragma once

#include <nectar/core/asset_status.h>
#include <nectar/server/asset_loader.h>
#include <nectar/server/asset_event.h>
#include <wax/pointers/handle.h>
#include <wax/containers/vector.h>
#include <wax/serialization/byte_span.h>
#include <hive/core/assert.h>
#include <comb/default_allocator.h>
#include <comb/new.h>
#include <nectar/core/type_id.h>
#include <cstdint>
#include <cstddef>

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
        virtual size_t CollectGarbage(uint32_t gc_grace_frames) noexcept = 0;

        /// Load asset from raw data into an existing slot. Returns true on success.
        virtual bool LoadFromData(uint32_t index, uint32_t generation,
                                  wax::ByteSpan data, comb::DefaultAllocator& alloc) noexcept = 0;

        virtual size_t Count() const noexcept = 0;
        virtual size_t Capacity() const noexcept = 0;

        // -- Events --

        /// Drain queued events into a type-erased buffer. Returns number drained.
        virtual size_t DrainEvents(void* out_buffer, size_t max_count) noexcept = 0;

        // -- GC / Budget --

        virtual void SetPersistent(uint32_t index, bool persistent) noexcept = 0;
        virtual size_t BytesUsed() const noexcept = 0;
        virtual void SetBudget(size_t bytes) noexcept = 0;
    };

    /// Concrete typed storage for assets of type T.
    /// Manages slots with generation counters, ref counts, status, and loaded asset pointers.
    template<typename T>
    class AssetStorageFor final : public IAssetStorage
    {
    public:
        AssetStorageFor(comb::DefaultAllocator& alloc, size_t capacity)
            : allocator_{&alloc}
            , capacity_{capacity}
            , count_{0}
            , first_free_{0}
            , event_queue_{alloc}
        {
            hive::Assert(capacity > 0, "AssetStorageFor capacity must be > 0");
            hive::Assert(capacity <= UINT32_MAX, "AssetStorageFor capacity exceeds max");

            slots_ = static_cast<Slot*>(alloc.Allocate(sizeof(Slot) * capacity, alignof(Slot)));
            hive::Check(slots_ != nullptr, "Failed to allocate AssetStorageFor slots");

            for (size_t i = 0; i < capacity_ - 1; ++i)
            {
                new (&slots_[i]) Slot{};
                slots_[i].next_free = static_cast<uint32_t>(i + 1);
            }
            new (&slots_[capacity_ - 1]) Slot{};
            slots_[capacity_ - 1].next_free = UINT32_MAX;
        }

        ~AssetStorageFor() override
        {
            for (size_t i = 0; i < capacity_; ++i)
            {
                if (slots_[i].alive && slots_[i].asset && loader_)
                {
                    loader_->Unload(slots_[i].asset, *allocator_);
                }
                slots_[i].~Slot();
            }

            if (slots_ && allocator_)
            {
                allocator_->Deallocate(slots_);
            }
        }

        AssetStorageFor(const AssetStorageFor&) = delete;
        AssetStorageFor& operator=(const AssetStorageFor&) = delete;

        nectar::TypeId GetTypeId() const noexcept override
        {
            return nectar::TypeIdOf<T>();
        }

        void SetLoader(AssetLoader<T>* loader) noexcept { loader_ = loader; }
        [[nodiscard]] AssetLoader<T>* GetLoader() const noexcept { return loader_; }

        void SetPlaceholder(T* placeholder) noexcept { placeholder_ = placeholder; }
        [[nodiscard]] T* GetPlaceholder() const noexcept { return placeholder_; }

        /// Allocate a new slot. Returns Handle::Invalid() if full.
        [[nodiscard]] wax::Handle<T> AllocateSlot() noexcept
        {
            if (first_free_ == UINT32_MAX)
            {
                return wax::Handle<T>::Invalid();
            }

            uint32_t index = first_free_;
            Slot& slot = slots_[index];
            first_free_ = slot.next_free;

            slot.alive = true;
            slot.ref_count = 0;
            slot.status = AssetStatus::NotLoaded;
            slot.error = AssetErrorInfo{};
            slot.asset = nullptr;
            ++count_;

            return wax::Handle<T>{index, slot.generation};
        }

        void SetAsset(wax::Handle<T> handle, T* asset) noexcept
        {
            hive::Assert(!handle.IsNull() && handle.index < capacity_, "Invalid handle in SetAsset");
            Slot& slot = slots_[handle.index];
            hive::Assert(slot.alive && slot.generation == handle.generation, "Stale handle in SetAsset");
            slot.asset = asset;
            if (asset && loader_)
                bytes_used_ += loader_->SizeOf(asset);
        }

        [[nodiscard]] T* GetAsset(wax::Handle<T> handle) const noexcept
        {
            if (handle.IsNull() || handle.index >= capacity_) return nullptr;
            const Slot& slot = slots_[handle.index];
            if (!slot.alive || slot.generation != handle.generation) return nullptr;
            return slot.asset;
        }

        /// Returns the loaded asset if ready, or the placeholder otherwise.
        [[nodiscard]] T* GetAssetOrPlaceholder(wax::Handle<T> handle) const noexcept
        {
            if (handle.IsNull() || handle.index >= capacity_) return placeholder_;
            const Slot& slot = slots_[handle.index];
            if (!slot.alive || slot.generation != handle.generation) return placeholder_;
            if (slot.status == AssetStatus::Ready && slot.asset) return slot.asset;
            return placeholder_;
        }

        // -- IAssetStorage interface --

        void IncrementRef(uint32_t index) noexcept override
        {
            hive::Assert(index < capacity_ && slots_[index].alive, "IncrementRef on dead slot");
            ++slots_[index].ref_count;
        }

        void DecrementRef(uint32_t index) noexcept override
        {
            hive::Assert(index < capacity_ && slots_[index].alive, "DecrementRef on dead slot");
            hive::Assert(slots_[index].ref_count > 0, "DecrementRef below zero");
            --slots_[index].ref_count;
        }

        uint32_t GetRefCount(uint32_t index) const noexcept override
        {
            if (index >= capacity_ || !slots_[index].alive) return 0;
            return slots_[index].ref_count;
        }

        AssetStatus GetStatus(uint32_t index) const noexcept override
        {
            if (index >= capacity_ || !slots_[index].alive) return AssetStatus::NotLoaded;
            return slots_[index].status;
        }

        void SetStatus(uint32_t index, AssetStatus status) noexcept override
        {
            hive::Assert(index < capacity_ && slots_[index].alive, "SetStatus on dead slot");
            auto old = slots_[index].status;
            slots_[index].status = status;

            if (status == AssetStatus::Ready && old != AssetStatus::Ready)
                EmitEvent(AssetEventKind::Loaded, index, slots_[index].generation);
            else if (status == AssetStatus::Failed && old != AssetStatus::Failed)
                EmitEvent(AssetEventKind::Failed, index, slots_[index].generation);
        }

        const AssetErrorInfo* GetError(uint32_t index) const noexcept override
        {
            if (index >= capacity_ || !slots_[index].alive) return nullptr;
            return &slots_[index].error;
        }

        void SetError(uint32_t index, AssetErrorInfo error) noexcept override
        {
            hive::Assert(index < capacity_ && slots_[index].alive, "SetError on dead slot");
            slots_[index].error = static_cast<AssetErrorInfo&&>(error);
        }

        bool IsHandleValid(uint32_t index, uint32_t generation) const noexcept override
        {
            if (index >= capacity_) return false;
            return slots_[index].alive && slots_[index].generation == generation;
        }

        void UnloadSlot(uint32_t index, uint32_t generation) noexcept override
        {
            if (index >= capacity_) return;
            Slot& slot = slots_[index];
            if (!slot.alive || slot.generation != generation) return;

            EmitEvent(AssetEventKind::Unloaded, index, generation);

            if (slot.asset && loader_)
            {
                size_t sz = loader_->SizeOf(slot.asset);
                if (sz <= bytes_used_) bytes_used_ -= sz;
                loader_->Unload(slot.asset, *allocator_);
            }

            slot.asset = nullptr;
            slot.alive = false;
            slot.status = AssetStatus::Unloaded;
            slot.error = AssetErrorInfo{};
            slot.ref_count = 0;
            slot.gc_countdown = 0;
            slot.persistent = false;
            ++slot.generation;
            slot.next_free = first_free_;
            first_free_ = index;
            --count_;
        }

        size_t CollectGarbage(uint32_t gc_grace_frames) noexcept override
        {
            size_t collected = 0;
            bool over_budget = (budget_ > 0 && bytes_used_ > budget_);

            for (size_t i = 0; i < capacity_; ++i)
            {
                Slot& slot = slots_[i];
                if (!slot.alive) continue;

                // Reset countdown if asset got re-referenced
                if (slot.ref_count > 0 && slot.gc_countdown > 0)
                {
                    slot.gc_countdown = 0;
                    continue;
                }

                if (slot.ref_count != 0 || slot.status != AssetStatus::Ready) continue;
                if (slot.persistent) continue;

                // Over budget → immediate unload (no grace period)
                if (over_budget)
                {
                    UnloadSlot(static_cast<uint32_t>(i), slot.generation);
                    ++collected;
                    over_budget = (budget_ > 0 && bytes_used_ > budget_);
                    continue;
                }

                // Grace period logic
                if (gc_grace_frames == 0)
                {
                    UnloadSlot(static_cast<uint32_t>(i), slot.generation);
                    ++collected;
                    continue;
                }

                if (slot.gc_countdown == 0)
                {
                    slot.gc_countdown = gc_grace_frames;
                }
                else
                {
                    --slot.gc_countdown;
                    if (slot.gc_countdown == 0)
                    {
                        UnloadSlot(static_cast<uint32_t>(i), slot.generation);
                        ++collected;
                    }
                }
            }
            return collected;
        }

        bool LoadFromData(uint32_t index, uint32_t generation,
                          wax::ByteSpan data, comb::DefaultAllocator& alloc) noexcept override
        {
            if (index >= capacity_ || !slots_[index].alive || slots_[index].generation != generation)
                return false;
            if (!loader_)
                return false;

            T* asset = loader_->Load(data, alloc);
            if (!asset)
                return false;

            slots_[index].asset = asset;
            bytes_used_ += loader_->SizeOf(asset);
            return true;
        }

        size_t Count() const noexcept override { return count_; }
        size_t Capacity() const noexcept override { return capacity_; }

        // -- Events --

        size_t DrainEvents(void* out_buffer, size_t max_count) noexcept override
        {
            size_t count = event_queue_.Size() < max_count ? event_queue_.Size() : max_count;
            auto* dst = static_cast<AssetEvent<T>*>(out_buffer);
            for (size_t i = 0; i < count; ++i)
                dst[i] = event_queue_[i];

            // Remove drained events (shift remainder)
            if (count == event_queue_.Size())
            {
                event_queue_.Clear();
            }
            else
            {
                wax::Vector<AssetEvent<T>> remaining{*allocator_};
                for (size_t i = count; i < event_queue_.Size(); ++i)
                    remaining.PushBack(event_queue_[i]);
                event_queue_ = static_cast<wax::Vector<AssetEvent<T>>&&>(remaining);
            }

            return count;
        }

        // -- GC / Budget --

        void SetPersistent(uint32_t index, bool persistent) noexcept override
        {
            if (index < capacity_ && slots_[index].alive)
                slots_[index].persistent = persistent;
        }

        size_t BytesUsed() const noexcept override { return bytes_used_; }

        void SetBudget(size_t bytes) noexcept override { budget_ = bytes; }

        /// Reload an asset in-place. Swaps old → new, emits Reloaded event.
        bool ReloadAsset(wax::Handle<T> handle, wax::ByteSpan data)
        {
            if (handle.IsNull() || handle.index >= capacity_) return false;
            Slot& slot = slots_[handle.index];
            if (!slot.alive || slot.generation != handle.generation) return false;
            if (!loader_) return false;

            T* new_asset = loader_->Load(data, *allocator_);
            if (!new_asset) return false;

            // Unload old
            if (slot.asset)
            {
                size_t old_sz = loader_->SizeOf(slot.asset);
                if (old_sz <= bytes_used_) bytes_used_ -= old_sz;
                loader_->Unload(slot.asset, *allocator_);
            }

            slot.asset = new_asset;
            slot.status = AssetStatus::Ready;
            bytes_used_ += loader_->SizeOf(new_asset);
            EmitEvent(AssetEventKind::Reloaded, handle.index, handle.generation);
            return true;
        }

    private:
        struct Slot
        {
            uint32_t generation{0};
            uint32_t next_free{UINT32_MAX};
            uint32_t ref_count{0};
            uint32_t gc_countdown{0};
            bool alive{false};
            bool persistent{false};
            AssetStatus status{AssetStatus::NotLoaded};
            AssetErrorInfo error{};
            T* asset{nullptr};
        };

        void EmitEvent(AssetEventKind kind, uint32_t index, uint32_t generation)
        {
            event_queue_.PushBack(AssetEvent<T>{kind, wax::Handle<T>{index, generation}});
        }

        comb::DefaultAllocator* allocator_;
        Slot* slots_;
        size_t capacity_;
        size_t count_;
        uint32_t first_free_;
        AssetLoader<T>* loader_{nullptr};
        T* placeholder_{nullptr};
        wax::Vector<AssetEvent<T>> event_queue_;
        size_t bytes_used_{0};
        size_t budget_{0};
    };
}
