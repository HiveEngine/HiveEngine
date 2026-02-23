#include <larvae/larvae.h>
#include <hive/core/sort_key.h>

namespace {

    using namespace hive::sort_key;

    // =========================================================================
    // QuantizeDepth
    // =========================================================================

    auto t_quantize_at_near = larvae::RegisterTest("SortKey", "QuantizeAtNear", []() {
        larvae::AssertEqual(QuantizeDepth(0.1f, 0.1f, 100.f), 0u);
    });

    auto t_quantize_at_far = larvae::RegisterTest("SortKey", "QuantizeAtFar", []() {
        larvae::AssertEqual(QuantizeDepth(100.f, 0.1f, 100.f), kDepthMax);
    });

    auto t_quantize_midpoint = larvae::RegisterTest("SortKey", "QuantizeMidpoint", []() {
        uint32_t mid = QuantizeDepth(50.05f, 0.1f, 100.f);
        larvae::AssertTrue(mid > kDepthMax / 2 - 1000);
        larvae::AssertTrue(mid < kDepthMax / 2 + 1000);
    });

    auto t_quantize_clamps_below = larvae::RegisterTest("SortKey", "QuantizeClampsBelow", []() {
        larvae::AssertEqual(QuantizeDepth(-10.f, 0.1f, 100.f), 0u);
    });

    auto t_quantize_clamps_above = larvae::RegisterTest("SortKey", "QuantizeClampsAbove", []() {
        larvae::AssertEqual(QuantizeDepth(500.f, 0.1f, 100.f), kDepthMax);
    });

    // =========================================================================
    // Encode / Extract round-trip
    // =========================================================================

    auto t_encode_opaque_roundtrip = larvae::RegisterTest("SortKey", "EncodeOpaqueRoundtrip", []() {
        uint64_t key = EncodeOpaque(3, 2, 1234, 5678, 123456);
        larvae::AssertEqual(ExtractLayer(key), uint8_t{3});
        larvae::AssertEqual(ExtractPass(key), uint8_t{2});
        larvae::AssertTrue(!ExtractTranslucent(key));
        larvae::AssertEqual(ExtractPipeline(key), uint16_t{1234});
        larvae::AssertEqual(ExtractMaterial(key), uint16_t{5678});
        larvae::AssertEqual(ExtractDepth(key), 123456u);
    });

    auto t_encode_transparent_roundtrip = larvae::RegisterTest("SortKey", "EncodeTransparentRoundtrip", []() {
        uint64_t key = EncodeTransparent(1, 3, 100, 200, 1000);
        larvae::AssertEqual(ExtractLayer(key), uint8_t{1});
        larvae::AssertEqual(ExtractPass(key), uint8_t{3});
        larvae::AssertTrue(ExtractTranslucent(key));
        larvae::AssertEqual(ExtractPipeline(key), uint16_t{100});
        larvae::AssertEqual(ExtractMaterial(key), uint16_t{200});
        larvae::AssertEqual(ExtractDepth(key), kDepthMax - 1000);
    });

    // =========================================================================
    // Ordering
    // =========================================================================

    auto t_opaque_front_to_back = larvae::RegisterTest("SortKey", "OpaqueFrontToBack", []() {
        uint64_t near_key = EncodeOpaque(0, 0, 0, 0, 100);
        uint64_t far_key  = EncodeOpaque(0, 0, 0, 0, 5000);
        larvae::AssertTrue(near_key < far_key);
    });

    auto t_transparent_back_to_front = larvae::RegisterTest("SortKey", "TransparentBackToFront", []() {
        uint64_t near_key = EncodeTransparent(0, 0, 0, 0, 100);
        uint64_t far_key  = EncodeTransparent(0, 0, 0, 0, 5000);
        larvae::AssertTrue(far_key < near_key);
    });

    auto t_layer_ordering = larvae::RegisterTest("SortKey", "LayerOrdering", []() {
        uint64_t layer0 = EncodeOpaque(0, 0, 0, 0, 0);
        uint64_t layer1 = EncodeOpaque(1, 0, 0, 0, 0);
        larvae::AssertTrue(layer0 < layer1);
    });

    auto t_opaque_before_transparent = larvae::RegisterTest("SortKey", "OpaqueBeforeTransparent", []() {
        uint64_t opaque = EncodeOpaque(0, 0, 0, 0, 0);
        uint64_t transparent = EncodeTransparent(0, 0, 0, 0, 0);
        larvae::AssertTrue(opaque < transparent);
    });

    auto t_material_grouping = larvae::RegisterTest("SortKey", "MaterialGrouping", []() {
        uint64_t mat0 = EncodeOpaque(0, 0, 0, 0, 500);
        uint64_t mat1 = EncodeOpaque(0, 0, 0, 1, 100);
        larvae::AssertTrue(mat0 < mat1);
    });

    auto t_pipeline_before_material = larvae::RegisterTest("SortKey", "PipelineBeforeMaterial", []() {
        uint64_t pipe0_mat5 = EncodeOpaque(0, 0, 0, 5, 0);
        uint64_t pipe1_mat0 = EncodeOpaque(0, 0, 1, 0, 0);
        larvae::AssertTrue(pipe0_mat5 < pipe1_mat0);
    });

} // namespace
