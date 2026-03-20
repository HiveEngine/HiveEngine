#include <nectar/io/io_scheduler.h>
#include <nectar/server/asset_server.h>

#include <cstdio>

namespace nectar
{
    AssetServer::AssetServer(comb::DefaultAllocator& alloc)
        : m_allocator{&alloc}
        , m_storages{alloc, 16}
        , m_pathCache{alloc, 64}
        , m_basePath{alloc}
        , m_pendingLoads{alloc, 16}
    {
    }

    AssetServer::AssetServer(comb::DefaultAllocator& alloc, VirtualFilesystem& vfs, IOScheduler& io)
        : m_allocator{&alloc}
        , m_storages{alloc, 16}
        , m_pathCache{alloc, 64}
        , m_basePath{alloc}
        , m_io{&io}
        , m_pendingLoads{alloc, 64}
    {
    }

    AssetServer::~AssetServer()
    {
        for (auto it = m_storages.Begin(); it != m_storages.End(); ++it)
        {
            IAssetStorage* storage = it.Value();
            if (storage != nullptr)
            {
                storage->Destroy(*m_allocator);
            }
        }
    }

    void AssetServer::Update()
    {
        HIVE_PROFILE_SCOPE_N("AssetServer::Update");

        if (m_io != nullptr)
        {
            wax::Vector<IOCompletion> completions{*m_allocator};
            m_io->DrainCompletions(completions);

            for (size_t i = 0; i < completions.Size(); ++i)
            {
                auto* pending = m_pendingLoads.Find(completions[i].m_requestId);
                if (pending == nullptr)
                {
                    continue;
                }

                auto* found = m_storages.Find(pending->m_typeId);
                if (found == nullptr)
                {
                    m_pendingLoads.Remove(completions[i].m_requestId);
                    continue;
                }

                IAssetStorage* storage = *found;
                if (!completions[i].m_success)
                {
                    storage->SetStatus(pending->m_slotIndex, AssetStatus::FAILED);
                    storage->SetError(pending->m_slotIndex,
                                      AssetErrorInfo{AssetError::FILE_NOT_FOUND, wax::String{*m_allocator}});
                }
                else
                {
                    storage->SetStatus(pending->m_slotIndex, AssetStatus::LOADING);

                    const bool ok = storage->LoadFromData(pending->m_slotIndex, pending->m_slotGeneration,
                                                          completions[i].m_data.View(), *m_allocator);

                    // Release completed IO data once the asset copy has been made.
                    completions[i].m_data = wax::ByteBuffer{*m_allocator};

                    if (ok)
                    {
                        storage->SetStatus(pending->m_slotIndex, AssetStatus::READY);
                    }
                    else
                    {
                        storage->SetStatus(pending->m_slotIndex, AssetStatus::FAILED);
                        storage->SetError(pending->m_slotIndex,
                                          AssetErrorInfo{AssetError::LOAD_FAILED, wax::String{*m_allocator}});
                    }
                }

                m_pendingLoads.Remove(completions[i].m_requestId);
            }
        }

        for (auto it = m_storages.Begin(); it != m_storages.End(); ++it)
        {
            it.Value()->CollectGarbage(m_gcGraceFrames);
        }
    }

    size_t AssetServer::GetTotalAssetCount() const
    {
        size_t total = 0;
        for (auto it = m_storages.Begin(); it != m_storages.End(); ++it)
        {
            total += it.Value()->Count();
        }
        return total;
    }

    void AssetServer::SubmitAsyncLoad(uint32_t index, uint32_t generation, nectar::TypeId typeId, wax::StringView path)
    {
        HIVE_PROFILE_SCOPE_N("AssetServer::SubmitAsyncLoad");

        if (m_io == nullptr)
        {
            return;
        }

        const IORequestId requestId = m_io->Submit(path, LoadPriority::NORMAL);
        m_pendingLoads.Insert(requestId, PendingLoad{index, generation, typeId});
    }

    wax::ByteBuffer AssetServer::ReadFile(wax::StringView path)
    {
        wax::ByteBuffer buffer{*m_allocator};

        wax::String fullPath{*m_allocator};
        if (m_basePath.Size() > 0)
        {
            fullPath.Append(m_basePath.View().Data(), m_basePath.View().Size());
            fullPath.Append("/", 1);
        }
        fullPath.Append(path.Data(), path.Size());

        FILE* file = std::fopen(fullPath.CStr(), "rb");
        if (file == nullptr)
        {
            return buffer;
        }

        std::fseek(file, 0, SEEK_END);
        const long fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        if (fileSize <= 0)
        {
            std::fclose(file);
            return buffer;
        }

        buffer.Resize(static_cast<size_t>(fileSize));
        const size_t read = std::fread(buffer.Data(), 1, static_cast<size_t>(fileSize), file);
        std::fclose(file);

        if (read != static_cast<size_t>(fileSize))
        {
            buffer.Clear();
        }

        return buffer;
    }
} // namespace nectar
