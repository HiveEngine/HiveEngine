#include <larvae/larvae.h>
#include <nectar/vfs/path.h>
#include <comb/default_allocator.h>

namespace {

    auto& GetPathAlloc()
    {
        static comb::ModuleAllocator alloc{"TestPath", 1 * 1024 * 1024};
        return alloc.Get();
    }

    // =========================================================================
    // NormalizePath
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarPath", "ForwardSlashUnchanged", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("a/b/c", alloc);
        larvae::AssertTrue(r.View().Equals("a/b/c"));
    });

    auto t2 = larvae::RegisterTest("NectarPath", "BackslashConverted", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("a\\b\\c", alloc);
        larvae::AssertTrue(r.View().Equals("a/b/c"));
    });

    auto t3 = larvae::RegisterTest("NectarPath", "Lowercase", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("Textures/Hero.PNG", alloc);
        larvae::AssertTrue(r.View().Equals("textures/hero.png"));
    });

    auto t4 = larvae::RegisterTest("NectarPath", "DoubleSlash", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("a//b///c", alloc);
        larvae::AssertTrue(r.View().Equals("a/b/c"));
    });

    auto t5 = larvae::RegisterTest("NectarPath", "TrailingSlash", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("a/b/", alloc);
        larvae::AssertTrue(r.View().Equals("a/b"));
    });

    auto t6 = larvae::RegisterTest("NectarPath", "LeadingSlash", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("/a/b", alloc);
        larvae::AssertTrue(r.View().Equals("a/b"));
    });

    auto t7 = larvae::RegisterTest("NectarPath", "DotResolved", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("a/./b", alloc);
        larvae::AssertTrue(r.View().Equals("a/b"));
    });

    auto t8 = larvae::RegisterTest("NectarPath", "DotDotResolved", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("a/b/../c", alloc);
        larvae::AssertTrue(r.View().Equals("a/c"));
    });

    auto t9 = larvae::RegisterTest("NectarPath", "DotDotAtRoot", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("../a", alloc);
        larvae::AssertTrue(r.View().Equals("a"));
    });

    auto t10 = larvae::RegisterTest("NectarPath", "Empty", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("", alloc);
        larvae::AssertTrue(r.View().Equals(""));
    });

    auto t11 = larvae::RegisterTest("NectarPath", "SingleFile", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("file.txt", alloc);
        larvae::AssertTrue(r.View().Equals("file.txt"));
    });

    auto t12 = larvae::RegisterTest("NectarPath", "MixedSeparators", []() {
        auto& alloc = GetPathAlloc();
        auto r = nectar::NormalizePath("a\\b/c\\d", alloc);
        larvae::AssertTrue(r.View().Equals("a/b/c/d"));
    });

    // =========================================================================
    // PathParent / PathFilename / PathExtension
    // =========================================================================

    auto t13 = larvae::RegisterTest("NectarPath", "PathParent", []() {
        auto p = nectar::PathParent("textures/hero.png");
        larvae::AssertTrue(p.Equals("textures"));
    });

    auto t14 = larvae::RegisterTest("NectarPath", "PathParentNoSlash", []() {
        auto p = nectar::PathParent("hero.png");
        larvae::AssertEqual(p.Size(), size_t{0});
    });

    auto t15 = larvae::RegisterTest("NectarPath", "PathFilename", []() {
        auto f = nectar::PathFilename("textures/hero.png");
        larvae::AssertTrue(f.Equals("hero.png"));
    });

    auto t16 = larvae::RegisterTest("NectarPath", "PathFilenameNoSlash", []() {
        auto f = nectar::PathFilename("hero.png");
        larvae::AssertTrue(f.Equals("hero.png"));
    });

    auto t17 = larvae::RegisterTest("NectarPath", "PathExtension", []() {
        auto e = nectar::PathExtension("hero.png");
        larvae::AssertTrue(e.Equals(".png"));
    });

    auto t18 = larvae::RegisterTest("NectarPath", "PathExtensionNone", []() {
        auto e = nectar::PathExtension("Makefile");
        larvae::AssertEqual(e.Size(), size_t{0});
    });

    auto t19 = larvae::RegisterTest("NectarPath", "PathExtensionDotfile", []() {
        auto e = nectar::PathExtension(".gitignore");
        larvae::AssertEqual(e.Size(), size_t{0});
    });

}
