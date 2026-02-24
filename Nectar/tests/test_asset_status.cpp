#include <larvae/larvae.h>
#include <nectar/core/asset_status.h>

namespace {

    auto t1 = larvae::RegisterTest("NectarAssetStatus", "StatusValuesAreDistinct", []() {
        larvae::AssertNotEqual(
            static_cast<uint8_t>(nectar::AssetStatus::NotLoaded),
            static_cast<uint8_t>(nectar::AssetStatus::Loading));
        larvae::AssertNotEqual(
            static_cast<uint8_t>(nectar::AssetStatus::Loading),
            static_cast<uint8_t>(nectar::AssetStatus::Ready));
        larvae::AssertNotEqual(
            static_cast<uint8_t>(nectar::AssetStatus::Ready),
            static_cast<uint8_t>(nectar::AssetStatus::Failed));
        larvae::AssertNotEqual(
            static_cast<uint8_t>(nectar::AssetStatus::Failed),
            static_cast<uint8_t>(nectar::AssetStatus::Unloaded));
    });

    auto t2 = larvae::RegisterTest("NectarAssetStatus", "DefaultErrorInfoIsNone", []() {
        nectar::AssetErrorInfo info{};
        larvae::AssertEqual(
            static_cast<uint8_t>(info.code),
            static_cast<uint8_t>(nectar::AssetError::None));
    });

    auto t3 = larvae::RegisterTest("NectarAssetStatus", "ErrorInfoWithCode", []() {
        nectar::AssetErrorInfo info{nectar::AssetError::FileNotFound, wax::String<>{}};
        larvae::AssertEqual(
            static_cast<uint8_t>(info.code),
            static_cast<uint8_t>(nectar::AssetError::FileNotFound));
    });

}
