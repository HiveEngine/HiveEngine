#pragma once

#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <nectar/core/asset_status.h>
#include <nectar/core/asset_handle.h>
#include <nectar/server/asset_loader.h>
#include <nectar/server/asset_storage.h>
#include <nectar/server/asset_event.h>
#include <nectar/io/io_request.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/serialization/byte_buffer.h>
#include <nectar/core/type_id.h>
#include <comb/default_allocator.h>
#include <comb/new.h>
#include <hive/profiling/profiler.h>

namespace nectar
{
    class VirtualFilesystem;
    class IOScheduler;

    static constexpr size_t kDefaultStorageCapacity = 4096;

    class AssetServer
    {
    public:
        explicit AssetServer(comb::DefaultAllocator& alloc);
        AssetServer(comb::DefaultAllocator& alloc, VirtualFilesystem& vfs, IOScheduler& io);
        ~AssetServer();

        AssetServer(const AssetServer&) = delete;
        AssetServer& operator=(const AssetServer&) = delete;

        // -- Registration --

        template<typename T>
        void RegisterLoader(AssetLoader<T>* loader)
        {
            auto& storage = GetOrCreateStorage<T>();
            storage.SetLoader(loader);
        }

        template<typename T>
        void RegisterPlaceholder(T* placeholder)
        {
            auto& storage = GetOrCreateStorage<T>();
            storage.SetPlaceholder(placeholder);
        }

        // -- Loading --

        /// Synchronous load from disk.
        template<typename T>
        StrongHandle<T> Load(wax::StringView path);

        /// Load from raw bytes (for testing / in-memory assets).
        template<typename T>
        StrongHandle<T> LoadFromMemory(wax::StringView name, wax::ByteSpan data);

        // -- Access --

        /// Returns the loaded asset, or placeholder if not ready. nullptr if nothing available.
        template<typename T>
        T* Get(const StrongHandle<T>& handle) const
        {
            if (handle.IsNull()) return GetPlaceholder<T>();
            auto* storage = FindStorage<T>();
            if (!storage) return nullptr;
            return storage->GetAssetOrPlaceholder(handle.Raw());
        }

        // -- Status --

        template<typename T>
        AssetStatus GetStatus(const StrongHandle<T>& handle) const
        {
            if (handle.IsNull()) return AssetStatus::NotLoaded;
            auto* storage = FindStorage<T>();
            if (!storage) return AssetStatus::NotLoaded;
            return storage->GetStatus(handle.Raw().index);
        }

        template<typename T>
        bool IsReady(const StrongHandle<T>& handle) const
        {
            return GetStatus(handle) == AssetStatus::Ready;
        }

        template<typename T>
        const AssetErrorInfo* GetError(const StrongHandle<T>& handle) const
        {
            if (handle.IsNull()) return nullptr;
            auto* storage = FindStorage<T>();
            if (!storage) return nullptr;
            return storage->GetError(handle.Raw().index);
        }

        // -- Lifecycle --

        /// Tick: collect garbage (unload zero-ref assets).
        void Update();

        /// Explicitly release a strong handle (sets it to null).
        template<typename T>
        void Release(StrongHandle<T>& handle)
        {
            handle = StrongHandle<T>{};
        }

        // -- Weak handle support --

        /// Promote a weak handle to strong. Returns null if the asset was unloaded.
        template<typename T>
        StrongHandle<T> Lock(const WeakHandle<T>& weak)
        {
            if (weak.IsNull()) return StrongHandle<T>{};
            auto* storage = FindStorage<T>();
            if (!storage) return StrongHandle<T>{};
            if (!storage->IsHandleValid(weak.raw.index, weak.raw.generation))
            {
                return StrongHandle<T>{};
            }
            storage->IncrementRef(weak.raw.index);
            return StrongHandle<T>{weak.raw, this};
        }

        // -- Ref counting (called by StrongHandle RAII) --

        template<typename T>
        void IncrementRef(wax::Handle<T> handle)
        {
            auto* storage = FindStorage<T>();
            if (storage && storage->IsHandleValid(handle.index, handle.generation))
            {
                storage->IncrementRef(handle.index);
            }
        }

        template<typename T>
        void DecrementRef(wax::Handle<T> handle)
        {
            auto* storage = FindStorage<T>();
            if (storage && storage->IsHandleValid(handle.index, handle.generation))
            {
                storage->DecrementRef(handle.index);
            }
        }

        // -- Stats --

        size_t GetTotalAssetCount() const;

        // -- Events --

        /// Poll one event for type T. Returns true if an event was available.
        template<typename T>
        bool PollEvents(AssetEvent<T>& out)
        {
            auto* storage = FindStorage<T>();
            if (!storage) return false;
            return storage->DrainEvents(&out, 1) > 0;
        }

        // -- GC configuration --

        void SetGcGraceFrames(uint32_t frames) { gc_grace_frames_ = frames; }
        [[nodiscard]] uint32_t GetGcGraceFrames() const { return gc_grace_frames_; }

        template<typename T>
        void SetPersistent(const StrongHandle<T>& handle, bool persistent)
        {
            if (handle.IsNull()) return;
            auto* storage = FindStorage<T>();
            if (storage)
                storage->SetPersistent(handle.Raw().index, persistent);
        }

        // -- Budget --

        template<typename T>
        void SetBudget(size_t bytes)
        {
            auto* storage = FindStorage<T>();
            if (storage) storage->SetBudget(bytes);
        }

        template<typename T>
        size_t GetBytesUsed() const
        {
            auto* storage = FindStorage<T>();
            return storage ? storage->BytesUsed() : 0;
        }

        // -- Hot reload --

        template<typename T>
        bool Reload(wax::Handle<T> handle, wax::ByteSpan new_data)
        {
            HIVE_PROFILE_SCOPE_N("AssetServer::Reload");
            auto* storage = FindStorage<T>();
            if (!storage) return false;
            return storage->ReloadAsset(handle, new_data);
        }

    private:
        template<typename T>
        AssetStorageFor<T>& GetOrCreateStorage();

        template<typename T>
        AssetStorageFor<T>* FindStorage() const;

        template<typename T>
        T* GetPlaceholder() const
        {
            auto* storage = FindStorage<T>();
            return storage ? storage->GetPlaceholder() : nullptr;
        }

        /// Sync file read (fallback without VFS)
        wax::ByteBuffer<comb::DefaultAllocator> ReadFile(wax::StringView path);

        /// Submit an async load via IOScheduler
        void SubmitAsyncLoad(uint32_t index, uint32_t generation,
                             nectar::TypeId type_id, wax::StringView path);

        // Path cache key — type + path combined
        struct PathKey
        {
            nectar::TypeId type{0};
            wax::String<> path{};

            bool operator==(const PathKey& other) const noexcept
            {
                return type == other.type && path.View().Equals(other.path.View());
            }
        };

        struct PathKeyHash
        {
            size_t operator()(const PathKey& key) const noexcept
            {
                // FNV-1a combine
                size_t h = static_cast<size_t>(key.type);
                auto view = key.path.View();
                for (size_t i = 0; i < view.Size(); ++i)
                {
                    h ^= static_cast<size_t>(view[i]);
                    h *= 1099511628211ULL;
                }
                return h;
            }
        };

        struct ErasedHandle
        {
            uint32_t index{UINT32_MAX};
            uint32_t generation{0};
        };

        comb::DefaultAllocator* allocator_;
        wax::HashMap<nectar::TypeId, IAssetStorage*, comb::DefaultAllocator> storages_;
        wax::HashMap<PathKey, ErasedHandle, comb::DefaultAllocator, PathKeyHash> path_cache_;
        wax::String<comb::DefaultAllocator> base_path_;

        VirtualFilesystem* vfs_{nullptr};
        IOScheduler* io_{nullptr};
        uint32_t gc_grace_frames_{0};

        struct PendingLoad
        {
            uint32_t slot_index;
            uint32_t slot_generation;
            nectar::TypeId type_id;
        };
        wax::HashMap<IORequestId, PendingLoad, comb::DefaultAllocator> pending_loads_;
    };

    // -- Template implementations that need full AssetServer definition --

    template<typename T>
    AssetStorageFor<T>& AssetServer::GetOrCreateStorage()
    {
        nectar::TypeId tid = nectar::TypeIdOf<T>();
        auto* existing = storages_.Find(tid);
        if (existing)
        {
            return *static_cast<AssetStorageFor<T>*>(*existing);
        }

        auto* storage = comb::New<AssetStorageFor<T>>(*allocator_, *allocator_, kDefaultStorageCapacity);
        storages_.Insert(tid, storage);
        return *storage;
    }

    template<typename T>
    AssetStorageFor<T>* AssetServer::FindStorage() const
    {
        nectar::TypeId tid = nectar::TypeIdOf<T>();
        auto* found = storages_.Find(tid);
        if (!found) return nullptr;
        return static_cast<AssetStorageFor<T>*>(*found);
    }

    template<typename T>
    StrongHandle<T> AssetServer::Load(wax::StringView path)
    {
        HIVE_PROFILE_SCOPE_N("AssetServer::Load");
        nectar::TypeId tid = nectar::TypeIdOf<T>();
        auto& storage = GetOrCreateStorage<T>();

        // Check path cache
        PathKey key{tid, wax::String<>{*allocator_}};
        key.path.Append(path.Data(), path.Size());

        auto* cached = path_cache_.Find(key);
        if (cached)
        {
            wax::Handle<T> existing{cached->index, cached->generation};
            if (storage.IsHandleValid(existing.index, existing.generation))
            {
                storage.IncrementRef(existing.index);
                return StrongHandle<T>{existing, this};
            }
            // Stale entry — will be overwritten below
        }

        // Check loader
        if (!storage.GetLoader())
        {
            auto handle = storage.AllocateSlot();
            if (handle.IsNull()) return StrongHandle<T>{};
            storage.SetStatus(handle.index, AssetStatus::Failed);
            storage.SetError(handle.index, AssetErrorInfo{AssetError::NoLoader, wax::String<>{}});
            storage.IncrementRef(handle.index);
            path_cache_.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.index, handle.generation});
            return StrongHandle<T>{handle, this};
        }

        // Allocate slot
        auto handle = storage.AllocateSlot();
        if (handle.IsNull()) return StrongHandle<T>{};

        // Async path via VFS + IOScheduler
        if (io_)
        {
            storage.SetStatus(handle.index, AssetStatus::Queued);
            storage.IncrementRef(handle.index);
            path_cache_.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.index, handle.generation});
            SubmitAsyncLoad(handle.index, handle.generation, tid, path);
            return StrongHandle<T>{handle, this};
        }

        // Sync path via direct file read
        storage.SetStatus(handle.index, AssetStatus::Loading);

        auto buffer = ReadFile(path);
        if (buffer.Size() == 0)
        {
            storage.SetStatus(handle.index, AssetStatus::Failed);
            storage.SetError(handle.index, AssetErrorInfo{AssetError::FileNotFound, wax::String<>{}});
            storage.IncrementRef(handle.index);
            path_cache_.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.index, handle.generation});
            return StrongHandle<T>{handle, this};
        }

        T* asset = storage.GetLoader()->Load(buffer.View(), *allocator_);
        if (!asset)
        {
            storage.SetStatus(handle.index, AssetStatus::Failed);
            storage.SetError(handle.index, AssetErrorInfo{AssetError::LoadFailed, wax::String<>{}});
            storage.IncrementRef(handle.index);
            path_cache_.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.index, handle.generation});
            return StrongHandle<T>{handle, this};
        }

        storage.SetAsset(handle, asset);
        storage.SetStatus(handle.index, AssetStatus::Ready);
        storage.IncrementRef(handle.index);
        path_cache_.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.index, handle.generation});
        return StrongHandle<T>{handle, this};
    }

    template<typename T>
    StrongHandle<T> AssetServer::LoadFromMemory(wax::StringView name, wax::ByteSpan data)
    {
        HIVE_PROFILE_SCOPE_N("AssetServer::LoadFromMemory");
        nectar::TypeId tid = nectar::TypeIdOf<T>();
        auto& storage = GetOrCreateStorage<T>();

        // Check cache
        PathKey key{tid, wax::String<>{*allocator_}};
        key.path.Append(name.Data(), name.Size());

        auto* cached = path_cache_.Find(key);
        if (cached)
        {
            wax::Handle<T> existing{cached->index, cached->generation};
            if (storage.IsHandleValid(existing.index, existing.generation))
            {
                storage.IncrementRef(existing.index);
                return StrongHandle<T>{existing, this};
            }
        }

        // Check loader
        if (!storage.GetLoader())
        {
            auto handle = storage.AllocateSlot();
            if (handle.IsNull()) return StrongHandle<T>{};
            storage.SetStatus(handle.index, AssetStatus::Failed);
            storage.SetError(handle.index, AssetErrorInfo{AssetError::NoLoader, wax::String<>{}});
            storage.IncrementRef(handle.index);
            path_cache_.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.index, handle.generation});
            return StrongHandle<T>{handle, this};
        }

        auto handle = storage.AllocateSlot();
        if (handle.IsNull()) return StrongHandle<T>{};

        storage.SetStatus(handle.index, AssetStatus::Loading);

        T* asset = storage.GetLoader()->Load(data, *allocator_);
        if (!asset)
        {
            storage.SetStatus(handle.index, AssetStatus::Failed);
            storage.SetError(handle.index, AssetErrorInfo{AssetError::LoadFailed, wax::String<>{}});
            storage.IncrementRef(handle.index);
            path_cache_.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.index, handle.generation});
            return StrongHandle<T>{handle, this};
        }

        storage.SetAsset(handle, asset);
        storage.SetStatus(handle.index, AssetStatus::Ready);
        storage.IncrementRef(handle.index);
        path_cache_.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.index, handle.generation});
        return StrongHandle<T>{handle, this};
    }
}

// StrongHandle method bodies that depend on AssetServer
#include <nectar/core/asset_handle_impl.h>
