#include <nectar/pipeline/import_pipeline.h>
#include <nectar/pipeline/importer_registry.h>
#include <nectar/pipeline/import_context.h>
#include <nectar/cas/cas_store.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/database/asset_database.h>
#include <nectar/hive/hive_document.h>
#include <hive/profiling/profiler.h>

namespace nectar
{
    ImportPipeline::ImportPipeline(comb::DefaultAllocator& alloc,
                                   ImporterRegistry& registry,
                                   CasStore& cas,
                                   VirtualFilesystem& vfs,
                                   AssetDatabase& db)
        : alloc_{&alloc}
        , registry_{&registry}
        , cas_{&cas}
        , vfs_{&vfs}
        , db_{&db}
    {}

    ImportOutput ImportPipeline::ImportAsset(const ImportRequest& request)
    {
        HiveDocument empty_settings{*alloc_};
        return ImportAsset(request, empty_settings);
    }

    ImportOutput ImportPipeline::ImportAsset(const ImportRequest& request,
                                             const HiveDocument& settings)
    {
        HIVE_PROFILE_SCOPE_N("ImportPipeline::ImportAsset");
        ImportOutput output{};
        output.error_message = wax::String<>{*alloc_};
        output.dependencies = wax::Vector<DependencyEdge>{*alloc_};

        // 1. Find importer
        IAssetImporter* importer = registry_->FindByPath(request.source_path);
        if (!importer)
        {
            output.error_message.Append("No importer for path: ");
            output.error_message.Append(request.source_path.Data(), request.source_path.Size());
            return output;
        }

        // 2. Read source from VFS
        auto source_data = vfs_->ReadSync(request.source_path);
        if (source_data.Size() == 0)
        {
            output.error_message.Append("Source file not found or empty: ");
            output.error_message.Append(request.source_path.Data(), request.source_path.Size());
            return output;
        }

        // 3. Hash the source (for change detection in NeedsReimport)
        ContentHash source_hash = ContentHash::FromData(source_data.View());

        // 4. Run import
        ImportContext ctx{*alloc_, *db_, request.asset_id};
        auto result = importer->Import(source_data.View(), settings, ctx);
        if (!result.success)
        {
            output.error_message = static_cast<wax::String<>&&>(result.error_message);
            return output;
        }

        // 5. Hash intermediate data (CAS key)
        ContentHash cas_hash = ContentHash::FromData(result.intermediate_data.View());

        // 6. Store in CAS
        ContentHash stored = cas_->Store(result.intermediate_data.View());
        if (!stored.IsValid())
        {
            output.error_message.Append("Failed to store blob in CAS");
            return output;
        }

        // 7. Update/insert database record
        //    content_hash = source hash (for change detection)
        auto* existing = db_->FindByUuid(request.asset_id);
        if (existing)
        {
            existing->content_hash = source_hash;
            existing->intermediate_hash = cas_hash;
            existing->import_version = importer->Version();
            existing->type = wax::String<>{*alloc_};
            existing->type.Append(importer->TypeName().Data(), importer->TypeName().Size());
        }
        else
        {
            AssetRecord record{};
            record.uuid = request.asset_id;
            record.path = wax::String<>{*alloc_};
            record.path.Append(request.source_path.Data(), request.source_path.Size());
            record.type = wax::String<>{*alloc_};
            record.type.Append(importer->TypeName().Data(), importer->TypeName().Size());
            record.name = wax::String<>{*alloc_};
            record.content_hash = source_hash;
            record.intermediate_hash = cas_hash;
            record.import_version = importer->Version();
            record.labels = wax::Vector<wax::String<>>{*alloc_};
            db_->Insert(static_cast<AssetRecord&&>(record));
        }

        // 8. Update dependency graph
        auto& deps = ctx.GetDeclaredDeps();
        auto& graph = db_->GetDependencyGraph();
        for (size_t i = 0; i < deps.Size(); ++i)
        {
            graph.AddEdge(deps[i].from, deps[i].to, deps[i].kind);
            output.dependencies.PushBack(deps[i]);
        }

        // 9. Fill output (content_hash = CAS hash of intermediate)
        output.success = true;
        output.content_hash = cas_hash;
        output.import_version = importer->Version();
        return output;
    }

    void ImportPipeline::ScanOutdated(wax::Vector<AssetId>& out) const
    {
        HIVE_PROFILE_SCOPE_N("ImportPipeline::ScanOutdated");
        db_->ForEach([&](AssetId id, const AssetRecord&) {
            if (NeedsReimport(id))
                out.PushBack(id);
        });
    }

    size_t ImportPipeline::ReimportOutdated(const wax::Vector<AssetId>& assets)
    {
        HIVE_PROFILE_SCOPE_N("ImportPipeline::ReimportOutdated");
        size_t count = 0;
        for (size_t i = 0; i < assets.Size(); ++i)
        {
            auto* record = db_->FindByUuid(assets[i]);
            if (!record) continue;

            ImportRequest req;
            req.source_path = record->path.View();
            req.asset_id = assets[i];
            auto result = ImportAsset(req);
            if (result.success)
                ++count;
        }
        return count;
    }

    bool ImportPipeline::NeedsReimport(AssetId id) const
    {
        auto* record = db_->FindByUuid(id);
        if (!record)
            return true;

        // Find importer for this asset's path
        IAssetImporter* importer = registry_->FindByPath(record->path.View());
        if (!importer)
            return false;  // no importer = can't reimport

        // Version mismatch
        if (record->import_version != importer->Version())
            return true;

        // Content hash mismatch
        auto source_data = vfs_->ReadSync(record->path.View());
        if (source_data.Size() == 0)
            return true;  // source missing = needs reimport

        ContentHash current = ContentHash::FromData(source_data.View());
        return !(current == record->content_hash);
    }
}
