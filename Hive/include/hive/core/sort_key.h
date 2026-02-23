#pragma once

#include <hive/math/functions.h>
#include <cstdint>

namespace hive::sort_key
{

// 64-bit draw call sort key layout (MSB = highest priority):
//
//   Bits 63-60  Layer       (4 bits)   Game, Effects, HUD, Debug
//   Bits 59-56  Pass        (4 bits)   shadow, z-prepass, gbuffer, forward
//   Bit  55     Translucent (1 bit)    0 = opaque, 1 = transparent
//   Bits 54-40  Pipeline ID (15 bits)
//   Bits 39-24  Material ID (16 bits)
//   Bits 23-0   Depth       (24 bits)

constexpr uint32_t kDepthBits    = 24;
constexpr uint32_t kMaterialBits = 16;
constexpr uint32_t kPipelineBits = 15;
constexpr uint32_t kTranslucentBit = 55;
constexpr uint32_t kPassShift    = 56;
constexpr uint32_t kLayerShift   = 60;
constexpr uint32_t kDepthMax     = (1u << kDepthBits) - 1;

// Quantize a distance to 24-bit unsigned integer.
[[nodiscard]] inline uint32_t QuantizeDepth(float distance, float z_near, float z_far)
{
    float t = (distance - z_near) / (z_far - z_near);
    t = math::Clamp(0.f, t, 1.f);
    return static_cast<uint32_t>(t * static_cast<float>(kDepthMax));
}

// Opaque sort key: front-to-back (ascending depth for early-Z)
[[nodiscard]] inline uint64_t EncodeOpaque(
    uint8_t layer, uint8_t pass, uint16_t pipeline_id,
    uint16_t material_id, uint32_t depth_24)
{
    uint64_t key = 0;
    key |= static_cast<uint64_t>(layer & 0xF)          << kLayerShift;
    key |= static_cast<uint64_t>(pass & 0xF)           << kPassShift;
    key |= static_cast<uint64_t>(pipeline_id & 0x7FFF) << 40;
    key |= static_cast<uint64_t>(material_id)          << kDepthBits;
    key |= static_cast<uint64_t>(depth_24 & kDepthMax);
    return key;
}

// Transparent sort key: back-to-front (inverted depth for correct blending)
[[nodiscard]] inline uint64_t EncodeTransparent(
    uint8_t layer, uint8_t pass, uint16_t pipeline_id,
    uint16_t material_id, uint32_t depth_24)
{
    uint64_t key = 0;
    key |= static_cast<uint64_t>(layer & 0xF)          << kLayerShift;
    key |= static_cast<uint64_t>(pass & 0xF)           << kPassShift;
    key |= uint64_t{1}                                  << kTranslucentBit;
    key |= static_cast<uint64_t>(pipeline_id & 0x7FFF) << 40;
    key |= static_cast<uint64_t>(material_id)          << kDepthBits;
    key |= static_cast<uint64_t>((kDepthMax - depth_24) & kDepthMax);
    return key;
}

// Extract fields (for debug / tests)
[[nodiscard]] inline uint8_t  ExtractLayer(uint64_t key)       { return static_cast<uint8_t>((key >> kLayerShift) & 0xF); }
[[nodiscard]] inline uint8_t  ExtractPass(uint64_t key)        { return static_cast<uint8_t>((key >> kPassShift) & 0xF); }
[[nodiscard]] inline bool     ExtractTranslucent(uint64_t key) { return ((key >> kTranslucentBit) & 1) != 0; }
[[nodiscard]] inline uint16_t ExtractPipeline(uint64_t key)    { return static_cast<uint16_t>((key >> 40) & 0x7FFF); }
[[nodiscard]] inline uint16_t ExtractMaterial(uint64_t key)    { return static_cast<uint16_t>((key >> kDepthBits) & 0xFFFF); }
[[nodiscard]] inline uint32_t ExtractDepth(uint64_t key)       { return static_cast<uint32_t>(key & kDepthMax); }

} // namespace hive::sort_key
