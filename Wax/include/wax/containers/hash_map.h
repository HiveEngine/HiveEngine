#pragma once

#include <hive/core/assert.h>
#include <comb/allocator_concepts.h>
#include <comb/default_allocator.h>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <functional>

namespace wax
{
    /**
     * Open-addressing hash map with Robin Hood hashing
     *
     * Cache-friendly hash map optimized for game engines. Uses linear probing
     * with Robin Hood insertion to minimize probe distances.
     *
     * Use cases:
     * - Entity component lookup (EntityID -> Component)
     * - Asset caches (string hash -> asset)
     * - General key-value storage with fast lookup
     *
     * Memory layout: Single contiguous array of buckets
     * ┌─────────┬─────────┬─────────┬─────────┐
     * │ Bucket0 │ Bucket1 │ Bucket2 │ ...     │
     * │ K,V,PSL │ K,V,PSL │ K,V,PSL │         │
     * └─────────┴─────────┴─────────┴─────────┘
     *
     * Performance:
     * - Insert: O(1) average
     * - Find: O(1) average
     * - Remove: O(1) average
     * - Iteration: O(capacity)
     * - Load factor: 0.75 max (rehash at 75% full)
     *
     * Limitations:
     * - Keys must be hashable (std::hash<K> or custom)
     * - Iteration order is not deterministic
     * - No pointer stability (rehash invalidates pointers)
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1_MB};
     *   wax::HashMap<EntityID, Transform> transforms{alloc, 1000};
     *
     *   transforms.Insert(entity.id, transform);
     *
     *   if (auto* t = transforms.Find(entity.id)) {
     *       t->position.x += 1.0f;
     *   }
     *
     *   transforms.Remove(entity.id);
     * @endcode
     */
    template<typename K, typename V, comb::Allocator Allocator = comb::DefaultAllocator, typename Hash = std::hash<K>>
    class HashMap
    {
    private:
        static constexpr uint8_t kEmpty = 0;
        static constexpr uint8_t kOccupied = 1;
        static constexpr uint8_t kDeleted = 2;
        static constexpr float kMaxLoadFactor = 0.75f;

        struct Bucket
        {
            alignas(K) unsigned char key_storage[sizeof(K)];
            alignas(V) unsigned char value_storage[sizeof(V)];
            uint8_t state{kEmpty};
            uint8_t psl{0};  // Probe Sequence Length (Robin Hood)

            K* Key() { return reinterpret_cast<K*>(key_storage); }
            const K* Key() const { return reinterpret_cast<const K*>(key_storage); }
            V* Value() { return reinterpret_cast<V*>(value_storage); }
            const V* Value() const { return reinterpret_cast<const V*>(value_storage); }
        };

    public:
        class Iterator
        {
        public:
            Iterator(Bucket* buckets, size_t index, size_t capacity)
                : buckets_{buckets}, index_{index}, capacity_{capacity}
            {
                SkipEmpty();
            }

            std::pair<const K&, V&> operator*() const
            {
                return {*buckets_[index_].Key(), *buckets_[index_].Value()};
            }

            Iterator& operator++()
            {
                ++index_;
                SkipEmpty();
                return *this;
            }

            bool operator==(const Iterator& other) const { return index_ == other.index_; }
            bool operator!=(const Iterator& other) const { return index_ != other.index_; }

            const K& Key() const { return *buckets_[index_].Key(); }
            V& Value() { return *buckets_[index_].Value(); }

        private:
            void SkipEmpty()
            {
                while (index_ < capacity_ && buckets_[index_].state != kOccupied)
                {
                    ++index_;
                }
            }

            Bucket* buckets_;
            size_t index_;
            size_t capacity_;
        };

        class ConstIterator
        {
        public:
            ConstIterator(const Bucket* buckets, size_t index, size_t capacity)
                : buckets_{buckets}, index_{index}, capacity_{capacity}
            {
                SkipEmpty();
            }

            std::pair<const K&, const V&> operator*() const
            {
                return {*buckets_[index_].Key(), *buckets_[index_].Value()};
            }

            ConstIterator& operator++()
            {
                ++index_;
                SkipEmpty();
                return *this;
            }

            bool operator==(const ConstIterator& other) const { return index_ == other.index_; }
            bool operator!=(const ConstIterator& other) const { return index_ != other.index_; }

            const K& Key() const { return *buckets_[index_].Key(); }
            const V& Value() const { return *buckets_[index_].Value(); }

        private:
            void SkipEmpty()
            {
                while (index_ < capacity_ && buckets_[index_].state != kOccupied)
                {
                    ++index_;
                }
            }

            const Bucket* buckets_;
            size_t index_;
            size_t capacity_;
        };

        // Default constructor - uses global default allocator
        HashMap(size_t initial_capacity = 16)
            requires std::is_same_v<Allocator, comb::DefaultAllocator>
            : allocator_{&comb::GetDefaultAllocator()}
            , capacity_{NextPowerOfTwo(initial_capacity)}
            , count_{0}
        {
            hive::Assert(initial_capacity > 0, "HashMap capacity must be > 0");

            buckets_ = static_cast<Bucket*>(allocator_->Allocate(sizeof(Bucket) * capacity_, alignof(Bucket)));
            hive::Assert(buckets_ != nullptr, "Failed to allocate HashMap buckets");

            for (size_t i = 0; i < capacity_; ++i)
            {
                buckets_[i].state = kEmpty;
                buckets_[i].psl = 0;
            }
        }

        // Constructor with allocator
        HashMap(Allocator& allocator, size_t initial_capacity = 16)
            : allocator_{&allocator}
            , capacity_{NextPowerOfTwo(initial_capacity)}
            , count_{0}
        {
            hive::Assert(initial_capacity > 0, "HashMap capacity must be > 0");

            buckets_ = static_cast<Bucket*>(allocator.Allocate(sizeof(Bucket) * capacity_, alignof(Bucket)));
            hive::Assert(buckets_ != nullptr, "Failed to allocate HashMap buckets");

            for (size_t i = 0; i < capacity_; ++i)
            {
                buckets_[i].state = kEmpty;
                buckets_[i].psl = 0;
            }
        }

        ~HashMap()
        {
            Clear();
            if (buckets_ && allocator_)
            {
                allocator_->Deallocate(buckets_);
            }
        }

        HashMap(const HashMap&) = delete;
        HashMap& operator=(const HashMap&) = delete;

        HashMap(HashMap&& other) noexcept
            : allocator_{other.allocator_}
            , buckets_{other.buckets_}
            , capacity_{other.capacity_}
            , count_{other.count_}
        {
            other.allocator_ = nullptr;
            other.buckets_ = nullptr;
            other.capacity_ = 0;
            other.count_ = 0;
        }

        HashMap& operator=(HashMap&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                if (buckets_ && allocator_)
                {
                    allocator_->Deallocate(buckets_);
                }

                allocator_ = other.allocator_;
                buckets_ = other.buckets_;
                capacity_ = other.capacity_;
                count_ = other.count_;

                other.allocator_ = nullptr;
                other.buckets_ = nullptr;
                other.capacity_ = 0;
                other.count_ = 0;
            }
            return *this;
        }

        bool Insert(const K& key, const V& value)
        {
            if (ShouldRehash())
            {
                Rehash(capacity_ * 2);
            }

            return InsertInternal(key, value);
        }

        bool Insert(const K& key, V&& value)
        {
            if (ShouldRehash())
            {
                Rehash(capacity_ * 2);
            }

            return InsertInternalMove(key, static_cast<V&&>(value));
        }

        template<typename... Args>
        bool Emplace(const K& key, Args&&... args)
        {
            if (ShouldRehash())
            {
                Rehash(capacity_ * 2);
            }

            size_t hash = Hash{}(key);
            size_t index = hash & (capacity_ - 1);
            uint8_t psl = 0;

            while (true)
            {
                Bucket& bucket = buckets_[index];

                if (bucket.state == kEmpty || bucket.state == kDeleted)
                {
                    new (bucket.key_storage) K(key);
                    new (bucket.value_storage) V(static_cast<Args&&>(args)...);
                    bucket.state = kOccupied;
                    bucket.psl = psl;
                    ++count_;
                    return true;
                }

                if (bucket.state == kOccupied && *bucket.Key() == key)
                {
                    return false;
                }

                if (psl > bucket.psl)
                {
                    K temp_key = static_cast<K&&>(*bucket.Key());
                    V temp_value = static_cast<V&&>(*bucket.Value());
                    uint8_t temp_psl = bucket.psl;

                    bucket.Key()->~K();
                    bucket.Value()->~V();

                    new (bucket.key_storage) K(key);
                    new (bucket.value_storage) V(static_cast<Args&&>(args)...);
                    bucket.psl = psl;

                    InsertDisplaced(static_cast<K&&>(temp_key), static_cast<V&&>(temp_value), temp_psl, index);
                    ++count_;
                    return true;
                }

                ++psl;
                index = (index + 1) & (capacity_ - 1);
            }
        }

        [[nodiscard]] V* Find(const K& key) noexcept
        {
            size_t hash = Hash{}(key);
            size_t index = hash & (capacity_ - 1);
            uint8_t psl = 0;

            while (true)
            {
                Bucket& bucket = buckets_[index];

                if (bucket.state == kEmpty)
                {
                    return nullptr;
                }

                if (bucket.state == kOccupied)
                {
                    if (psl > bucket.psl)
                    {
                        return nullptr;
                    }

                    if (*bucket.Key() == key)
                    {
                        return bucket.Value();
                    }
                }

                ++psl;
                index = (index + 1) & (capacity_ - 1);
            }
        }

        [[nodiscard]] const V* Find(const K& key) const noexcept
        {
            return const_cast<HashMap*>(this)->Find(key);
        }

        [[nodiscard]] bool Contains(const K& key) const noexcept
        {
            return Find(key) != nullptr;
        }

        bool Remove(const K& key)
        {
            size_t hash = Hash{}(key);
            size_t index = hash & (capacity_ - 1);
            uint8_t psl = 0;

            while (true)
            {
                Bucket& bucket = buckets_[index];

                if (bucket.state == kEmpty)
                {
                    return false;
                }

                if (bucket.state == kOccupied)
                {
                    if (psl > bucket.psl)
                    {
                        return false;
                    }

                    if (*bucket.Key() == key)
                    {
                        bucket.Key()->~K();
                        bucket.Value()->~V();
                        bucket.state = kDeleted;
                        --count_;

                        ShiftBackward(index);
                        return true;
                    }
                }

                ++psl;
                index = (index + 1) & (capacity_ - 1);
            }
        }

        V& operator[](const K& key)
        {
            V* found = Find(key);
            if (found)
            {
                return *found;
            }

            Emplace(key);
            return *Find(key);
        }

        void Clear()
        {
            for (size_t i = 0; i < capacity_; ++i)
            {
                if (buckets_[i].state == kOccupied)
                {
                    buckets_[i].Key()->~K();
                    buckets_[i].Value()->~V();
                    buckets_[i].state = kEmpty;
                }
                else
                {
                    buckets_[i].state = kEmpty;
                }
                buckets_[i].psl = 0;
            }
            count_ = 0;
        }

        [[nodiscard]] size_t Count() const noexcept { return count_; }
        [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }
        [[nodiscard]] bool IsEmpty() const noexcept { return count_ == 0; }
        [[nodiscard]] float LoadFactor() const noexcept { return static_cast<float>(count_) / capacity_; }

        Iterator begin() { return Iterator{buckets_, 0, capacity_}; }
        Iterator end() { return Iterator{buckets_, capacity_, capacity_}; }
        ConstIterator begin() const { return ConstIterator{buckets_, 0, capacity_}; }
        ConstIterator end() const { return ConstIterator{buckets_, capacity_, capacity_}; }

    private:
        [[nodiscard]] bool ShouldRehash() const noexcept
        {
            return count_ >= static_cast<size_t>(static_cast<float>(capacity_) * kMaxLoadFactor);
        }

        void Rehash(size_t new_capacity)
        {
            Bucket* old_buckets = buckets_;
            size_t old_capacity = capacity_;

            capacity_ = new_capacity;
            buckets_ = static_cast<Bucket*>(allocator_->Allocate(sizeof(Bucket) * capacity_, alignof(Bucket)));
            hive::Assert(buckets_ != nullptr, "Failed to allocate HashMap buckets during rehash");

            for (size_t i = 0; i < capacity_; ++i)
            {
                buckets_[i].state = kEmpty;
                buckets_[i].psl = 0;
            }

            count_ = 0;

            for (size_t i = 0; i < old_capacity; ++i)
            {
                if (old_buckets[i].state == kOccupied)
                {
                    InsertInternalMove(*old_buckets[i].Key(), static_cast<V&&>(*old_buckets[i].Value()));
                    old_buckets[i].Key()->~K();
                    old_buckets[i].Value()->~V();
                }
            }

            allocator_->Deallocate(old_buckets);
        }

        bool InsertInternal(const K& key, const V& value)
        {
            size_t hash = Hash{}(key);
            size_t index = hash & (capacity_ - 1);
            uint8_t psl = 0;

            K insert_key = key;
            V insert_value = value;

            while (true)
            {
                Bucket& bucket = buckets_[index];

                if (bucket.state == kEmpty || bucket.state == kDeleted)
                {
                    new (bucket.key_storage) K(static_cast<K&&>(insert_key));
                    new (bucket.value_storage) V(static_cast<V&&>(insert_value));
                    bucket.state = kOccupied;
                    bucket.psl = psl;
                    ++count_;
                    return true;
                }

                if (bucket.state == kOccupied && *bucket.Key() == key)
                {
                    return false;
                }

                if (psl > bucket.psl)
                {
                    std::swap(insert_key, *bucket.Key());
                    std::swap(insert_value, *bucket.Value());
                    std::swap(psl, bucket.psl);
                }

                ++psl;
                index = (index + 1) & (capacity_ - 1);
            }
        }

        bool InsertInternalMove(const K& key, V&& value)
        {
            size_t hash = Hash{}(key);
            size_t index = hash & (capacity_ - 1);
            uint8_t psl = 0;

            K insert_key = key;
            V insert_value = static_cast<V&&>(value);

            while (true)
            {
                Bucket& bucket = buckets_[index];

                if (bucket.state == kEmpty || bucket.state == kDeleted)
                {
                    new (bucket.key_storage) K(static_cast<K&&>(insert_key));
                    new (bucket.value_storage) V(static_cast<V&&>(insert_value));
                    bucket.state = kOccupied;
                    bucket.psl = psl;
                    ++count_;
                    return true;
                }

                if (bucket.state == kOccupied && *bucket.Key() == key)
                {
                    return false;
                }

                if (psl > bucket.psl)
                {
                    std::swap(insert_key, *bucket.Key());
                    std::swap(insert_value, *bucket.Value());
                    std::swap(psl, bucket.psl);
                }

                ++psl;
                index = (index + 1) & (capacity_ - 1);
            }
        }

        void InsertDisplaced(K&& key, V&& value, uint8_t psl, size_t start_index)
        {
            size_t index = (start_index + 1) & (capacity_ - 1);
            ++psl;

            while (true)
            {
                Bucket& bucket = buckets_[index];

                if (bucket.state == kEmpty || bucket.state == kDeleted)
                {
                    new (bucket.key_storage) K(static_cast<K&&>(key));
                    new (bucket.value_storage) V(static_cast<V&&>(value));
                    bucket.state = kOccupied;
                    bucket.psl = psl;
                    return;
                }

                if (psl > bucket.psl)
                {
                    std::swap(key, *bucket.Key());
                    std::swap(value, *bucket.Value());
                    std::swap(psl, bucket.psl);
                }

                ++psl;
                index = (index + 1) & (capacity_ - 1);
            }
        }

        void ShiftBackward(size_t removed_index)
        {
            size_t current = removed_index;
            size_t next = (current + 1) & (capacity_ - 1);

            while (buckets_[next].state == kOccupied && buckets_[next].psl > 0)
            {
                new (buckets_[current].key_storage) K(static_cast<K&&>(*buckets_[next].Key()));
                new (buckets_[current].value_storage) V(static_cast<V&&>(*buckets_[next].Value()));
                buckets_[current].state = kOccupied;
                buckets_[current].psl = buckets_[next].psl - 1;

                buckets_[next].Key()->~K();
                buckets_[next].Value()->~V();
                buckets_[next].state = kEmpty;

                current = next;
                next = (next + 1) & (capacity_ - 1);
            }
        }

        static constexpr size_t NextPowerOfTwo(size_t n)
        {
            --n;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            n |= n >> 32;
            return n + 1;
        }

        Allocator* allocator_;
        Bucket* buckets_;
        size_t capacity_;
        size_t count_;
    };
}
