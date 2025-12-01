#pragma once

#include <hive/core/assert.h>
#include <comb/allocator_concepts.h>
#include <cstdint>
#include <cstddef>
#include <functional>

namespace wax
{
    /**
     * Open-addressing hash set with Robin Hood hashing
     *
     * Cache-friendly set optimized for game engines. Uses linear probing
     * with Robin Hood insertion to minimize probe distances.
     *
     * Use cases:
     * - Tracking unique entity IDs
     * - Visited node sets in pathfinding
     * - Tag/flag collections
     *
     * Performance:
     * - Insert: O(1) average
     * - Contains: O(1) average
     * - Remove: O(1) average
     * - Load factor: 0.75 max
     */
    template<typename K, comb::Allocator Allocator, typename Hash = std::hash<K>>
    class HashSet
    {
    private:
        static constexpr uint8_t kEmpty = 0;
        static constexpr uint8_t kOccupied = 1;
        static constexpr uint8_t kDeleted = 2;
        static constexpr float kMaxLoadFactor = 0.75f;

        struct Bucket
        {
            alignas(K) unsigned char key_storage[sizeof(K)];
            uint8_t state{kEmpty};
            uint8_t psl{0};

            K* Key() { return reinterpret_cast<K*>(key_storage); }
            const K* Key() const { return reinterpret_cast<const K*>(key_storage); }
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

            const K& operator*() const { return *buckets_[index_].Key(); }

            Iterator& operator++()
            {
                ++index_;
                SkipEmpty();
                return *this;
            }

            bool operator==(const Iterator& other) const { return index_ == other.index_; }
            bool operator!=(const Iterator& other) const { return index_ != other.index_; }

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

        HashSet(Allocator& allocator, size_t initial_capacity = 16)
            : allocator_{&allocator}
            , capacity_{NextPowerOfTwo(initial_capacity)}
            , count_{0}
        {
            hive::Assert(initial_capacity > 0, "HashSet capacity must be > 0");

            buckets_ = static_cast<Bucket*>(allocator.Allocate(sizeof(Bucket) * capacity_, alignof(Bucket)));
            hive::Assert(buckets_ != nullptr, "Failed to allocate HashSet buckets");

            for (size_t i = 0; i < capacity_; ++i)
            {
                buckets_[i].state = kEmpty;
                buckets_[i].psl = 0;
            }
        }

        ~HashSet()
        {
            Clear();
            if (buckets_ && allocator_)
            {
                allocator_->Deallocate(buckets_);
            }
        }

        HashSet(const HashSet&) = delete;
        HashSet& operator=(const HashSet&) = delete;

        HashSet(HashSet&& other) noexcept
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

        HashSet& operator=(HashSet&& other) noexcept
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

        bool Insert(const K& key)
        {
            if (ShouldRehash())
            {
                Rehash(capacity_ * 2);
            }

            return InsertInternal(key);
        }

        [[nodiscard]] bool Contains(const K& key) const noexcept
        {
            size_t hash = Hash{}(key);
            size_t index = hash & (capacity_ - 1);
            uint8_t psl = 0;

            while (true)
            {
                const Bucket& bucket = buckets_[index];

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
                        return true;
                    }
                }

                ++psl;
                index = (index + 1) & (capacity_ - 1);
            }
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

        void Clear()
        {
            for (size_t i = 0; i < capacity_; ++i)
            {
                if (buckets_[i].state == kOccupied)
                {
                    buckets_[i].Key()->~K();
                }
                buckets_[i].state = kEmpty;
                buckets_[i].psl = 0;
            }
            count_ = 0;
        }

        [[nodiscard]] size_t Count() const noexcept { return count_; }
        [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }
        [[nodiscard]] bool IsEmpty() const noexcept { return count_ == 0; }
        [[nodiscard]] float LoadFactor() const noexcept { return static_cast<float>(count_) / static_cast<float>(capacity_); }

        Iterator begin() { return Iterator{buckets_, 0, capacity_}; }
        Iterator end() { return Iterator{buckets_, capacity_, capacity_}; }

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
            hive::Assert(buckets_ != nullptr, "Failed to allocate HashSet buckets during rehash");

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
                    InsertInternal(*old_buckets[i].Key());
                    old_buckets[i].Key()->~K();
                }
            }

            allocator_->Deallocate(old_buckets);
        }

        bool InsertInternal(const K& key)
        {
            size_t hash = Hash{}(key);
            size_t index = hash & (capacity_ - 1);
            uint8_t psl = 0;

            K insert_key = key;

            while (true)
            {
                Bucket& bucket = buckets_[index];

                if (bucket.state == kEmpty || bucket.state == kDeleted)
                {
                    new (bucket.key_storage) K(static_cast<K&&>(insert_key));
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
                buckets_[current].state = kOccupied;
                buckets_[current].psl = buckets_[next].psl - 1;

                buckets_[next].Key()->~K();
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
