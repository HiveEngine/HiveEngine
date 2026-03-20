#pragma once

#include <hive/profiling/profiler.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/serialization/byte_buffer.h>

#include <nectar/core/asset_handle.h>
#include <nectar/core/asset_id.h>
#include <nectar/core/asset_status.h>
#include <nectar/core/content_hash.h>
#include <nectar/core/type_id.h>
#include <nectar/io/io_request.h>
#include <nectar/server/asset_event.h>
#include <nectar/server/asset_loader.h>
#include <nectar/server/asset_storage.h>

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

        template <typename T> void RegisterLoader(AssetLoader<T>* loader)
        {
            auto& storage = GetOrCreateStorage<T>();
            storage.SetLoader(loader);
        }

        template <typename T> void RegisterPlaceholder(T* placeholder)
        {
            auto& storage = GetOrCreateStorage<T>();
            storage.SetPlaceholder(placeholder);
        }

        // -- Loading --

        /// Synchronous load from disk.
        template <typename T> StrongHandle<T> Load(wax::StringView path);

        /// Load from raw bytes (for testing / in-memory assets).
        template <typename T> StrongHandle<T> LoadFromMemory(wax::StringView name, wax::ByteSpan data);

        // -- Access --

        /// Returns the loaded asset, or placeholder if not ready. nullptr if nothing available.
        template <typename T> [[nodiscard]] T* Get(const StrongHandle<T>& handle) const
        {
            if (handle.IsNull())
                return GetPlaceholder<T>();
            auto* storage = FindStorage<T>();
            if (!storage)
                return nullptr;
            return storage->GetAssetOrPlaceholder(handle.Raw());
        }

        // -- Status --

        template <typename T> [[nodiscard]] AssetStatus GetStatus(const StrongHandle<T>& handle) const
        {
            if (handle.IsNull())
                return AssetStatus::NOT_LOADED;
            auto* storage = FindStorage<T>();
            if (!storage)
                return AssetStatus::NOT_LOADED;
            return storage->GetStatus(handle.Raw().m_index);
        }

        template <typename T> [[nodiscard]] bool IsReady(const StrongHandle<T>& handle) const
        {
            return GetStatus(handle) == AssetStatus::READY;
        }

        template <typename T> const AssetErrorInfo* GetError(const StrongHandle<T>& handle) const
        {
            if (handle.IsNull())
                return nullptr;
            auto* storage = FindStorage<T>();
            if (!storage)
                return nullptr;
            return storage->GetError(handle.Raw().m_index);
        }

        // -- Lifecycle --

        /// Tick: collect garbage (unload zero-ref assets).
        void Update();

        /// Explicitly release a strong handle (sets it to null).
        template <typename T> void Release(StrongHandle<T>& handle)
        {
            handle = StrongHandle<T>{};
        }

        // -- Weak handle support --

        /// Promote a weak handle to strong. Returns null if the asset was unloaded.
        template <typename T> StrongHandle<T> Lock(const WeakHandle<T>& weak)
        {
            if (weak.IsNull())
                return StrongHandle<T>{};
            auto* storage = FindStorage<T>();
            if (!storage)
                return StrongHandle<T>{};
            if (!storage->IsHandleValid(weak.m_raw.m_index, weak.m_raw.m_generation))
            {
                return StrongHandle<T>{};
            }
            storage->IncrementRef(weak.m_raw.m_index);
            return StrongHandle<T>{weak.m_raw, this};
        }

        // -- Ref counting (called by StrongHandle RAII) --

        template <typename T> void IncrementRef(wax::Handle<T> handle)
        {
            auto* storage = FindStorage<T>();
            if (storage && storage->IsHandleValid(handle.m_index, handle.m_generation))
            {
                storage->IncrementRef(handle.m_index);
            }
        }

        template <typename T> void DecrementRef(wax::Handle<T> handle)
        {
            auto* storage = FindStorage<T>();
            if (storage && storage->IsHandleValid(handle.m_index, handle.m_generation))
            {
                storage->DecrementRef(handle.m_index);
            }
        }

        // -- Stats --

        [[nodiscard]] size_t GetTotalAssetCount() const;

        // -- Events --

        /// Poll one event for type T. Returns true if an event was available.
        template <typename T> bool PollEvents(AssetEvent<T>& out)
        {
            auto* storage = FindStorage<T>();
            if (!storage)
                return false;
            return storage->DrainEvents(&out, 1) > 0;
        }

        // -- GC configuration --

        void SetGcGraceFrames(uint32_t frames)
        {
            m_gcGraceFrames = frames;
        }
        [[nodiscard]] uint32_t GetGcGraceFrames() const
        {
            return m_gcGraceFrames;
        }

        template <typename T> void SetPersistent(const StrongHandle<T>& handle, bool persistent)
        {
            if (handle.IsNull())
                return;
            auto* storage = FindStorage<T>();
            if (storage)
                storage->SetPersistent(handle.Raw().m_index, persistent);
        }

        // -- Budget --

        template <typename T> void SetBudget(size_t bytes)
        {
            auto* storage = FindStorage<T>();
            if (storage)
                storage->SetBudget(bytes);
        }

        template <typename T> [[nodiscard]] size_t GetBytesUsed() const
        {
            auto* storage = FindStorage<T>();
            return storage ? storage->BytesUsed() : 0;
        }

        // -- Hot reload --

        /// Look up a cached handle by path. Returns null handle if not loaded.
        template <typename T> [[nodiscard]] wax::Handle<T> FindHandle(wax::StringView path) const
        {
            nectar::TypeId tid = nectar::TypeIdOf<T>();
            PathKey key{tid, wax::String{*m_allocator}};
            key.m_path.Append(path.Data(), path.Size());
            auto* cached = m_pathCache.Find(key);
            if (!cached)
                return wax::Handle<T>::Invalid();
            auto* storage = FindStorage<T>();
            if (!storage || !storage->IsHandleValid(cached->m_index, cached->m_generation))
                return wax::Handle<T>::Invalid();
            return wax::Handle<T>{cached->m_index, cached->m_generation};
        }

        template <typename T> bool Reload(wax::Handle<T> handle, wax::ByteSpan newData)
        {
            HIVE_PROFILE_SCOPE_N("AssetServer::Reload");
            auto* storage = FindStorage<T>();
            if (!storage)
                return false;
            return storage->ReloadAsset(handle, newData);
        }

    private:
        template <typename T> AssetStorageFor<T>& GetOrCreateStorage();

        template <typename T> AssetStorageFor<T>* FindStorage() const;

        template <typename T> [[nodiscard]] T* GetPlaceholder() const
        {
            auto* storage = FindStorage<T>();
            return storage ? storage->GetPlaceholder() : nullptr;
        }

        /// Sync file read (fallback without VFS)
        wax::ByteBuffer ReadFile(wax::StringView path);

        /// Submit an async load via IOScheduler
        void SubmitAsyncLoad(uint32_t index, uint32_t generation, nectar::TypeId typeId, wax::StringView path);

        // Path cache key — type + path combined
        struct PathKey
        {
            nectar::TypeId m_type{0};
            wax::String m_path{};

            bool operator==(const PathKey& other) const noexcept
            {
                return m_type == other.m_type && m_path.View().Equals(other.m_path.View());
            }
        };

        struct PathKeyHash
        {
            size_t operator()(const PathKey& key) const noexcept
            {
                // FNV-1a combine
                size_t h = static_cast<size_t>(key.m_type);
                auto view = key.m_path.View();
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
            uint32_t m_index{UINT32_MAX};
            uint32_t m_generation{0};
        };

        comb::DefaultAllocator* m_allocator;
        wax::HashMap<nectar::TypeId, IAssetStorage*> m_storages;
        wax::HashMap<PathKey, ErasedHandle, PathKeyHash> m_pathCache;
        wax::String m_basePath;

        IOScheduler* m_io{nullptr};
        uint32_t m_gcGraceFrames{0};

        struct PendingLoad
        {
            uint32_t m_slotIndex;
            uint32_t m_slotGeneration;
            nectar::TypeId m_typeId;
        };
        wax::HashMap<IORequestId, PendingLoad> m_pendingLoads;
    };

    // -- Template implementations that need full AssetServer definition --

    template <typename T> AssetStorageFor<T>& AssetServer::GetOrCreateStorage()
    {
        nectar::TypeId tid = nectar::TypeIdOf<T>();
        auto* existing = m_storages.Find(tid);
        if (existing)
        {
            return *static_cast<AssetStorageFor<T>*>(*existing);
        }

        auto* storage = comb::New<AssetStorageFor<T>>(*m_allocator, *m_allocator, kDefaultStorageCapacity);
        m_storages.Insert(tid, storage);
        return *storage;
    }

    template <typename T> AssetStorageFor<T>* AssetServer::FindStorage() const
    {
        nectar::TypeId tid = nectar::TypeIdOf<T>();
        auto* found = m_storages.Find(tid);
        if (!found)
            return nullptr;
        return static_cast<AssetStorageFor<T>*>(*found);
    }

    template <typename T> StrongHandle<T> AssetServer::Load(wax::StringView path)
    {
        HIVE_PROFILE_SCOPE_N("AssetServer::Load");
        nectar::TypeId tid = nectar::TypeIdOf<T>();
        auto& storage = GetOrCreateStorage<T>();

        PathKey key{tid, wax::String{*m_allocator}};
        key.m_path.Append(path.Data(), path.Size());

        auto* cached = m_pathCache.Find(key);
        if (cached)
        {
            wax::Handle<T> existing{cached->m_index, cached->m_generation};
            if (storage.IsHandleValid(existing.m_index, existing.m_generation))
            {
                storage.IncrementRef(existing.m_index);
                return StrongHandle<T>{existing, this};
            }
            // Stale entry — will be overwritten below
        }

        if (!storage.GetLoader())
        {
            auto handle = storage.AllocateSlot();
            if (handle.IsNull())
                return StrongHandle<T>{};
            storage.SetStatus(handle.m_index, AssetStatus::FAILED);
            storage.SetError(handle.m_index, AssetErrorInfo{AssetError::NO_LOADER, wax::String{}});
            storage.IncrementRef(handle.m_index);
            m_pathCache.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.m_index, handle.m_generation});
            return StrongHandle<T>{handle, this};
        }

        auto handle = storage.AllocateSlot();
        if (handle.IsNull())
            return StrongHandle<T>{};

        // Async path via VFS + IOScheduler
        if (m_io)
        {
            storage.SetStatus(handle.m_index, AssetStatus::QUEUED);
            storage.IncrementRef(handle.m_index);
            m_pathCache.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.m_index, handle.m_generation});
            SubmitAsyncLoad(handle.m_index, handle.m_generation, tid, path);
            return StrongHandle<T>{handle, this};
        }

        // Sync path via direct file read
        storage.SetStatus(handle.m_index, AssetStatus::LOADING);

        auto buffer = ReadFile(path);
        if (buffer.Size() == 0)
        {
            storage.SetStatus(handle.m_index, AssetStatus::FAILED);
            storage.SetError(handle.m_index, AssetErrorInfo{AssetError::FILE_NOT_FOUND, wax::String{}});
            storage.IncrementRef(handle.m_index);
            m_pathCache.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.m_index, handle.m_generation});
            return StrongHandle<T>{handle, this};
        }

        T* asset = storage.GetLoader()->Load(buffer.View(), *m_allocator);
        if (!asset)
        {
            storage.SetStatus(handle.m_index, AssetStatus::FAILED);
            storage.SetError(handle.m_index, AssetErrorInfo{AssetError::LOAD_FAILED, wax::String{}});
            storage.IncrementRef(handle.m_index);
            m_pathCache.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.m_index, handle.m_generation});
            return StrongHandle<T>{handle, this};
        }

        storage.SetAsset(handle, asset);
        storage.SetStatus(handle.m_index, AssetStatus::READY);
        storage.IncrementRef(handle.m_index);
        m_pathCache.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.m_index, handle.m_generation});
        return StrongHandle<T>{handle, this};
    }

    template <typename T> StrongHandle<T> AssetServer::LoadFromMemory(wax::StringView name, wax::ByteSpan data)
    {
        HIVE_PROFILE_SCOPE_N("AssetServer::LoadFromMemory");
        nectar::TypeId tid = nectar::TypeIdOf<T>();
        auto& storage = GetOrCreateStorage<T>();

        PathKey key{tid, wax::String{*m_allocator}};
        key.m_path.Append(name.Data(), name.Size());

        auto* cached = m_pathCache.Find(key);
        if (cached)
        {
            wax::Handle<T> existing{cached->m_index, cached->m_generation};
            if (storage.IsHandleValid(existing.m_index, existing.m_generation))
            {
                storage.IncrementRef(existing.m_index);
                return StrongHandle<T>{existing, this};
            }
        }

        if (!storage.GetLoader())
        {
            auto handle = storage.AllocateSlot();
            if (handle.IsNull())
                return StrongHandle<T>{};
            storage.SetStatus(handle.m_index, AssetStatus::FAILED);
            storage.SetError(handle.m_index, AssetErrorInfo{AssetError::NO_LOADER, wax::String{}});
            storage.IncrementRef(handle.m_index);
            m_pathCache.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.m_index, handle.m_generation});
            return StrongHandle<T>{handle, this};
        }

        auto handle = storage.AllocateSlot();
        if (handle.IsNull())
            return StrongHandle<T>{};

        storage.SetStatus(handle.m_index, AssetStatus::LOADING);

        T* asset = storage.GetLoader()->Load(data, *m_allocator);
        if (!asset)
        {
            storage.SetStatus(handle.m_index, AssetStatus::FAILED);
            storage.SetError(handle.m_index, AssetErrorInfo{AssetError::LOAD_FAILED, wax::String{}});
            storage.IncrementRef(handle.m_index);
            m_pathCache.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.m_index, handle.m_generation});
            return StrongHandle<T>{handle, this};
        }

        storage.SetAsset(handle, asset);
        storage.SetStatus(handle.m_index, AssetStatus::READY);
        storage.IncrementRef(handle.m_index);
        m_pathCache.Insert(static_cast<PathKey&&>(key), ErasedHandle{handle.m_index, handle.m_generation});
        return StrongHandle<T>{handle, this};
    }
} // namespace nectar

// StrongHandle method bodies that depend on AssetServer
#include <nectar/core/asset_handle_impl.h>
