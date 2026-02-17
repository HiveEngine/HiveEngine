#define _CRT_SECURE_NO_WARNINGS
#include <nectar/server/asset_server.h>
#include <nectar/io/io_scheduler.h>
#include <hive/profiling/profiler.h>
#include <cstdio>

namespace nectar
{
    AssetServer::AssetServer(comb::DefaultAllocator& alloc)
        : allocator_{&alloc}
        , storages_{alloc, 16}
        , path_cache_{alloc, 64}
        , base_path_{alloc}
        , pending_loads_{alloc, 16}
    {}

    AssetServer::AssetServer(comb::DefaultAllocator& alloc, VirtualFilesystem& vfs, IOScheduler& io)
        : allocator_{&alloc}
        , storages_{alloc, 16}
        , path_cache_{alloc, 64}
        , base_path_{alloc}
        , vfs_{&vfs}
        , io_{&io}
        , pending_loads_{alloc, 64}
    {}

    AssetServer::~AssetServer()
    {
        // Destroy all storages
        for (auto it = storages_.begin(); it != storages_.end(); ++it)
        {
            IAssetStorage* storage = it.Value();
            storage->~IAssetStorage();
            allocator_->Deallocate(storage);
        }
    }

    void AssetServer::Update()
    {
        HIVE_PROFILE_SCOPE_N("AssetServer::Update");
        if (io_)
        {
            wax::Vector<IOCompletion> completions{*allocator_};
            io_->DrainCompletions(completions);

            for (size_t i = 0; i < completions.Size(); ++i)
            {
                auto* pending = pending_loads_.Find(completions[i].request_id);
                if (!pending)
                    continue;

                auto* found = storages_.Find(pending->type_id);
                if (!found)
                {
                    pending_loads_.Remove(completions[i].request_id);
                    continue;
                }

                IAssetStorage* storage = *found;

                if (!completions[i].success)
                {
                    storage->SetStatus(pending->slot_index, AssetStatus::Failed);
                    storage->SetError(pending->slot_index,
                                      AssetErrorInfo{AssetError::FileNotFound, wax::String<>{}});
                }
                else
                {
                    storage->SetStatus(pending->slot_index, AssetStatus::Loading);
                    bool ok = storage->LoadFromData(pending->slot_index, pending->slot_generation,
                                                    completions[i].data.View(), *allocator_);
                    if (ok)
                        storage->SetStatus(pending->slot_index, AssetStatus::Ready);
                    else
                    {
                        storage->SetStatus(pending->slot_index, AssetStatus::Failed);
                        storage->SetError(pending->slot_index,
                                          AssetErrorInfo{AssetError::LoadFailed, wax::String<>{}});
                    }
                }

                pending_loads_.Remove(completions[i].request_id);
            }
        }

        for (auto it = storages_.begin(); it != storages_.end(); ++it)
        {
            it.Value()->CollectGarbage(gc_grace_frames_);
        }
    }

    size_t AssetServer::GetTotalAssetCount() const
    {
        size_t total = 0;
        for (auto it = storages_.begin(); it != storages_.end(); ++it)
        {
            total += it.Value()->Count();
        }
        return total;
    }

    void AssetServer::SubmitAsyncLoad(uint32_t index, uint32_t generation,
                                       nectar::TypeId type_id, wax::StringView path)
    {
        IORequestId req_id = io_->Submit(path, LoadPriority::Normal);
        pending_loads_.Insert(req_id, PendingLoad{index, generation, type_id});
    }

    wax::ByteBuffer<comb::DefaultAllocator> AssetServer::ReadFile(wax::StringView path)
    {
        wax::ByteBuffer<comb::DefaultAllocator> buffer{*allocator_};

        // Build full path
        wax::String<comb::DefaultAllocator> full_path{*allocator_};
        if (base_path_.Size() > 0)
        {
            full_path.Append(base_path_.View().Data(), base_path_.View().Size());
            full_path.Append("/", 1);
        }
        full_path.Append(path.Data(), path.Size());

        FILE* file = std::fopen(full_path.CStr(), "rb");
        if (!file) return buffer;

        std::fseek(file, 0, SEEK_END);
        long file_size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        if (file_size <= 0)
        {
            std::fclose(file);
            return buffer;
        }

        buffer.Resize(static_cast<size_t>(file_size));
        size_t read = std::fread(buffer.Data(), 1, static_cast<size_t>(file_size), file);
        std::fclose(file);

        if (read != static_cast<size_t>(file_size))
        {
            buffer.Clear();
        }

        return buffer;
    }
}
