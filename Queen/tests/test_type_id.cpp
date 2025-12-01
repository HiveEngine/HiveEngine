#include <larvae/larvae.h>
#include <queen/core/type_id.h>

namespace
{
    struct Position { float x, y, z; };
    struct Velocity { float dx, dy, dz; };
    struct Health { int value; };
    struct Player {};

    auto test1 = larvae::RegisterTest("QueenTypeId", "DifferentTypesHaveDifferentIds", []() {
        constexpr queen::TypeId posId = queen::TypeIdOf<Position>();
        constexpr queen::TypeId velId = queen::TypeIdOf<Velocity>();
        constexpr queen::TypeId healthId = queen::TypeIdOf<Health>();
        constexpr queen::TypeId playerId = queen::TypeIdOf<Player>();

        larvae::AssertNotEqual(posId, velId);
        larvae::AssertNotEqual(posId, healthId);
        larvae::AssertNotEqual(posId, playerId);
        larvae::AssertNotEqual(velId, healthId);
        larvae::AssertNotEqual(velId, playerId);
        larvae::AssertNotEqual(healthId, playerId);
    });

    auto test2 = larvae::RegisterTest("QueenTypeId", "SameTypeHasSameId", []() {
        constexpr queen::TypeId id1 = queen::TypeIdOf<Position>();
        constexpr queen::TypeId id2 = queen::TypeIdOf<Position>();

        larvae::AssertEqual(id1, id2);
    });

    auto test3 = larvae::RegisterTest("QueenTypeId", "TypeIdIsNotZero", []() {
        constexpr queen::TypeId posId = queen::TypeIdOf<Position>();
        constexpr queen::TypeId velId = queen::TypeIdOf<Velocity>();

        larvae::AssertNotEqual(posId, queen::kInvalidTypeId);
        larvae::AssertNotEqual(velId, queen::kInvalidTypeId);
    });

    auto test4 = larvae::RegisterTest("QueenTypeId", "BuiltinTypesWork", []() {
        constexpr queen::TypeId intId = queen::TypeIdOf<int>();
        constexpr queen::TypeId floatId = queen::TypeIdOf<float>();
        constexpr queen::TypeId doubleId = queen::TypeIdOf<double>();

        larvae::AssertNotEqual(intId, floatId);
        larvae::AssertNotEqual(intId, doubleId);
        larvae::AssertNotEqual(floatId, doubleId);
    });

    auto test5 = larvae::RegisterTest("QueenTypeId", "PointersHaveDifferentIds", []() {
        constexpr queen::TypeId posId = queen::TypeIdOf<Position>();
        constexpr queen::TypeId posPtrId = queen::TypeIdOf<Position*>();
        constexpr queen::TypeId posRefId = queen::TypeIdOf<Position&>();

        larvae::AssertNotEqual(posId, posPtrId);
        larvae::AssertNotEqual(posId, posRefId);
        larvae::AssertNotEqual(posPtrId, posRefId);
    });

    auto test6 = larvae::RegisterTest("QueenTypeId", "ConstHasDifferentId", []() {
        constexpr queen::TypeId posId = queen::TypeIdOf<Position>();
        constexpr queen::TypeId constPosId = queen::TypeIdOf<const Position>();

        larvae::AssertNotEqual(posId, constPosId);
    });

    auto test7 = larvae::RegisterTest("QueenTypeId", "TypeNameReturnsValidString", []() {
        constexpr std::string_view name = queen::TypeNameOf<Position>();

        larvae::AssertTrue(name.size() > 0);
        larvae::AssertTrue(name.find("Position") != std::string_view::npos);
    });

    auto test8 = larvae::RegisterTest("QueenTypeId", "CompileTimeEvaluation", []() {
        constexpr queen::TypeId id = queen::TypeIdOf<Position>();
        static_assert(id != 0, "TypeId should be non-zero at compile time");
        static_assert(id == queen::TypeIdOf<Position>(), "TypeId should be stable");

        larvae::AssertTrue(true);
    });
}
