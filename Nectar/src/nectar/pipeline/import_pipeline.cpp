#include <nectar/cas/cas_store.h>
#include <nectar/database/asset_database.h>
#include <nectar/hive/hive_document.h>
#include <nectar/pipeline/import_context.h>
#include <nectar/pipeline/import_pipeline.h>
#include <nectar/pipeline/importer_registry.h>
#include <nectar/vfs/virtual_filesystem.h>

namespace nectar
{
    ImportPipeline::ImportPipeline(comb::DefaultAllocator& alloc, ImporterRegistry& registry, CasStore& cas,
                                   VirtualFilesystem& vfs, AssetDatabase& db)
        : m_alloc{&alloc}
        , m_registry{&registry}
        , m_cas{&cas}
        , m_vfs{&vfs}
        , m_db{&db}
    {
    }

    ImportOutput ImportPipeline::ImportAsset(const ImportRequest& request)
    {
        HiveDocument emptySettings{*m_alloc};
        return ImportAsset(request, emptySettings);
    }

    ImportOutput ImportPipeline::ImportAsset(const ImportRequest& request, const HiveDocument& settings)
    {
        HIVE_PROFILE_SCOPE_N("ImportPipeline::ImportAsset");
        ImportOutput output{};
        output.m_errorMessage = wax::String{*m_alloc};
        output.m_dependencies = wax::Vector<DependencyEdge>{*m_alloc};

        IAssetImporter* importer = m_registry->FindByPath(request.m_sourcePath);
        if (!importer)
        {
            output.m_errorMessage.Append("No importer for path: ");
            output.m_errorMessage.Append(request.m_sourcePath.Data(), request.m_sourcePath.Size());
            return output;
        }

        auto sourceData = m_vfs->ReadSync(request.m_sourcePath);
        if (sourceData.Size() == 0)
        {
            output.m_errorMessage.Append("Source file not found or empty: ");
            output.m_errorMessage.Append(request.m_sourcePath.Data(), request.m_sourcePath.Size());
            return output;
        }

        ContentHash sourceHash = ContentHash::FromData(sourceData.View());

        ImportContext ctx{*m_alloc, *m_db, request.m_assetId};
        auto result = importer->Import(sourceData.View(), settings, ctx);
        if (!result.m_success)
        {
            output.m_errorMessage = static_cast<wax::String&&>(result.m_errorMessage);
            return output;
        }

        ContentHash stored = m_cas->Store(result.m_intermediateData.View());
        if (!stored.IsValid())
        {
            output.m_errorMessage.Append("Failed to store blob in CAS");
            return output;
        }

        auto* existing = m_db->FindByUuid(request.m_assetId);
        if (existing)
        {
            existing->m_contentHash = sourceHash;
            existing->m_intermediateHash = stored;
            existing->m_importVersion = importer->Version();
            existing->m_type = wax::String{*m_alloc};
            existing->m_type.Append(importer->TypeName().Data(), importer->TypeName().Size());
        }
        else
        {
            AssetRecord record{};
            record.m_uuid = request.m_assetId;
            record.m_path = wax::String{*m_alloc};
            record.m_path.Append(request.m_sourcePath.Data(), request.m_sourcePath.Size());
            record.m_type = wax::String{*m_alloc};
            record.m_type.Append(importer->TypeName().Data(), importer->TypeName().Size());
            record.m_name = wax::String{*m_alloc};
            record.m_contentHash = sourceHash;
            record.m_intermediateHash = stored;
            record.m_importVersion = importer->Version();
            record.m_labels = wax::Vector<wax::String>{*m_alloc};
            m_db->Insert(static_cast<AssetRecord&&>(record));
        }

        auto& deps = ctx.GetDeclaredDeps();
        auto& graph = m_db->GetDependencyGraph();
        for (size_t i = 0; i < deps.Size(); ++i)
        {
            graph.AddEdge(deps[i].m_from, deps[i].m_to, deps[i].m_kind);
            output.m_dependencies.PushBack(deps[i]);
        }

        output.m_success = true;
        output.m_contentHash = stored;
        output.m_importVersion = importer->Version();
        return output;
    }

    void ImportPipeline::ScanOutdated(wax::Vector<AssetId>& out) const
    {
        HIVE_PROFILE_SCOPE_N("ImportPipeline::ScanOutdated");
        m_db->ForEach([&](AssetId id, const AssetRecord&) {
            if (NeedsReimport(id))
            {
                out.PushBack(id);
            }
        });
    }

    size_t ImportPipeline::ReimportOutdated(const wax::Vector<AssetId>& assets)
    {
        HIVE_PROFILE_SCOPE_N("ImportPipeline::ReimportOutdated");
        size_t count = 0;
        for (size_t i = 0; i < assets.Size(); ++i)
        {
            auto* record = m_db->FindByUuid(assets[i]);
            if (!record)
            {
                continue;
            }

            ImportRequest req;
            req.m_sourcePath = record->m_path.View();
            req.m_assetId = assets[i];
            auto result = ImportAsset(req);
            if (result.m_success)
            {
                ++count;
            }
        }
        return count;
    }

    bool ImportPipeline::NeedsReimport(AssetId id) const
    {
        auto* record = m_db->FindByUuid(id);
        if (!record)
        {
            return true;
        }

        IAssetImporter* importer = m_registry->FindByPath(record->m_path.View());
        if (!importer)
        {
            return false;
        }

        if (record->m_importVersion != importer->Version())
        {
            return true;
        }

        auto sourceData = m_vfs->ReadSync(record->m_path.View());
        if (sourceData.Size() == 0)
        {
            return true;
        }

        ContentHash current = ContentHash::FromData(sourceData.View());
        return !(current == record->m_contentHash);
    }
} // namespace nectar
