#include <queen/core/component_info.h>

#include <larvae/larvae.h>

namespace
{
    struct Position
    {
        float x, y, z;
    };

    struct Velocity
    {
        float dx, dy, dz;
    };

    struct Player
    {
    };

    struct SparseComponent
    {
        int value;
        static constexpr queen::StorageType storage = queen::StorageType::SPARSE;
    };

    struct NonTrivial
    {
        int* data;

        NonTrivial()
            : data{nullptr} {}
        ~NonTrivial() { data = nullptr; }
        NonTrivial(const NonTrivial& other)
            : data{other.data} {}
        NonTrivial(NonTrivial&& other) noexcept
            : data{other.data} {
            other.data = nullptr;
        }
    };

    auto test1 = larvae::RegisterTest("QueenComponentInfo", "StaticTypeInfo", []() {
        using Info = queen::ComponentInfo<Position>;

        larvae::AssertEqual(Info::id, queen::TypeIdOf<Position>());
        larvae::AssertEqual(Info::size, sizeof(Position));
        larvae::AssertEqual(Info::alignment, alignof(Position));
        larvae::AssertTrue(Info::isTriviallyCopyable);
        larvae::AssertTrue(Info::isTriviallyDestructible);
        larvae::AssertEqual(static_cast<int>(Info::storage), static_cast<int>(queen::StorageType::DENSE));
    });

    auto test2 = larvae::RegisterTest("QueenComponentInfo", "ConstructFunction", []() {
        alignas(Position) char buffer[sizeof(Position)];
        queen::ComponentInfo<Position>::Construct(buffer);

        Position* pos = reinterpret_cast<Position*>(buffer);
        larvae::AssertEqual(pos->x, 0.0f);
        larvae::AssertEqual(pos->y, 0.0f);
        larvae::AssertEqual(pos->z, 0.0f);
    });

    auto test3 = larvae::RegisterTest("QueenComponentInfo", "MoveFunction", []() {
        Position src{1.0f, 2.0f, 3.0f};
        alignas(Position) char dst_buffer[sizeof(Position)];

        queen::ComponentInfo<Position>::Move(dst_buffer, &src);

        Position* dst = reinterpret_cast<Position*>(dst_buffer);
        larvae::AssertEqual(dst->x, 1.0f);
        larvae::AssertEqual(dst->y, 2.0f);
        larvae::AssertEqual(dst->z, 3.0f);
    });

    auto test4 = larvae::RegisterTest("QueenComponentInfo", "CopyFunction", []() {
        Position src{1.0f, 2.0f, 3.0f};
        alignas(Position) char dst_buffer[sizeof(Position)];

        queen::ComponentInfo<Position>::Copy(dst_buffer, &src);

        Position* dst = reinterpret_cast<Position*>(dst_buffer);
        larvae::AssertEqual(dst->x, 1.0f);
        larvae::AssertEqual(dst->y, 2.0f);
        larvae::AssertEqual(dst->z, 3.0f);
    });

    auto test5 = larvae::RegisterTest("QueenComponentInfo", "SparseStorageHint", []() {
        using Info = queen::ComponentInfo<SparseComponent>;

        larvae::AssertEqual(static_cast<int>(Info::storage), static_cast<int>(queen::StorageType::SPARSE));
    });

    auto test6 = larvae::RegisterTest("QueenComponentInfo", "NonTrivialType", []() {
        using Info = queen::ComponentInfo<NonTrivial>;

        larvae::AssertFalse(Info::isTriviallyCopyable);
        larvae::AssertFalse(Info::isTriviallyDestructible);
    });

    auto test7 = larvae::RegisterTest("QueenComponentMeta", "BasicTypeInfo", []() {
        queen::ComponentMeta meta = queen::ComponentMeta::Of<Position>();

        larvae::AssertTrue(meta.IsValid());
        larvae::AssertEqual(meta.m_size, sizeof(Position));
        larvae::AssertEqual(meta.m_alignment, alignof(Position));
        larvae::AssertEqual(meta.m_typeId, queen::TypeIdOf<Position>());
    });

    auto test8 = larvae::RegisterTest("QueenComponentMeta", "TrivialTypeHasNoDestructor", []() {
        queen::ComponentMeta meta = queen::ComponentMeta::Of<Position>();

        larvae::AssertTrue(meta.IsTrivial());
        larvae::AssertNull(meta.m_destruct);
    });

    auto test9 = larvae::RegisterTest("QueenComponentMeta", "NonTrivialTypeHasDestructor", []() {
        queen::ComponentMeta meta = queen::ComponentMeta::Of<NonTrivial>();

        larvae::AssertFalse(meta.IsTrivial());
        larvae::AssertNotNull(meta.m_destruct);
    });

    auto test10 = larvae::RegisterTest("QueenComponentMeta", "ConstructFunction", []() {
        queen::ComponentMeta meta = queen::ComponentMeta::Of<Position>();

        larvae::AssertNotNull(meta.m_construct);

        alignas(Position) char buffer[sizeof(Position)];
        meta.m_construct(buffer);

        Position* pos = reinterpret_cast<Position*>(buffer);
        larvae::AssertEqual(pos->x, 0.0f);
        larvae::AssertEqual(pos->y, 0.0f);
        larvae::AssertEqual(pos->z, 0.0f);
    });

    auto test11 = larvae::RegisterTest("QueenComponentMeta", "TagComponent", []() {
        queen::ComponentMeta meta = queen::ComponentMeta::OfTag<Player>();

        larvae::AssertFalse(meta.IsValid());
        larvae::AssertEqual(meta.m_size, size_t{0});
        larvae::AssertEqual(meta.m_alignment, size_t{1});
        larvae::AssertEqual(meta.m_typeId, queen::TypeIdOf<Player>());
        larvae::AssertNull(meta.m_construct);
        larvae::AssertNull(meta.m_destruct);
        larvae::AssertNull(meta.m_move);
        larvae::AssertNull(meta.m_copy);
    });

    auto test12 = larvae::RegisterTest("QueenComponentMeta", "DifferentTypesHaveDifferentIds", []() {
        queen::ComponentMeta pos_meta = queen::ComponentMeta::Of<Position>();
        queen::ComponentMeta vel_meta = queen::ComponentMeta::Of<Velocity>();

        larvae::AssertNotEqual(pos_meta.m_typeId, vel_meta.m_typeId);
    });

    auto test13 = larvae::RegisterTest("QueenComponentMeta", "InvalidInfoByDefault", []() {
        queen::ComponentMeta meta{};

        larvae::AssertFalse(meta.IsValid());
        larvae::AssertEqual(meta.m_typeId, queen::TypeId{0});
        larvae::AssertEqual(meta.m_size, size_t{0});
    });
} // namespace
