#pragma once

// Windows header macro conflict workaround
#ifdef GetObject
#undef GetObject
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <hive/core/assert.h>

namespace nectar
{
    /**
     * JSON value type enumeration
     */
    enum class JsonType : uint8_t
    {
        Null,
        Bool,
        Integer,
        Number,
        String,
        Array,
        Object
    };

    // Forward declarations
    template<typename Allocator>
    class JsonValue;

    template<typename Allocator>
    class JsonArray;

    template<typename Allocator>
    class JsonObject;

    /**
     * Fixed-capacity string for JSON keys and small strings
     *
     * Uses inline storage up to 63 bytes to avoid allocations
     * for typical JSON keys and short values.
     */
    class JsonString
    {
    public:
        static constexpr size_t kInlineCapacity = 63;

        constexpr JsonString() noexcept
            : size_{0}
        {
            data_[0] = '\0';
        }

        constexpr JsonString(const char* str) noexcept
            : size_{0}
        {
            if (str)
            {
                size_t len = 0;
                while (str[len] && len < kInlineCapacity)
                {
                    data_[len] = str[len];
                    ++len;
                }
                size_ = static_cast<uint8_t>(len);
                data_[len] = '\0';
            }
            else
            {
                data_[0] = '\0';
            }
        }

        constexpr JsonString(const char* str, size_t len) noexcept
            : size_{0}
        {
            size_t copy_len = len < kInlineCapacity ? len : kInlineCapacity;
            for (size_t i = 0; i < copy_len; ++i)
            {
                data_[i] = str[i];
            }
            data_[copy_len] = '\0';
            size_ = static_cast<uint8_t>(copy_len);
        }

        [[nodiscard]] constexpr const char* CStr() const noexcept { return data_; }
        [[nodiscard]] constexpr size_t Size() const noexcept { return size_; }
        [[nodiscard]] constexpr bool IsEmpty() const noexcept { return size_ == 0; }

        [[nodiscard]] constexpr bool operator==(const JsonString& other) const noexcept
        {
            if (size_ != other.size_) return false;
            for (size_t i = 0; i < size_; ++i)
            {
                if (data_[i] != other.data_[i]) return false;
            }
            return true;
        }

        [[nodiscard]] constexpr bool operator!=(const JsonString& other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr bool operator==(const char* str) const noexcept
        {
            if (!str) return size_ == 0;
            size_t i = 0;
            while (i < size_ && str[i])
            {
                if (data_[i] != str[i]) return false;
                ++i;
            }
            return i == size_ && str[i] == '\0';
        }

        [[nodiscard]] constexpr size_t Hash() const noexcept
        {
            // FNV-1a hash
            size_t hash = 14695981039346656037ULL;
            for (size_t i = 0; i < size_; ++i)
            {
                hash ^= static_cast<size_t>(data_[i]);
                hash *= 1099511628211ULL;
            }
            return hash;
        }

    private:
        char data_[kInlineCapacity + 1];
        uint8_t size_;
    };

    /**
     * JSON array - dynamic array of JsonValue
     *
     * Template on Allocator for Comb compatibility.
     */
    template<typename Allocator>
    class JsonArray
    {
    public:
        explicit JsonArray(Allocator& alloc) noexcept
            : allocator_{&alloc}
            , data_{nullptr}
            , size_{0}
            , capacity_{0}
        {}

        ~JsonArray()
        {
            Clear();
            if (data_ && allocator_)
            {
                allocator_->Deallocate(data_);
            }
        }

        JsonArray(const JsonArray&) = delete;
        JsonArray& operator=(const JsonArray&) = delete;

        JsonArray(JsonArray&& other) noexcept
            : allocator_{other.allocator_}
            , data_{other.data_}
            , size_{other.size_}
            , capacity_{other.capacity_}
        {
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }

        JsonArray& operator=(JsonArray&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                if (data_)
                {
                    allocator_->Deallocate(data_);
                }
                allocator_ = other.allocator_;
                data_ = other.data_;
                size_ = other.size_;
                capacity_ = other.capacity_;
                other.data_ = nullptr;
                other.size_ = 0;
                other.capacity_ = 0;
            }
            return *this;
        }

        void Push(JsonValue<Allocator>&& value) noexcept;

        void Clear() noexcept;

        [[nodiscard]] size_t Size() const noexcept { return size_; }
        [[nodiscard]] bool IsEmpty() const noexcept { return size_ == 0; }

        [[nodiscard]] JsonValue<Allocator>& operator[](size_t index) noexcept
        {
            hive::Assert(index < size_, "JsonArray index out of bounds");
            return data_[index];
        }

        [[nodiscard]] const JsonValue<Allocator>& operator[](size_t index) const noexcept
        {
            hive::Assert(index < size_, "JsonArray index out of bounds");
            return data_[index];
        }

        [[nodiscard]] JsonValue<Allocator>* begin() noexcept { return data_; }
        [[nodiscard]] JsonValue<Allocator>* end() noexcept { return data_ + size_; }
        [[nodiscard]] const JsonValue<Allocator>* begin() const noexcept { return data_; }
        [[nodiscard]] const JsonValue<Allocator>* end() const noexcept { return data_ + size_; }

    private:
        void Grow() noexcept;

        Allocator* allocator_;
        JsonValue<Allocator>* data_;
        size_t size_;
        size_t capacity_;
    };

    /**
     * JSON object key-value pair
     */
    template<typename Allocator>
    struct JsonKeyValue
    {
        JsonString key;
        JsonValue<Allocator> value;
    };

    /**
     * JSON object - collection of key-value pairs
     *
     * Uses simple linear storage for small objects (typical JSON).
     * Template on Allocator for Comb compatibility.
     */
    template<typename Allocator>
    class JsonObject
    {
    public:
        explicit JsonObject(Allocator& alloc) noexcept
            : allocator_{&alloc}
            , data_{nullptr}
            , size_{0}
            , capacity_{0}
        {}

        ~JsonObject()
        {
            Clear();
            if (data_ && allocator_)
            {
                allocator_->Deallocate(data_);
            }
        }

        JsonObject(const JsonObject&) = delete;
        JsonObject& operator=(const JsonObject&) = delete;

        JsonObject(JsonObject&& other) noexcept
            : allocator_{other.allocator_}
            , data_{other.data_}
            , size_{other.size_}
            , capacity_{other.capacity_}
        {
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }

        JsonObject& operator=(JsonObject&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                if (data_)
                {
                    allocator_->Deallocate(data_);
                }
                allocator_ = other.allocator_;
                data_ = other.data_;
                size_ = other.size_;
                capacity_ = other.capacity_;
                other.data_ = nullptr;
                other.size_ = 0;
                other.capacity_ = 0;
            }
            return *this;
        }

        void Set(const JsonString& key, JsonValue<Allocator>&& value) noexcept;
        void Set(const char* key, JsonValue<Allocator>&& value) noexcept;

        [[nodiscard]] JsonValue<Allocator>* Get(const JsonString& key) noexcept;
        [[nodiscard]] const JsonValue<Allocator>* Get(const JsonString& key) const noexcept;
        [[nodiscard]] JsonValue<Allocator>* Get(const char* key) noexcept;
        [[nodiscard]] const JsonValue<Allocator>* Get(const char* key) const noexcept;

        [[nodiscard]] bool Has(const JsonString& key) const noexcept;
        [[nodiscard]] bool Has(const char* key) const noexcept;

        void Clear() noexcept;

        [[nodiscard]] size_t Size() const noexcept { return size_; }
        [[nodiscard]] bool IsEmpty() const noexcept { return size_ == 0; }

        [[nodiscard]] JsonKeyValue<Allocator>* begin() noexcept { return data_; }
        [[nodiscard]] JsonKeyValue<Allocator>* end() noexcept { return data_ + size_; }
        [[nodiscard]] const JsonKeyValue<Allocator>* begin() const noexcept { return data_; }
        [[nodiscard]] const JsonKeyValue<Allocator>* end() const noexcept { return data_ + size_; }

    private:
        void Grow() noexcept;

        Allocator* allocator_;
        JsonKeyValue<Allocator>* data_;
        size_t size_;
        size_t capacity_;
    };

    /**
     * JSON value - tagged union for all JSON types
     *
     * Supports: null, bool, integer (int64), number (double),
     * string, array, and object.
     *
     * Template on Allocator for Comb compatibility. Arrays and
     * objects are heap-allocated using the provided allocator.
     *
     * Performance characteristics:
     * - Storage: 72 bytes (type + union data)
     * - Small strings inline (up to 63 chars)
     * - Objects/arrays heap-allocated
     *
     * Use cases:
     * - JSON parsing for .meta files
     * - Configuration data
     * - Serializable metadata
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{4096};
     *   nectar::JsonValue<comb::LinearAllocator> value{alloc};
     *   value.SetString("hello");
     *
     *   auto& obj = value.SetObject();
     *   obj.Set("key", nectar::JsonValue<comb::LinearAllocator>::MakeInt(alloc, 42));
     * @endcode
     */
    template<typename Allocator>
    class JsonValue
    {
    public:
        explicit JsonValue(Allocator& alloc) noexcept
            : allocator_{&alloc}
            , type_{JsonType::Null}
        {
            data_.integer = 0;
        }

        ~JsonValue()
        {
            Destroy();
        }

        JsonValue(const JsonValue&) = delete;
        JsonValue& operator=(const JsonValue&) = delete;

        JsonValue(JsonValue&& other) noexcept
            : allocator_{other.allocator_}
            , type_{other.type_}
            , data_{other.data_}
        {
            other.type_ = JsonType::Null;
            other.data_.integer = 0;
        }

        JsonValue& operator=(JsonValue&& other) noexcept
        {
            if (this != &other)
            {
                Destroy();
                allocator_ = other.allocator_;
                type_ = other.type_;
                data_ = other.data_;
                other.type_ = JsonType::Null;
                other.data_.integer = 0;
            }
            return *this;
        }

        // Static factory methods
        [[nodiscard]] static JsonValue MakeNull(Allocator& alloc) noexcept
        {
            return JsonValue{alloc};
        }

        [[nodiscard]] static JsonValue MakeBool(Allocator& alloc, bool value) noexcept
        {
            JsonValue v{alloc};
            v.SetBool(value);
            return v;
        }

        [[nodiscard]] static JsonValue MakeInt(Allocator& alloc, int64_t value) noexcept
        {
            JsonValue v{alloc};
            v.SetInt(value);
            return v;
        }

        [[nodiscard]] static JsonValue MakeNumber(Allocator& alloc, double value) noexcept
        {
            JsonValue v{alloc};
            v.SetNumber(value);
            return v;
        }

        [[nodiscard]] static JsonValue MakeString(Allocator& alloc, const char* str) noexcept
        {
            JsonValue v{alloc};
            v.SetString(str);
            return v;
        }

        [[nodiscard]] static JsonValue MakeString(Allocator& alloc, const char* str, size_t len) noexcept
        {
            JsonValue v{alloc};
            v.SetString(str, len);
            return v;
        }

        // Type accessors
        [[nodiscard]] JsonType Type() const noexcept { return type_; }
        [[nodiscard]] bool IsNull() const noexcept { return type_ == JsonType::Null; }
        [[nodiscard]] bool IsBool() const noexcept { return type_ == JsonType::Bool; }
        [[nodiscard]] bool IsInt() const noexcept { return type_ == JsonType::Integer; }
        [[nodiscard]] bool IsNumber() const noexcept { return type_ == JsonType::Number; }
        [[nodiscard]] bool IsString() const noexcept { return type_ == JsonType::String; }
        [[nodiscard]] bool IsArray() const noexcept { return type_ == JsonType::Array; }
        [[nodiscard]] bool IsObject() const noexcept { return type_ == JsonType::Object; }

        // Value getters
        [[nodiscard]] bool GetBool() const noexcept
        {
            hive::Assert(type_ == JsonType::Bool, "JsonValue is not a bool");
            return data_.boolean;
        }

        [[nodiscard]] int64_t GetInt() const noexcept
        {
            hive::Assert(type_ == JsonType::Integer, "JsonValue is not an integer");
            return data_.integer;
        }

        [[nodiscard]] double GetNumber() const noexcept
        {
            if (type_ == JsonType::Integer)
            {
                return static_cast<double>(data_.integer);
            }
            hive::Assert(type_ == JsonType::Number, "JsonValue is not a number");
            return data_.number;
        }

        [[nodiscard]] const JsonString& GetString() const noexcept
        {
            hive::Assert(type_ == JsonType::String, "JsonValue is not a string");
            return data_.string;
        }

        [[nodiscard]] JsonArray<Allocator>& GetArray() noexcept
        {
            hive::Assert(type_ == JsonType::Array, "JsonValue is not an array");
            return *data_.array;
        }

        [[nodiscard]] const JsonArray<Allocator>& GetArray() const noexcept
        {
            hive::Assert(type_ == JsonType::Array, "JsonValue is not an array");
            return *data_.array;
        }

        [[nodiscard]] JsonObject<Allocator>& GetObject() noexcept
        {
            hive::Assert(type_ == JsonType::Object, "JsonValue is not an object");
            return *data_.object;
        }

        [[nodiscard]] const JsonObject<Allocator>& GetObject() const noexcept
        {
            hive::Assert(type_ == JsonType::Object, "JsonValue is not an object");
            return *data_.object;
        }

        // Value setters
        void SetNull() noexcept
        {
            Destroy();
            type_ = JsonType::Null;
            data_.integer = 0;
        }

        void SetBool(bool value) noexcept
        {
            Destroy();
            type_ = JsonType::Bool;
            data_.boolean = value;
        }

        void SetInt(int64_t value) noexcept
        {
            Destroy();
            type_ = JsonType::Integer;
            data_.integer = value;
        }

        void SetNumber(double value) noexcept
        {
            Destroy();
            type_ = JsonType::Number;
            data_.number = value;
        }

        void SetString(const char* str) noexcept
        {
            Destroy();
            type_ = JsonType::String;
            new (&data_.string) JsonString{str};
        }

        void SetString(const char* str, size_t len) noexcept
        {
            Destroy();
            type_ = JsonType::String;
            new (&data_.string) JsonString{str, len};
        }

        JsonArray<Allocator>& SetArray() noexcept
        {
            Destroy();
            type_ = JsonType::Array;
            void* mem = allocator_->Allocate(sizeof(JsonArray<Allocator>), alignof(JsonArray<Allocator>));
            data_.array = new (mem) JsonArray<Allocator>{*allocator_};
            return *data_.array;
        }

        JsonObject<Allocator>& SetObject() noexcept
        {
            Destroy();
            type_ = JsonType::Object;
            void* mem = allocator_->Allocate(sizeof(JsonObject<Allocator>), alignof(JsonObject<Allocator>));
            data_.object = new (mem) JsonObject<Allocator>{*allocator_};
            return *data_.object;
        }

        // Convenience accessors for objects
        [[nodiscard]] JsonValue* operator[](const char* key) noexcept
        {
            if (type_ != JsonType::Object) return nullptr;
            return data_.object->Get(key);
        }

        [[nodiscard]] const JsonValue* operator[](const char* key) const noexcept
        {
            if (type_ != JsonType::Object) return nullptr;
            return data_.object->Get(key);
        }

        // Convenience accessors for arrays
        [[nodiscard]] JsonValue& operator[](size_t index) noexcept
        {
            hive::Assert(type_ == JsonType::Array, "JsonValue is not an array");
            return (*data_.array)[index];
        }

        [[nodiscard]] const JsonValue& operator[](size_t index) const noexcept
        {
            hive::Assert(type_ == JsonType::Array, "JsonValue is not an array");
            return (*data_.array)[index];
        }

    private:
        void Destroy() noexcept
        {
            switch (type_)
            {
                case JsonType::String:
                    data_.string.~JsonString();
                    break;
                case JsonType::Array:
                    data_.array->~JsonArray();
                    allocator_->Deallocate(data_.array);
                    break;
                case JsonType::Object:
                    data_.object->~JsonObject();
                    allocator_->Deallocate(data_.object);
                    break;
                default:
                    break;
            }
            type_ = JsonType::Null;
        }

        Allocator* allocator_;
        JsonType type_;

        union Data
        {
            bool boolean;
            int64_t integer;
            double number;
            JsonString string;
            JsonArray<Allocator>* array;
            JsonObject<Allocator>* object;

            Data() : integer{0} {}
            ~Data() {}
        } data_;
    };

    // Implementation of JsonArray methods
    template<typename Allocator>
    void JsonArray<Allocator>::Push(JsonValue<Allocator>&& value) noexcept
    {
        if (size_ >= capacity_)
        {
            Grow();
        }
        new (&data_[size_]) JsonValue<Allocator>{std::move(value)};
        ++size_;
    }

    template<typename Allocator>
    void JsonArray<Allocator>::Clear() noexcept
    {
        for (size_t i = 0; i < size_; ++i)
        {
            data_[i].~JsonValue();
        }
        size_ = 0;
    }

    template<typename Allocator>
    void JsonArray<Allocator>::Grow() noexcept
    {
        size_t new_capacity = capacity_ == 0 ? 4 : capacity_ * 2;
        void* mem = allocator_->Allocate(new_capacity * sizeof(JsonValue<Allocator>), alignof(JsonValue<Allocator>));
        auto* new_data = static_cast<JsonValue<Allocator>*>(mem);

        for (size_t i = 0; i < size_; ++i)
        {
            new (&new_data[i]) JsonValue<Allocator>{std::move(data_[i])};
            data_[i].~JsonValue();
        }

        if (data_)
        {
            allocator_->Deallocate(data_);
        }

        data_ = new_data;
        capacity_ = new_capacity;
    }

    // Implementation of JsonObject methods
    template<typename Allocator>
    void JsonObject<Allocator>::Set(const JsonString& key, JsonValue<Allocator>&& value) noexcept
    {
        // Check if key already exists
        for (size_t i = 0; i < size_; ++i)
        {
            if (data_[i].key == key)
            {
                data_[i].value = std::move(value);
                return;
            }
        }

        // Add new entry
        if (size_ >= capacity_)
        {
            Grow();
        }
        data_[size_].key = key;
        new (&data_[size_].value) JsonValue<Allocator>{std::move(value)};
        ++size_;
    }

    template<typename Allocator>
    void JsonObject<Allocator>::Set(const char* key, JsonValue<Allocator>&& value) noexcept
    {
        Set(JsonString{key}, std::move(value));
    }

    template<typename Allocator>
    JsonValue<Allocator>* JsonObject<Allocator>::Get(const JsonString& key) noexcept
    {
        for (size_t i = 0; i < size_; ++i)
        {
            if (data_[i].key == key)
            {
                return &data_[i].value;
            }
        }
        return nullptr;
    }

    template<typename Allocator>
    const JsonValue<Allocator>* JsonObject<Allocator>::Get(const JsonString& key) const noexcept
    {
        for (size_t i = 0; i < size_; ++i)
        {
            if (data_[i].key == key)
            {
                return &data_[i].value;
            }
        }
        return nullptr;
    }

    template<typename Allocator>
    JsonValue<Allocator>* JsonObject<Allocator>::Get(const char* key) noexcept
    {
        for (size_t i = 0; i < size_; ++i)
        {
            if (data_[i].key == key)
            {
                return &data_[i].value;
            }
        }
        return nullptr;
    }

    template<typename Allocator>
    const JsonValue<Allocator>* JsonObject<Allocator>::Get(const char* key) const noexcept
    {
        for (size_t i = 0; i < size_; ++i)
        {
            if (data_[i].key == key)
            {
                return &data_[i].value;
            }
        }
        return nullptr;
    }

    template<typename Allocator>
    bool JsonObject<Allocator>::Has(const JsonString& key) const noexcept
    {
        return Get(key) != nullptr;
    }

    template<typename Allocator>
    bool JsonObject<Allocator>::Has(const char* key) const noexcept
    {
        return Get(key) != nullptr;
    }

    template<typename Allocator>
    void JsonObject<Allocator>::Clear() noexcept
    {
        for (size_t i = 0; i < size_; ++i)
        {
            data_[i].value.~JsonValue();
        }
        size_ = 0;
    }

    template<typename Allocator>
    void JsonObject<Allocator>::Grow() noexcept
    {
        size_t new_capacity = capacity_ == 0 ? 4 : capacity_ * 2;
        void* mem = allocator_->Allocate(new_capacity * sizeof(JsonKeyValue<Allocator>), alignof(JsonKeyValue<Allocator>));
        auto* new_data = static_cast<JsonKeyValue<Allocator>*>(mem);

        for (size_t i = 0; i < size_; ++i)
        {
            new_data[i].key = data_[i].key;
            new (&new_data[i].value) JsonValue<Allocator>{std::move(data_[i].value)};
            data_[i].value.~JsonValue();
        }

        if (data_)
        {
            allocator_->Deallocate(data_);
        }

        data_ = new_data;
        capacity_ = new_capacity;
    }

}
