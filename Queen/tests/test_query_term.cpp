#include <larvae/larvae.h>
#include <queen/query/query_term.h>

namespace
{
    struct Position { float x, y, z; };
    struct Velocity { float dx, dy, dz; };
    struct Health { int current, max; };
    struct Player {};
    struct Dead {};

    // ─────────────────────────────────────────────────────────────
    // Term struct tests
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenQueryTerm", "TermCreate", []() {
        queen::Term term = queen::Term::Create<Position>();

        larvae::AssertTrue(term.IsValid());
        larvae::AssertEqual(term.type_id, queen::TypeIdOf<Position>());
        larvae::AssertEqual(static_cast<int>(term.op), static_cast<int>(queen::TermOperator::With));
        larvae::AssertEqual(static_cast<int>(term.access), static_cast<int>(queen::TermAccess::Read));
    });

    auto test2 = larvae::RegisterTest("QueenQueryTerm", "TermCreateWithOptions", []() {
        queen::Term term = queen::Term::Create<Velocity>(
            queen::TermOperator::Without,
            queen::TermAccess::None
        );

        larvae::AssertTrue(term.IsValid());
        larvae::AssertEqual(term.type_id, queen::TypeIdOf<Velocity>());
        larvae::AssertTrue(term.IsExcluded());
        larvae::AssertFalse(term.HasDataAccess());
    });

    auto test3 = larvae::RegisterTest("QueenQueryTerm", "TermIsRequired", []() {
        queen::Term with = queen::Term::Create<Position>(queen::TermOperator::With);
        queen::Term without = queen::Term::Create<Position>(queen::TermOperator::Without);
        queen::Term optional = queen::Term::Create<Position>(queen::TermOperator::Optional);

        larvae::AssertTrue(with.IsRequired());
        larvae::AssertFalse(without.IsRequired());
        larvae::AssertFalse(optional.IsRequired());
    });

    auto test4 = larvae::RegisterTest("QueenQueryTerm", "TermIsExcluded", []() {
        queen::Term with = queen::Term::Create<Position>(queen::TermOperator::With);
        queen::Term without = queen::Term::Create<Position>(queen::TermOperator::Without);

        larvae::AssertFalse(with.IsExcluded());
        larvae::AssertTrue(without.IsExcluded());
    });

    auto test5 = larvae::RegisterTest("QueenQueryTerm", "TermIsOptional", []() {
        queen::Term with = queen::Term::Create<Position>(queen::TermOperator::With);
        queen::Term optional = queen::Term::Create<Position>(queen::TermOperator::Optional);

        larvae::AssertFalse(with.IsOptional());
        larvae::AssertTrue(optional.IsOptional());
    });

    auto test6 = larvae::RegisterTest("QueenQueryTerm", "TermAccessModes", []() {
        queen::Term read = queen::Term::Create<Position>(queen::TermOperator::With, queen::TermAccess::Read);
        queen::Term write = queen::Term::Create<Position>(queen::TermOperator::With, queen::TermAccess::Write);
        queen::Term none = queen::Term::Create<Position>(queen::TermOperator::With, queen::TermAccess::None);

        larvae::AssertTrue(read.IsReadOnly());
        larvae::AssertFalse(read.IsWritable());
        larvae::AssertTrue(read.HasDataAccess());

        larvae::AssertFalse(write.IsReadOnly());
        larvae::AssertTrue(write.IsWritable());
        larvae::AssertTrue(write.HasDataAccess());

        larvae::AssertFalse(none.IsReadOnly());
        larvae::AssertFalse(none.IsWritable());
        larvae::AssertFalse(none.HasDataAccess());
    });

    // ─────────────────────────────────────────────────────────────
    // Read<T> wrapper tests
    // ─────────────────────────────────────────────────────────────

    auto test7 = larvae::RegisterTest("QueenQueryTerm", "ReadWrapper", []() {
        using ReadPos = queen::Read<Position>;

        larvae::AssertEqual(ReadPos::type_id, queen::TypeIdOf<Position>());
        larvae::AssertEqual(static_cast<int>(ReadPos::op), static_cast<int>(queen::TermOperator::With));
        larvae::AssertEqual(static_cast<int>(ReadPos::access), static_cast<int>(queen::TermAccess::Read));

        queen::Term term = ReadPos::ToTerm();
        larvae::AssertTrue(term.IsValid());
        larvae::AssertTrue(term.IsRequired());
        larvae::AssertTrue(term.IsReadOnly());
    });

    // ─────────────────────────────────────────────────────────────
    // Write<T> wrapper tests
    // ─────────────────────────────────────────────────────────────

    auto test8 = larvae::RegisterTest("QueenQueryTerm", "WriteWrapper", []() {
        using WriteVel = queen::Write<Velocity>;

        larvae::AssertEqual(WriteVel::type_id, queen::TypeIdOf<Velocity>());
        larvae::AssertEqual(static_cast<int>(WriteVel::op), static_cast<int>(queen::TermOperator::With));
        larvae::AssertEqual(static_cast<int>(WriteVel::access), static_cast<int>(queen::TermAccess::Write));

        queen::Term term = WriteVel::ToTerm();
        larvae::AssertTrue(term.IsWritable());
        larvae::AssertTrue(term.HasDataAccess());
    });

    // ─────────────────────────────────────────────────────────────
    // With<T> wrapper tests
    // ─────────────────────────────────────────────────────────────

    auto test9 = larvae::RegisterTest("QueenQueryTerm", "WithWrapper", []() {
        using WithPlayer = queen::With<Player>;

        larvae::AssertEqual(WithPlayer::type_id, queen::TypeIdOf<Player>());
        larvae::AssertEqual(static_cast<int>(WithPlayer::op), static_cast<int>(queen::TermOperator::With));
        larvae::AssertEqual(static_cast<int>(WithPlayer::access), static_cast<int>(queen::TermAccess::None));

        queen::Term term = WithPlayer::ToTerm();
        larvae::AssertTrue(term.IsRequired());
        larvae::AssertFalse(term.HasDataAccess());
    });

    // ─────────────────────────────────────────────────────────────
    // Without<T> wrapper tests
    // ─────────────────────────────────────────────────────────────

    auto test10 = larvae::RegisterTest("QueenQueryTerm", "WithoutWrapper", []() {
        using WithoutDead = queen::Without<Dead>;

        larvae::AssertEqual(WithoutDead::type_id, queen::TypeIdOf<Dead>());
        larvae::AssertEqual(static_cast<int>(WithoutDead::op), static_cast<int>(queen::TermOperator::Without));
        larvae::AssertEqual(static_cast<int>(WithoutDead::access), static_cast<int>(queen::TermAccess::None));

        queen::Term term = WithoutDead::ToTerm();
        larvae::AssertTrue(term.IsExcluded());
        larvae::AssertFalse(term.HasDataAccess());
    });

    // ─────────────────────────────────────────────────────────────
    // Maybe<T> wrapper tests
    // ─────────────────────────────────────────────────────────────

    auto test11 = larvae::RegisterTest("QueenQueryTerm", "MaybeWrapper", []() {
        using MaybeHealth = queen::Maybe<Health>;

        larvae::AssertEqual(MaybeHealth::type_id, queen::TypeIdOf<Health>());
        larvae::AssertEqual(static_cast<int>(MaybeHealth::op), static_cast<int>(queen::TermOperator::Optional));
        larvae::AssertEqual(static_cast<int>(MaybeHealth::access), static_cast<int>(queen::TermAccess::Read));

        queen::Term term = MaybeHealth::ToTerm();
        larvae::AssertTrue(term.IsOptional());
        larvae::AssertTrue(term.IsReadOnly());
        larvae::AssertTrue(term.HasDataAccess());
    });

    auto test12 = larvae::RegisterTest("QueenQueryTerm", "MaybeWriteWrapper", []() {
        using MaybeWriteHealth = queen::MaybeWrite<Health>;

        larvae::AssertEqual(MaybeWriteHealth::type_id, queen::TypeIdOf<Health>());
        larvae::AssertEqual(static_cast<int>(MaybeWriteHealth::op), static_cast<int>(queen::TermOperator::Optional));
        larvae::AssertEqual(static_cast<int>(MaybeWriteHealth::access), static_cast<int>(queen::TermAccess::Write));

        queen::Term term = MaybeWriteHealth::ToTerm();
        larvae::AssertTrue(term.IsOptional());
        larvae::AssertTrue(term.IsWritable());
    });

    // ─────────────────────────────────────────────────────────────
    // Type traits tests
    // ─────────────────────────────────────────────────────────────

    auto test13 = larvae::RegisterTest("QueenQueryTerm", "IsQueryTermTrait", []() {
        larvae::AssertTrue(queen::detail::IsQueryTermV<queen::Read<Position>>);
        larvae::AssertTrue(queen::detail::IsQueryTermV<queen::Write<Velocity>>);
        larvae::AssertTrue(queen::detail::IsQueryTermV<queen::With<Player>>);
        larvae::AssertTrue(queen::detail::IsQueryTermV<queen::Without<Dead>>);
        larvae::AssertTrue(queen::detail::IsQueryTermV<queen::Maybe<Health>>);
        larvae::AssertTrue(queen::detail::IsQueryTermV<queen::MaybeWrite<Health>>);

        larvae::AssertFalse(queen::detail::IsQueryTermV<Position>);
        larvae::AssertFalse(queen::detail::IsQueryTermV<int>);
    });

    auto test14 = larvae::RegisterTest("QueenQueryTerm", "HasDataAccessTrait", []() {
        larvae::AssertTrue(queen::detail::HasDataAccessV<queen::Read<Position>>);
        larvae::AssertTrue(queen::detail::HasDataAccessV<queen::Write<Velocity>>);
        larvae::AssertTrue(queen::detail::HasDataAccessV<queen::Maybe<Health>>);

        larvae::AssertFalse(queen::detail::HasDataAccessV<queen::With<Player>>);
        larvae::AssertFalse(queen::detail::HasDataAccessV<queen::Without<Dead>>);
    });

    auto test15 = larvae::RegisterTest("QueenQueryTerm", "IsOptionalTermTrait", []() {
        larvae::AssertTrue(queen::detail::IsOptionalTermV<queen::Maybe<Health>>);
        larvae::AssertTrue(queen::detail::IsOptionalTermV<queen::MaybeWrite<Health>>);

        larvae::AssertFalse(queen::detail::IsOptionalTermV<queen::Read<Position>>);
        larvae::AssertFalse(queen::detail::IsOptionalTermV<queen::Write<Velocity>>);
        larvae::AssertFalse(queen::detail::IsOptionalTermV<queen::With<Player>>);
        larvae::AssertFalse(queen::detail::IsOptionalTermV<queen::Without<Dead>>);
    });

    // ─────────────────────────────────────────────────────────────
    // Constexpr tests
    // ─────────────────────────────────────────────────────────────

    auto test16 = larvae::RegisterTest("QueenQueryTerm", "ConstexprTermCreation", []() {
        constexpr queen::Term term = queen::Term::Create<Position>();

        static_assert(term.IsValid(), "Term should be valid");
        static_assert(term.IsRequired(), "Term should be required");
        static_assert(term.IsReadOnly(), "Term should be read-only");

        larvae::AssertTrue(term.IsValid());
    });

    auto test17 = larvae::RegisterTest("QueenQueryTerm", "ConstexprWrappers", []() {
        constexpr queen::Term read_term = queen::Read<Position>::ToTerm();
        constexpr queen::Term write_term = queen::Write<Velocity>::ToTerm();
        constexpr queen::Term with_term = queen::With<Player>::ToTerm();
        constexpr queen::Term without_term = queen::Without<Dead>::ToTerm();

        static_assert(read_term.IsReadOnly(), "Read term should be read-only");
        static_assert(write_term.IsWritable(), "Write term should be writable");
        static_assert(with_term.IsRequired(), "With term should be required");
        static_assert(without_term.IsExcluded(), "Without term should be excluded");

        larvae::AssertTrue(read_term.IsValid());
        larvae::AssertTrue(write_term.IsValid());
        larvae::AssertTrue(with_term.IsValid());
        larvae::AssertTrue(without_term.IsValid());
    });

    // ─────────────────────────────────────────────────────────────
    // ComponentType extraction tests
    // ─────────────────────────────────────────────────────────────

    auto test18 = larvae::RegisterTest("QueenQueryTerm", "ComponentTypeExtraction", []() {
        using PosRead = queen::Read<Position>;
        using VelWrite = queen::Write<Velocity>;
        using PlayerWith = queen::With<Player>;

        static_assert(std::is_same_v<PosRead::ComponentType, Position>, "ComponentType should be Position");
        static_assert(std::is_same_v<VelWrite::ComponentType, Velocity>, "ComponentType should be Velocity");
        static_assert(std::is_same_v<PlayerWith::ComponentType, Player>, "ComponentType should be Player");

        larvae::AssertTrue(true);
    });
}
