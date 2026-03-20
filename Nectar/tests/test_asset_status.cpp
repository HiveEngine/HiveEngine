#include <nectar/core/asset_status.h>

#include <larvae/larvae.h>

namespace
{

    auto t1 = larvae::RegisterTest("NectarAssetStatus", "StatusValuesAreDistinct", []() {
        larvae::AssertNotEqual(static_cast<uint8_t>(nectar::AssetStatus::NOT_LOADED),
                               static_cast<uint8_t>(nectar::AssetStatus::LOADING));
        larvae::AssertNotEqual(static_cast<uint8_t>(nectar::AssetStatus::LOADING),
                               static_cast<uint8_t>(nectar::AssetStatus::READY));
        larvae::AssertNotEqual(static_cast<uint8_t>(nectar::AssetStatus::READY),
                               static_cast<uint8_t>(nectar::AssetStatus::FAILED));
        larvae::AssertNotEqual(static_cast<uint8_t>(nectar::AssetStatus::FAILED),
                               static_cast<uint8_t>(nectar::AssetStatus::UNLOADED));
    });

    auto t2 = larvae::RegisterTest("NectarAssetStatus", "DefaultErrorInfoIsNone", []() {
        nectar::AssetErrorInfo info{};
        larvae::AssertEqual(static_cast<uint8_t>(info.m_code), static_cast<uint8_t>(nectar::AssetError::NONE));
    });

    auto t3 = larvae::RegisterTest("NectarAssetStatus", "ErrorInfoWithCode", []() {
        nectar::AssetErrorInfo info{nectar::AssetError::FILE_NOT_FOUND, wax::String{}};
        larvae::AssertEqual(static_cast<uint8_t>(info.m_code),
                            static_cast<uint8_t>(nectar::AssetError::FILE_NOT_FOUND));
    });

} // namespace
