#pragma once

#include <utility>
#include <type_traits>
#include <hive/core/assert.h>
#include <comb/new.h>
#include <comb/default_allocator.h>

namespace wax
{
    /**
     * Shared ownership smart pointer with non-atomic reference counting
     *
     * Rc<T, Allocator> is a shared-ownership smart pointer that uses NON-ATOMIC
     * reference counting for maximum performance in single-threaded contexts.
     * Multiple Rc instances can share ownership of the same object.
     *
     * Performance characteristics:
     * - Storage: 16 bytes (control block* + allocator* on 64-bit)
     * - Access: O(1) - single pointer dereference
     * - Copy: O(1) - increment ref count (10-50x faster than std::shared_ptr)
     * - Destruction: O(1) - decrement ref count, deallocate if zero
     * - Ref count ops: Non-atomic (no locks, no memory barriers)
     *
     * Memory layout:
     * ┌──────────────────────────┐
     * │ Control Block:           │
     * │   size_t ref_count       │
     * │   T object (inline)      │
     * └──────────────────────────┘
     *
     * Rc stores pointer to control block + allocator reference = 16 bytes
     *
     * Limitations:
     * - NOT thread-safe (use Arc<T> for atomic ref counting)
     * - Allocator must outlive all Rc instances
     * - No weak pointers (yet)
     * - No custom deleters
     *
     * Use cases:
     * - Shared ownership in single-threaded game loops
     * - Resource sharing (textures, meshes, materials)
     * - Graph structures (nodes with multiple parents)
     * - Event systems (multiple subscribers)
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1024};
     *
     *   // Create shared object
     *   auto texture = wax::MakeRc<Texture>(alloc, "player.png");
     *   auto copy1 = texture;  // RefCount = 2 (no atomic!)
     *   auto copy2 = texture;  // RefCount = 3
     *
     *   // All copies share the same Texture
     *   texture->Load();
     *
     *   // Object destroyed when last Rc goes out of scope
     * @endcode
     */
    template<typename T, comb::Allocator Allocator = comb::DefaultAllocator>
    class Rc
    {
    private:
        // Control block with ref count and object
        struct ControlBlock
        {
            size_t ref_count;
            T object;

            template<typename... Args>
            ControlBlock(Args&&... args)
                : ref_count{1}
                , object{std::forward<Args>(args)...}
            {}
        };

        // Friend declarations for MakeRc
        template<typename U, comb::Allocator A, typename... Args>
        friend Rc<U, A> MakeRc(A& allocator, Args&&... args);

        template<typename U, typename... Args>
        friend Rc<U, comb::DefaultAllocator> MakeRc(Args&&... args);

    public:
        using ValueType = T;
        using AllocatorType = Allocator;

        constexpr Rc() noexcept
            : control_{nullptr}
            , allocator_{nullptr}
        {}

        // Constructor with control block (takes ownership)
        constexpr Rc(Allocator& allocator, ControlBlock* control) noexcept
            : control_{control}
            , allocator_{&allocator}
        {}

        // Copy constructor (increments ref count)
        Rc(const Rc& other) noexcept
            : control_{other.control_}
            , allocator_{other.allocator_}
        {
            if (control_)
            {
                ++control_->ref_count;
            }
        }

        Rc& operator=(const Rc& other) noexcept
        {
            if (this != &other)
            {
                Release();

                control_ = other.control_;
                allocator_ = other.allocator_;

                if (control_)
                {
                    ++control_->ref_count;
                }
            }
            return *this;
        }

        constexpr Rc(Rc&& other) noexcept
            : control_{other.control_}
            , allocator_{other.allocator_}
        {
            other.control_ = nullptr;
            other.allocator_ = nullptr;
        }

        constexpr Rc& operator=(Rc&& other) noexcept
        {
            if (this != &other)
            {
                Release();

                control_ = other.control_;
                allocator_ = other.allocator_;

                other.control_ = nullptr;
                other.allocator_ = nullptr;
            }
            return *this;
        }

        // Destructor (decrements ref count, destroys if zero)
        ~Rc() noexcept
        {
            Release();
        }

        // Dereference operators (checked in debug)
        [[nodiscard]] T& operator*() const noexcept
        {
            hive::Assert(control_ != nullptr, "Dereferencing null Rc");
            return control_->object;
        }

        [[nodiscard]] T* operator->() const noexcept
        {
            hive::Assert(control_ != nullptr, "Dereferencing null Rc");
            return &control_->object;
        }

        [[nodiscard]] T* Get() const noexcept
        {
            return control_ ? &control_->object : nullptr;
        }

        [[nodiscard]] Allocator* GetAllocator() const noexcept
        {
            return allocator_;
        }

        [[nodiscard]] size_t GetRefCount() const noexcept
        {
            return control_ ? control_->ref_count : 0;
        }

        [[nodiscard]] bool IsUnique() const noexcept
        {
            return control_ && control_->ref_count == 1;
        }

        // Bool conversion (null check)
        [[nodiscard]] explicit operator bool() const noexcept
        {
            return control_ != nullptr;
        }

        [[nodiscard]] bool IsNull() const noexcept
        {
            return control_ == nullptr;
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return control_ != nullptr;
        }

        // Reset (releases ownership)
        void Reset() noexcept
        {
            Release();
            control_ = nullptr;
            allocator_ = nullptr;
        }

        // Comparison operators
        [[nodiscard]] bool operator==(const Rc& other) const noexcept
        {
            return control_ == other.control_;
        }

        [[nodiscard]] bool operator==(std::nullptr_t) const noexcept
        {
            return control_ == nullptr;
        }

    private:
        void Release() noexcept
        {
            if (control_ && allocator_)
            {
                --control_->ref_count;

                if (control_->ref_count == 0)
                {
                    // Destroy object
                    control_->object.~T();

                    // Deallocate control block
                    allocator_->Deallocate(control_);
                }
            }
        }

        ControlBlock* control_;
        Allocator* allocator_;
    };

    template<typename T, comb::Allocator Allocator, typename... Args>
    [[nodiscard]] Rc<T, Allocator> MakeRc(Allocator& allocator, Args&&... args)
    {
        using ControlBlock = typename Rc<T, Allocator>::ControlBlock;

        void* mem = allocator.Allocate(sizeof(ControlBlock), alignof(ControlBlock));
        hive::Check(mem != nullptr, "Rc allocation failed");

        ControlBlock* control = new (mem) ControlBlock(std::forward<Args>(args)...);

        return Rc<T, Allocator>{allocator, control};
    }

    // Default allocator overload
    template<typename T, typename... Args>
    [[nodiscard]] Rc<T, comb::DefaultAllocator> MakeRc(Args&&... args)
    {
        auto& allocator = comb::GetDefaultAllocator();
        using ControlBlock = typename Rc<T, comb::DefaultAllocator>::ControlBlock;

        void* mem = allocator.Allocate(sizeof(ControlBlock), alignof(ControlBlock));
        hive::Check(mem != nullptr, "Rc allocation failed");

        ControlBlock* control = new (mem) ControlBlock(std::forward<Args>(args)...);

        return Rc<T, comb::DefaultAllocator>{allocator, control};
    }
}
