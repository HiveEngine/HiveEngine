#pragma once

#include <hive/core/assert.h>

#include <comb/memory_resource.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace wax
{
    template <typename K, typename V, typename Hash = std::hash<K>> class HashMap
    {
    private:
        static constexpr uint8_t kEmpty = 0;
        static constexpr uint8_t kOccupied = 1;
        static constexpr uint8_t kDeleted = 2;
        static constexpr float kMaxLoadFactor = 0.75f;

        struct Bucket
        {
            alignas(K) unsigned char m_keyStorage[sizeof(K)];
            alignas(V) unsigned char m_valueStorage[sizeof(V)];
            uint8_t m_state{kEmpty};
            uint8_t m_psl{0};

            [[nodiscard]] K* Key()
            {
                return reinterpret_cast<K*>(m_keyStorage);
            }
            [[nodiscard]] const K* Key() const
            {
                return reinterpret_cast<const K*>(m_keyStorage);
            }
            [[nodiscard]] V* Value()
            {
                return reinterpret_cast<V*>(m_valueStorage);
            }
            [[nodiscard]] const V* Value() const
            {
                return reinterpret_cast<const V*>(m_valueStorage);
            }
        };

    public:
        class Iterator
        {
        public:
            Iterator(Bucket* buckets, size_t index, size_t capacity)
                : m_buckets{buckets}
                , m_index{index}
                , m_capacity{capacity}
            {
                SkipEmpty();
            }

            [[nodiscard]] std::pair<const K&, V&> operator*() const
            {
                return {*m_buckets[m_index].Key(), *m_buckets[m_index].Value()};
            }

            Iterator& operator++()
            {
                ++m_index;
                SkipEmpty();
                return *this;
            }

            bool operator==(const Iterator& other) const
            {
                return m_index == other.m_index;
            }
            [[nodiscard]] const K& Key() const
            {
                return *m_buckets[m_index].Key();
            }
            [[nodiscard]] V& Value()
            {
                return *m_buckets[m_index].Value();
            }

        private:
            void SkipEmpty()
            {
                while (m_index < m_capacity && m_buckets[m_index].m_state != kOccupied)
                {
                    ++m_index;
                }
            }

            Bucket* m_buckets;
            size_t m_index;
            size_t m_capacity;
        };

        class ConstIterator
        {
        public:
            ConstIterator(const Bucket* buckets, size_t index, size_t capacity)
                : m_buckets{buckets}
                , m_index{index}
                , m_capacity{capacity}
            {
                SkipEmpty();
            }

            [[nodiscard]] std::pair<const K&, const V&> operator*() const
            {
                return {*m_buckets[m_index].Key(), *m_buckets[m_index].Value()};
            }

            ConstIterator& operator++()
            {
                ++m_index;
                SkipEmpty();
                return *this;
            }

            bool operator==(const ConstIterator& other) const
            {
                return m_index == other.m_index;
            }
            [[nodiscard]] const K& Key() const
            {
                return *m_buckets[m_index].Key();
            }
            [[nodiscard]] const V& Value() const
            {
                return *m_buckets[m_index].Value();
            }

        private:
            void SkipEmpty()
            {
                while (m_index < m_capacity && m_buckets[m_index].m_state != kOccupied)
                {
                    ++m_index;
                }
            }

            const Bucket* m_buckets;
            size_t m_index;
            size_t m_capacity;
        };

        explicit HashMap(size_t initialCapacity = 16)
            : m_allocator{comb::GetDefaultMemoryResource()}
            , m_capacity{NextPowerOfTwo(initialCapacity)}
            , m_count{0}
        {
            InitializeBuckets(initialCapacity);
        }

        explicit HashMap(comb::MemoryResource allocator, size_t initialCapacity = 16)
            : m_allocator{allocator}
            , m_capacity{NextPowerOfTwo(initialCapacity)}
            , m_count{0}
        {
            InitializeBuckets(initialCapacity);
        }

        template <comb::Allocator Allocator>
        HashMap(Allocator& allocator, size_t initialCapacity = 16)
            : HashMap{comb::MemoryResource{allocator}, initialCapacity}
        {
        }

        ~HashMap()
        {
            Clear();
            if (m_buckets)
            {
                m_allocator.Deallocate(m_buckets);
            }
        }

        HashMap(const HashMap&) = delete;
        HashMap& operator=(const HashMap&) = delete;

        HashMap(HashMap&& other) noexcept
            : m_allocator{other.m_allocator}
            , m_buckets{other.m_buckets}
            , m_capacity{other.m_capacity}
            , m_count{other.m_count}
        {
            other.m_buckets = nullptr;
            other.m_capacity = 0;
            other.m_count = 0;
        }

        HashMap& operator=(HashMap&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                if (m_buckets)
                {
                    m_allocator.Deallocate(m_buckets);
                }

                m_allocator = other.m_allocator;
                m_buckets = other.m_buckets;
                m_capacity = other.m_capacity;
                m_count = other.m_count;

                other.m_buckets = nullptr;
                other.m_capacity = 0;
                other.m_count = 0;
            }
            return *this;
        }

        bool Insert(const K& key, const V& value)
        {
            if (ShouldRehash())
            {
                Rehash(m_capacity * 2);
            }
            return InsertInternal(key, value);
        }

        bool Insert(const K& key, V&& value)
        {
            if (ShouldRehash())
            {
                Rehash(m_capacity * 2);
            }
            return InsertInternalMove(key, static_cast<V&&>(value));
        }

        template <typename... Args> bool Emplace(const K& key, Args&&... args)
        {
            if (ShouldRehash())
            {
                Rehash(m_capacity * 2);
            }

            const size_t hash = Hash{}(key);
            size_t index = hash & (m_capacity - 1);
            uint8_t psl = 0;

            while (true)
            {
                Bucket& bucket = m_buckets[index];

                if (bucket.m_state == kEmpty || bucket.m_state == kDeleted)
                {
                    new (bucket.m_keyStorage) K(key);
                    new (bucket.m_valueStorage) V(static_cast<Args&&>(args)...);
                    bucket.m_state = kOccupied;
                    bucket.m_psl = psl;
                    ++m_count;
                    return true;
                }

                if (bucket.m_state == kOccupied && *bucket.Key() == key)
                {
                    return false;
                }

                if (psl > bucket.m_psl)
                {
                    K tempKey = static_cast<K&&>(*bucket.Key());
                    V tempValue = static_cast<V&&>(*bucket.Value());
                    const uint8_t tempPsl = bucket.m_psl;

                    bucket.Key()->~K();
                    bucket.Value()->~V();

                    new (bucket.m_keyStorage) K(key);
                    new (bucket.m_valueStorage) V(static_cast<Args&&>(args)...);
                    bucket.m_psl = psl;

                    InsertDisplaced(static_cast<K&&>(tempKey), static_cast<V&&>(tempValue), tempPsl, index);
                    ++m_count;
                    return true;
                }

                ++psl;
                index = (index + 1) & (m_capacity - 1);
            }
        }

        [[nodiscard]] V* Find(const K& key) noexcept
        {
            const size_t hash = Hash{}(key);
            size_t index = hash & (m_capacity - 1);
            uint8_t psl = 0;

            while (true)
            {
                Bucket& bucket = m_buckets[index];

                if (bucket.m_state == kEmpty)
                {
                    return nullptr;
                }

                if (bucket.m_state == kOccupied)
                {
                    if (psl > bucket.m_psl)
                    {
                        return nullptr;
                    }

                    if (*bucket.Key() == key)
                    {
                        return bucket.Value();
                    }
                }

                ++psl;
                index = (index + 1) & (m_capacity - 1);
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
            const size_t hash = Hash{}(key);
            size_t index = hash & (m_capacity - 1);
            uint8_t psl = 0;

            while (true)
            {
                Bucket& bucket = m_buckets[index];

                if (bucket.m_state == kEmpty)
                {
                    return false;
                }

                if (bucket.m_state == kOccupied)
                {
                    if (psl > bucket.m_psl)
                    {
                        return false;
                    }

                    if (*bucket.Key() == key)
                    {
                        bucket.Key()->~K();
                        bucket.Value()->~V();
                        bucket.m_state = kDeleted;
                        --m_count;
                        ShiftBackward(index);
                        return true;
                    }
                }

                ++psl;
                index = (index + 1) & (m_capacity - 1);
            }
        }

        V& operator[](const K& key)
        {
            if (V* found = Find(key))
            {
                return *found;
            }

            Emplace(key);
            return *Find(key);
        }

        void Clear() noexcept
        {
            if (m_buckets == nullptr)
            {
                return;
            }

            for (size_t i = 0; i < m_capacity; ++i)
            {
                if (m_buckets[i].m_state == kOccupied)
                {
                    m_buckets[i].Key()->~K();
                    m_buckets[i].Value()->~V();
                }
                m_buckets[i].m_state = kEmpty;
                m_buckets[i].m_psl = 0;
            }
            m_count = 0;
        }

        [[nodiscard]] size_t Count() const noexcept
        {
            return m_count;
        }
        [[nodiscard]] size_t Capacity() const noexcept
        {
            return m_capacity;
        }
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_count == 0;
        }
        [[nodiscard]] float LoadFactor() const noexcept
        {
            return static_cast<float>(m_count) / static_cast<float>(m_capacity);
        }
        [[nodiscard]] comb::MemoryResource GetAllocator() const noexcept
        {
            return m_allocator;
        }

        [[nodiscard]] Iterator Begin() noexcept
        {
            return Iterator{m_buckets, 0, m_capacity};
        }
        [[nodiscard]] Iterator End() noexcept
        {
            return Iterator{m_buckets, m_capacity, m_capacity};
        }
        [[nodiscard]] ConstIterator Begin() const noexcept
        {
            return ConstIterator{m_buckets, 0, m_capacity};
        }
        [[nodiscard]] ConstIterator End() const noexcept
        {
            return ConstIterator{m_buckets, m_capacity, m_capacity};
        }

    private:
        void InitializeBuckets(size_t initialCapacity)
        {
            hive::Assert(initialCapacity > 0, "HashMap capacity must be > 0");

            m_buckets = static_cast<Bucket*>(m_allocator.Allocate(sizeof(Bucket) * m_capacity, alignof(Bucket)));
            hive::Assert(m_buckets != nullptr, "Failed to allocate HashMap buckets");

            for (size_t i = 0; i < m_capacity; ++i)
            {
                m_buckets[i].m_state = kEmpty;
                m_buckets[i].m_psl = 0;
            }
        }

        [[nodiscard]] bool ShouldRehash() const noexcept
        {
            return m_count >= static_cast<size_t>(static_cast<float>(m_capacity) * kMaxLoadFactor);
        }

        void Rehash(size_t newCapacity)
        {
            Bucket* oldBuckets = m_buckets;
            const size_t oldCapacity = m_capacity;

            m_capacity = newCapacity;
            m_buckets = static_cast<Bucket*>(m_allocator.Allocate(sizeof(Bucket) * m_capacity, alignof(Bucket)));
            hive::Assert(m_buckets != nullptr, "Failed to allocate HashMap buckets during rehash");

            for (size_t i = 0; i < m_capacity; ++i)
            {
                m_buckets[i].m_state = kEmpty;
                m_buckets[i].m_psl = 0;
            }

            m_count = 0;

            for (size_t i = 0; i < oldCapacity; ++i)
            {
                if (oldBuckets[i].m_state == kOccupied)
                {
                    InsertInternalMove(*oldBuckets[i].Key(), static_cast<V&&>(*oldBuckets[i].Value()));
                    oldBuckets[i].Key()->~K();
                    oldBuckets[i].Value()->~V();
                }
            }

            m_allocator.Deallocate(oldBuckets);
        }

        bool InsertInternal(const K& key, const V& value)
        {
            const size_t hash = Hash{}(key);
            size_t index = hash & (m_capacity - 1);
            uint8_t psl = 0;

            K insertKey = key;
            V insertValue = value;

            while (true)
            {
                Bucket& bucket = m_buckets[index];

                if (bucket.m_state == kEmpty || bucket.m_state == kDeleted)
                {
                    new (bucket.m_keyStorage) K(static_cast<K&&>(insertKey));
                    new (bucket.m_valueStorage) V(static_cast<V&&>(insertValue));
                    bucket.m_state = kOccupied;
                    bucket.m_psl = psl;
                    ++m_count;
                    return true;
                }

                if (bucket.m_state == kOccupied && *bucket.Key() == key)
                {
                    return false;
                }

                if (psl > bucket.m_psl)
                {
                    std::swap(insertKey, *bucket.Key());
                    std::swap(insertValue, *bucket.Value());
                    std::swap(psl, bucket.m_psl);
                }

                ++psl;
                index = (index + 1) & (m_capacity - 1);
            }
        }

        bool InsertInternalMove(const K& key, V&& value)
        {
            const size_t hash = Hash{}(key);
            size_t index = hash & (m_capacity - 1);
            uint8_t psl = 0;

            K insertKey = key;
            V insertValue = static_cast<V&&>(value);

            while (true)
            {
                Bucket& bucket = m_buckets[index];

                if (bucket.m_state == kEmpty || bucket.m_state == kDeleted)
                {
                    new (bucket.m_keyStorage) K(static_cast<K&&>(insertKey));
                    new (bucket.m_valueStorage) V(static_cast<V&&>(insertValue));
                    bucket.m_state = kOccupied;
                    bucket.m_psl = psl;
                    ++m_count;
                    return true;
                }

                if (bucket.m_state == kOccupied && *bucket.Key() == key)
                {
                    return false;
                }

                if (psl > bucket.m_psl)
                {
                    std::swap(insertKey, *bucket.Key());
                    std::swap(insertValue, *bucket.Value());
                    std::swap(psl, bucket.m_psl);
                }

                ++psl;
                index = (index + 1) & (m_capacity - 1);
            }
        }

        void InsertDisplaced(K&& key, V&& value, uint8_t psl, size_t startIndex)
        {
            size_t index = (startIndex + 1) & (m_capacity - 1);
            ++psl;

            while (true)
            {
                Bucket& bucket = m_buckets[index];

                if (bucket.m_state == kEmpty || bucket.m_state == kDeleted)
                {
                    new (bucket.m_keyStorage) K(static_cast<K&&>(key));
                    new (bucket.m_valueStorage) V(static_cast<V&&>(value));
                    bucket.m_state = kOccupied;
                    bucket.m_psl = psl;
                    return;
                }

                if (psl > bucket.m_psl)
                {
                    std::swap(key, *bucket.Key());
                    std::swap(value, *bucket.Value());
                    std::swap(psl, bucket.m_psl);
                }

                ++psl;
                index = (index + 1) & (m_capacity - 1);
            }
        }

        void ShiftBackward(size_t removedIndex)
        {
            size_t current = removedIndex;
            size_t next = (current + 1) & (m_capacity - 1);

            while (m_buckets[next].m_state == kOccupied && m_buckets[next].m_psl > 0)
            {
                new (m_buckets[current].m_keyStorage) K(static_cast<K&&>(*m_buckets[next].Key()));
                new (m_buckets[current].m_valueStorage) V(static_cast<V&&>(*m_buckets[next].Value()));
                m_buckets[current].m_state = kOccupied;
                m_buckets[current].m_psl = m_buckets[next].m_psl - 1;

                m_buckets[next].Key()->~K();
                m_buckets[next].Value()->~V();
                m_buckets[next].m_state = kEmpty;

                current = next;
                next = (next + 1) & (m_capacity - 1);
            }
        }

        static constexpr size_t NextPowerOfTwo(size_t n)
        {
            if (n == 0)
            {
                return 1;
            }
            --n;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            n |= n >> 32;
            return n + 1;
        }

        comb::MemoryResource m_allocator;
        Bucket* m_buckets{nullptr};
        size_t m_capacity;
        size_t m_count;
    };
} // namespace wax