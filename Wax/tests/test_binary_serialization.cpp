#include <larvae/larvae.h>
#include <wax/serialization/binary_writer.h>
#include <wax/serialization/binary_reader.h>
#include <wax/serialization/byte_span.h>
#include <wax/serialization/byte_buffer.h>
#include <comb/linear_allocator.h>

namespace {

// ============================================================================
// ByteSpan Tests
// ============================================================================

auto bytespan_default = larvae::RegisterTest("ByteSpan", "DefaultConstruction", []() {
    wax::ByteSpan span;

    larvae::AssertNull(span.Data());
    larvae::AssertEqual(span.Size(), 0u);
    larvae::AssertTrue(span.IsEmpty());
});

auto bytespan_ptr_size = larvae::RegisterTest("ByteSpan", "ConstructFromPointerAndSize", []() {
    uint8_t data[] = {1, 2, 3, 4, 5};
    wax::ByteSpan span{data, 5};

    larvae::AssertEqual(span.Data(), static_cast<const uint8_t*>(data));
    larvae::AssertEqual(span.Size(), 5u);
    larvae::AssertFalse(span.IsEmpty());
});

auto bytespan_index = larvae::RegisterTest("ByteSpan", "IndexOperator", []() {
    uint8_t data[] = {10, 20, 30, 40};
    wax::ByteSpan span{data, 4};

    larvae::AssertEqual(span[0], static_cast<uint8_t>(10));
    larvae::AssertEqual(span[1], static_cast<uint8_t>(20));
    larvae::AssertEqual(span[2], static_cast<uint8_t>(30));
    larvae::AssertEqual(span[3], static_cast<uint8_t>(40));
});

auto bytespan_subspan = larvae::RegisterTest("ByteSpan", "Subspan", []() {
    uint8_t data[] = {1, 2, 3, 4, 5};
    wax::ByteSpan span{data, 5};

    auto sub = span.Subspan(2);
    larvae::AssertEqual(sub.Size(), 3u);
    larvae::AssertEqual(sub[0], static_cast<uint8_t>(3));

    auto sub2 = span.Subspan(1, 2);
    larvae::AssertEqual(sub2.Size(), 2u);
    larvae::AssertEqual(sub2[0], static_cast<uint8_t>(2));
    larvae::AssertEqual(sub2[1], static_cast<uint8_t>(3));
});

auto bytespan_first = larvae::RegisterTest("ByteSpan", "First", []() {
    uint8_t data[] = {1, 2, 3, 4, 5};
    wax::ByteSpan span{data, 5};

    auto first3 = span.First(3);
    larvae::AssertEqual(first3.Size(), 3u);
    larvae::AssertEqual(first3[0], static_cast<uint8_t>(1));
    larvae::AssertEqual(first3[2], static_cast<uint8_t>(3));
});

auto bytespan_last = larvae::RegisterTest("ByteSpan", "Last", []() {
    uint8_t data[] = {1, 2, 3, 4, 5};
    wax::ByteSpan span{data, 5};

    auto last2 = span.Last(2);
    larvae::AssertEqual(last2.Size(), 2u);
    larvae::AssertEqual(last2[0], static_cast<uint8_t>(4));
    larvae::AssertEqual(last2[1], static_cast<uint8_t>(5));
});

auto bytespan_iteration = larvae::RegisterTest("ByteSpan", "Iteration", []() {
    uint8_t data[] = {1, 2, 3};
    wax::ByteSpan span{data, 3};

    size_t sum = 0;
    for (auto byte : span)
    {
        sum += byte;
    }
    larvae::AssertEqual(sum, 6u);
});

// ============================================================================
// ByteBuffer Tests
// ============================================================================

auto bytebuffer_default = larvae::RegisterTest("ByteBuffer", "DefaultConstruction", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    larvae::AssertEqual(buffer.Size(), 0u);
    larvae::AssertTrue(buffer.IsEmpty());
});

auto bytebuffer_reserve = larvae::RegisterTest("ByteBuffer", "Reserve", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    buffer.Reserve(100);
    larvae::AssertTrue(buffer.Capacity() >= 100u);
    larvae::AssertEqual(buffer.Size(), 0u);
});

auto bytebuffer_append_byte = larvae::RegisterTest("ByteBuffer", "AppendByte", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    buffer.Append(static_cast<uint8_t>(42));
    buffer.Append(static_cast<uint8_t>(123));

    larvae::AssertEqual(buffer.Size(), 2u);
    larvae::AssertEqual(buffer[0], static_cast<uint8_t>(42));
    larvae::AssertEqual(buffer[1], static_cast<uint8_t>(123));
});

auto bytebuffer_append = larvae::RegisterTest("ByteBuffer", "Append", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    uint8_t data[] = {1, 2, 3, 4};
    buffer.Append(data, 4);

    larvae::AssertEqual(buffer.Size(), 4u);
    larvae::AssertEqual(buffer[0], static_cast<uint8_t>(1));
    larvae::AssertEqual(buffer[3], static_cast<uint8_t>(4));
});

auto bytebuffer_clear = larvae::RegisterTest("ByteBuffer", "Clear", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    buffer.Append(static_cast<uint8_t>(1));
    buffer.Append(static_cast<uint8_t>(2));
    buffer.Clear();

    larvae::AssertEqual(buffer.Size(), 0u);
    larvae::AssertTrue(buffer.IsEmpty());
});

auto bytebuffer_view = larvae::RegisterTest("ByteBuffer", "View", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    buffer.Append(static_cast<uint8_t>(10));
    buffer.Append(static_cast<uint8_t>(20));

    wax::ByteSpan view = buffer.View();
    larvae::AssertEqual(view.Size(), 2u);
    larvae::AssertEqual(view[0], static_cast<uint8_t>(10));
    larvae::AssertEqual(view[1], static_cast<uint8_t>(20));
});

// ============================================================================
// BinaryWriter Tests
// ============================================================================

auto writer_int8 = larvae::RegisterTest("BinaryWriter", "WriteInt8", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<int8_t>(-42);

    larvae::AssertEqual(writer.Size(), 1u);
    larvae::AssertEqual(static_cast<int8_t>(writer.View()[0]), static_cast<int8_t>(-42));
});

auto writer_int16 = larvae::RegisterTest("BinaryWriter", "WriteInt16", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<int16_t>(0x1234);

    larvae::AssertEqual(writer.Size(), 2u);
    // Little-endian
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(0x34));
    larvae::AssertEqual(writer.View()[1], static_cast<uint8_t>(0x12));
});

auto writer_int32 = larvae::RegisterTest("BinaryWriter", "WriteInt32", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<int32_t>(0x12345678);

    larvae::AssertEqual(writer.Size(), 4u);
    // Little-endian
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(0x78));
    larvae::AssertEqual(writer.View()[1], static_cast<uint8_t>(0x56));
    larvae::AssertEqual(writer.View()[2], static_cast<uint8_t>(0x34));
    larvae::AssertEqual(writer.View()[3], static_cast<uint8_t>(0x12));
});

auto writer_int64 = larvae::RegisterTest("BinaryWriter", "WriteInt64", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<int64_t>(0x123456789ABCDEF0LL);

    larvae::AssertEqual(writer.Size(), 8u);
});

auto writer_float = larvae::RegisterTest("BinaryWriter", "WriteFloat", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<float>(3.14159f);

    larvae::AssertEqual(writer.Size(), 4u);
});

auto writer_double = larvae::RegisterTest("BinaryWriter", "WriteDouble", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<double>(3.141592653589793);

    larvae::AssertEqual(writer.Size(), 8u);
});

auto writer_bytes = larvae::RegisterTest("BinaryWriter", "WriteBytes", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    writer.WriteBytes(data, 4);

    larvae::AssertEqual(writer.Size(), 4u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(0xDE));
    larvae::AssertEqual(writer.View()[3], static_cast<uint8_t>(0xEF));
});

auto writer_multiple = larvae::RegisterTest("BinaryWriter", "WriteMultipleValues", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<uint32_t>(0x12345678);
    writer.Write<uint16_t>(0xABCD);
    writer.Write<uint8_t>(0xFF);

    larvae::AssertEqual(writer.Size(), 7u);
});

auto writer_clear = larvae::RegisterTest("BinaryWriter", "Clear", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<uint32_t>(123);
    writer.Clear();

    larvae::AssertEqual(writer.Size(), 0u);
});

auto writer_varint = larvae::RegisterTest("BinaryWriter", "WriteVarInt", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    // Small value (1 byte)
    writer.WriteVarInt(0);
    larvae::AssertEqual(writer.Size(), 1u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(0));

    writer.Clear();

    // Value < 128 (1 byte)
    writer.WriteVarInt(127);
    larvae::AssertEqual(writer.Size(), 1u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(127));

    writer.Clear();

    // Value >= 128 (2 bytes)
    writer.WriteVarInt(128);
    larvae::AssertEqual(writer.Size(), 2u);

    writer.Clear();

    // Large value
    writer.WriteVarInt(300);
    larvae::AssertEqual(writer.Size(), 2u);
});

auto writer_string = larvae::RegisterTest("BinaryWriter", "WriteString", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.WriteString("Hello", 5);

    // 4 bytes length + 5 bytes data
    larvae::AssertEqual(writer.Size(), 9u);
});

// ============================================================================
// BinaryReader Tests
// ============================================================================

auto reader_default = larvae::RegisterTest("BinaryReader", "DefaultConstruction", []() {
    wax::BinaryReader reader;

    larvae::AssertEqual(reader.Size(), 0u);
    larvae::AssertEqual(reader.Position(), 0u);
    larvae::AssertTrue(reader.IsEof());
});

auto reader_ptr_size = larvae::RegisterTest("BinaryReader", "ConstructFromPointerAndSize", []() {
    uint8_t data[] = {1, 2, 3, 4};
    wax::BinaryReader reader{data, 4};

    larvae::AssertEqual(reader.Size(), 4u);
    larvae::AssertEqual(reader.Position(), 0u);
    larvae::AssertFalse(reader.IsEof());
});

auto reader_int8 = larvae::RegisterTest("BinaryReader", "ReadInt8", []() {
    uint8_t data[] = {0xFE};  // -2 as signed
    wax::BinaryReader reader{data, 1};

    int8_t value = reader.Read<int8_t>();
    larvae::AssertEqual(value, static_cast<int8_t>(-2));
    larvae::AssertTrue(reader.IsEof());
});

auto reader_int16 = larvae::RegisterTest("BinaryReader", "ReadInt16", []() {
    uint8_t data[] = {0x34, 0x12};  // Little-endian 0x1234
    wax::BinaryReader reader{data, 2};

    int16_t value = reader.Read<int16_t>();
    larvae::AssertEqual(value, static_cast<int16_t>(0x1234));
});

auto reader_int32 = larvae::RegisterTest("BinaryReader", "ReadInt32", []() {
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12};  // Little-endian
    wax::BinaryReader reader{data, 4};

    int32_t value = reader.Read<int32_t>();
    larvae::AssertEqual(value, static_cast<int32_t>(0x12345678));
});

auto reader_int64 = larvae::RegisterTest("BinaryReader", "ReadInt64", []() {
    uint8_t data[] = {0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12};
    wax::BinaryReader reader{data, 8};

    int64_t value = reader.Read<int64_t>();
    larvae::AssertEqual(value, static_cast<int64_t>(0x123456789ABCDEF0LL));
});

auto reader_float = larvae::RegisterTest("BinaryReader", "ReadFloat", []() {
    float original = 3.14159f;
    wax::BinaryReader reader{&original, sizeof(float)};

    float value = reader.Read<float>();
    larvae::AssertTrue(value > 3.14f && value < 3.15f);
});

auto reader_double = larvae::RegisterTest("BinaryReader", "ReadDouble", []() {
    double original = 3.141592653589793;
    wax::BinaryReader reader{&original, sizeof(double)};

    double value = reader.Read<double>();
    larvae::AssertTrue(value > 3.14159 && value < 3.14160);
});

auto reader_bytes = larvae::RegisterTest("BinaryReader", "ReadBytes", []() {
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    wax::BinaryReader reader{data, 4};

    uint8_t buffer[4];
    reader.ReadBytes(buffer, 4);

    larvae::AssertEqual(buffer[0], static_cast<uint8_t>(0xDE));
    larvae::AssertEqual(buffer[3], static_cast<uint8_t>(0xEF));
});

auto reader_bytes_span = larvae::RegisterTest("BinaryReader", "ReadBytesAsSpan", []() {
    uint8_t data[] = {1, 2, 3, 4, 5};
    wax::BinaryReader reader{data, 5};

    wax::ByteSpan span = reader.ReadBytes(3);
    larvae::AssertEqual(span.Size(), 3u);
    larvae::AssertEqual(span[0], static_cast<uint8_t>(1));
    larvae::AssertEqual(span[2], static_cast<uint8_t>(3));
    larvae::AssertEqual(reader.Position(), 3u);
});

auto reader_multiple = larvae::RegisterTest("BinaryReader", "ReadMultipleValues", []() {
    uint8_t data[] = {
        0x78, 0x56, 0x34, 0x12,  // uint32_t
        0xCD, 0xAB,              // uint16_t
        0xFF                      // uint8_t
    };
    wax::BinaryReader reader{data, 7};

    larvae::AssertEqual(reader.Read<uint32_t>(), 0x12345678u);
    larvae::AssertEqual(reader.Read<uint16_t>(), static_cast<uint16_t>(0xABCD));
    larvae::AssertEqual(reader.Read<uint8_t>(), static_cast<uint8_t>(0xFF));
    larvae::AssertTrue(reader.IsEof());
});

auto reader_skip = larvae::RegisterTest("BinaryReader", "Skip", []() {
    uint8_t data[] = {1, 2, 3, 4, 5};
    wax::BinaryReader reader{data, 5};

    reader.Skip(2);
    larvae::AssertEqual(reader.Position(), 2u);
    larvae::AssertEqual(reader.Read<uint8_t>(), static_cast<uint8_t>(3));
});

auto reader_seek = larvae::RegisterTest("BinaryReader", "Seek", []() {
    uint8_t data[] = {10, 20, 30, 40, 50};
    wax::BinaryReader reader{data, 5};

    reader.Seek(3);
    larvae::AssertEqual(reader.Position(), 3u);
    larvae::AssertEqual(reader.Read<uint8_t>(), static_cast<uint8_t>(40));

    reader.Seek(0);
    larvae::AssertEqual(reader.Read<uint8_t>(), static_cast<uint8_t>(10));
});

auto reader_remaining = larvae::RegisterTest("BinaryReader", "Remaining", []() {
    uint8_t data[] = {1, 2, 3, 4, 5};
    wax::BinaryReader reader{data, 5};

    larvae::AssertEqual(reader.Remaining(), 5u);
    reader.Skip(2);
    larvae::AssertEqual(reader.Remaining(), 3u);
});

auto reader_peek = larvae::RegisterTest("BinaryReader", "Peek", []() {
    uint8_t data[] = {42, 123};
    wax::BinaryReader reader{data, 2};

    larvae::AssertEqual(reader.Peek(), static_cast<uint8_t>(42));
    larvae::AssertEqual(reader.Position(), 0u);  // Position unchanged
    larvae::AssertEqual(reader.Peek(), static_cast<uint8_t>(42));  // Still same value
});

auto reader_tryread = larvae::RegisterTest("BinaryReader", "TryRead", []() {
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
    wax::BinaryReader reader{data, 4};

    uint32_t value;
    larvae::AssertTrue(reader.TryRead(value));
    larvae::AssertEqual(value, 0x12345678u);

    // No more data
    larvae::AssertFalse(reader.TryRead(value));
});

auto reader_tryskip = larvae::RegisterTest("BinaryReader", "TrySkip", []() {
    uint8_t data[] = {1, 2, 3};
    wax::BinaryReader reader{data, 3};

    larvae::AssertTrue(reader.TrySkip(2));
    larvae::AssertEqual(reader.Position(), 2u);

    larvae::AssertFalse(reader.TrySkip(5));  // Not enough data
    larvae::AssertEqual(reader.Position(), 2u);  // Position unchanged
});

auto reader_varint = larvae::RegisterTest("BinaryReader", "ReadVarInt", []() {
    // Single byte: 127
    uint8_t data1[] = {127};
    wax::BinaryReader reader1{data1, 1};
    larvae::AssertEqual(reader1.ReadVarInt(), 127u);

    // Two bytes: 128
    uint8_t data2[] = {0x80, 0x01};
    wax::BinaryReader reader2{data2, 2};
    larvae::AssertEqual(reader2.ReadVarInt(), 128u);

    // Two bytes: 300
    uint8_t data3[] = {0xAC, 0x02};
    wax::BinaryReader reader3{data3, 2};
    larvae::AssertEqual(reader3.ReadVarInt(), 300u);
});

auto reader_string = larvae::RegisterTest("BinaryReader", "ReadString", []() {
    // Length-prefixed string: "Hello"
    uint8_t data[] = {
        5, 0, 0, 0,           // Length (little-endian uint32)
        'H', 'e', 'l', 'l', 'o'
    };
    wax::BinaryReader reader{data, sizeof(data)};

    wax::ByteSpan str = reader.ReadString();
    larvae::AssertEqual(str.Size(), 5u);
    larvae::AssertEqual(str[0], static_cast<uint8_t>('H'));
    larvae::AssertEqual(str[4], static_cast<uint8_t>('o'));
});

auto reader_remaining_view = larvae::RegisterTest("BinaryReader", "RemainingView", []() {
    uint8_t data[] = {1, 2, 3, 4, 5};
    wax::BinaryReader reader{data, 5};

    reader.Skip(2);
    wax::ByteSpan remaining = reader.RemainingView();
    larvae::AssertEqual(remaining.Size(), 3u);
    larvae::AssertEqual(remaining[0], static_cast<uint8_t>(3));
});

// ============================================================================
// Round-trip Tests (Writer -> Reader)
// ============================================================================

auto roundtrip_int8 = larvae::RegisterTest("BinarySerialization", "RoundTripInt8", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<int8_t>(-100);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.Read<int8_t>(), static_cast<int8_t>(-100));
});

auto roundtrip_int16 = larvae::RegisterTest("BinarySerialization", "RoundTripInt16", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<int16_t>(-12345);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.Read<int16_t>(), static_cast<int16_t>(-12345));
});

auto roundtrip_int32 = larvae::RegisterTest("BinarySerialization", "RoundTripInt32", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<int32_t>(-123456789);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.Read<int32_t>(), -123456789);
});

auto roundtrip_int64 = larvae::RegisterTest("BinarySerialization", "RoundTripInt64", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<int64_t>(-1234567890123456789LL);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.Read<int64_t>(), -1234567890123456789LL);
});

auto roundtrip_unsigned = larvae::RegisterTest("BinarySerialization", "RoundTripUnsigned", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<uint8_t>(255);
    writer.Write<uint16_t>(65535);
    writer.Write<uint32_t>(0xFFFFFFFF);
    writer.Write<uint64_t>(0xFFFFFFFFFFFFFFFFULL);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.Read<uint8_t>(), static_cast<uint8_t>(255));
    larvae::AssertEqual(reader.Read<uint16_t>(), static_cast<uint16_t>(65535));
    larvae::AssertEqual(reader.Read<uint32_t>(), 0xFFFFFFFFu);
    larvae::AssertEqual(reader.Read<uint64_t>(), 0xFFFFFFFFFFFFFFFFULL);
});

auto roundtrip_float = larvae::RegisterTest("BinarySerialization", "RoundTripFloat", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<float>(3.14159f);
    writer.Write<float>(-123.456f);
    writer.Write<float>(0.0f);

    wax::BinaryReader reader{writer.View()};

    float v1 = reader.Read<float>();
    float v2 = reader.Read<float>();
    float v3 = reader.Read<float>();

    larvae::AssertTrue(v1 > 3.14f && v1 < 3.15f);
    larvae::AssertTrue(v2 > -123.5f && v2 < -123.4f);
    larvae::AssertTrue(v3 == 0.0f);
});

auto roundtrip_double = larvae::RegisterTest("BinarySerialization", "RoundTripDouble", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<double>(3.141592653589793);
    writer.Write<double>(-2.718281828459045);

    wax::BinaryReader reader{writer.View()};

    double v1 = reader.Read<double>();
    double v2 = reader.Read<double>();

    larvae::AssertTrue(v1 > 3.14159265 && v1 < 3.14159266);
    larvae::AssertTrue(v2 > -2.71828183 && v2 < -2.71828182);
});

auto roundtrip_varint = larvae::RegisterTest("BinarySerialization", "RoundTripVarInt", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.WriteVarInt(0);
    writer.WriteVarInt(127);
    writer.WriteVarInt(128);
    writer.WriteVarInt(300);
    writer.WriteVarInt(16384);
    writer.WriteVarInt(0xFFFFFFFFFFFFFFFFULL);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.ReadVarInt(), 0u);
    larvae::AssertEqual(reader.ReadVarInt(), 127u);
    larvae::AssertEqual(reader.ReadVarInt(), 128u);
    larvae::AssertEqual(reader.ReadVarInt(), 300u);
    larvae::AssertEqual(reader.ReadVarInt(), 16384u);
    larvae::AssertEqual(reader.ReadVarInt(), 0xFFFFFFFFFFFFFFFFULL);
});

auto roundtrip_mixed = larvae::RegisterTest("BinarySerialization", "RoundTripMixedTypes", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<uint32_t>(0xDEADBEEF);
    writer.Write<float>(42.5f);
    writer.Write<int16_t>(-1000);
    writer.Write<uint8_t>(255);
    writer.Write<double>(123.456);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.Read<uint32_t>(), 0xDEADBEEFu);

    float f = reader.Read<float>();
    larvae::AssertTrue(f > 42.4f && f < 42.6f);

    larvae::AssertEqual(reader.Read<int16_t>(), static_cast<int16_t>(-1000));
    larvae::AssertEqual(reader.Read<uint8_t>(), static_cast<uint8_t>(255));

    double d = reader.Read<double>();
    larvae::AssertTrue(d > 123.455 && d < 123.457);
});

auto roundtrip_struct = larvae::RegisterTest("BinarySerialization", "RoundTripStruct", []() {
    struct TestStruct
    {
        int32_t x;
        int32_t y;
        float z;
    };

    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    TestStruct original{100, -200, 3.5f};
    writer.WriteBytes(&original, sizeof(TestStruct));

    wax::BinaryReader reader{writer.View()};
    TestStruct loaded = reader.Read<TestStruct>();

    larvae::AssertEqual(loaded.x, 100);
    larvae::AssertEqual(loaded.y, -200);
    larvae::AssertTrue(loaded.z > 3.4f && loaded.z < 3.6f);
});

// ============================================================================
// ByteSpan Additional Tests
// ============================================================================

auto bytespan_at = larvae::RegisterTest("ByteSpan", "At", []() {
    uint8_t data[] = {10, 20, 30};
    wax::ByteSpan span{data, 3};

    larvae::AssertEqual(span.At(0), static_cast<uint8_t>(10));
    larvae::AssertEqual(span.At(1), static_cast<uint8_t>(20));
    larvae::AssertEqual(span.At(2), static_cast<uint8_t>(30));
});

auto bytespan_read = larvae::RegisterTest("ByteSpan", "ReadTyped", []() {
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12, 0xCD, 0xAB};
    wax::ByteSpan span{data, 6};

    uint32_t val32 = span.Read<uint32_t>(0);
    larvae::AssertEqual(val32, 0x12345678u);

    uint16_t val16 = span.Read<uint16_t>(4);
    larvae::AssertEqual(val16, static_cast<uint16_t>(0xABCD));
});

auto bytespan_tryread = larvae::RegisterTest("ByteSpan", "TryRead", []() {
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
    wax::ByteSpan span{data, 4};

    uint32_t val32;
    larvae::AssertTrue(span.TryRead(0, val32));
    larvae::AssertEqual(val32, 0x12345678u);

    // Out of bounds
    uint32_t overflow;
    larvae::AssertFalse(span.TryRead(2, overflow));

    // Exact end
    uint16_t val16;
    larvae::AssertTrue(span.TryRead(2, val16));
    larvae::AssertEqual(val16, static_cast<uint16_t>(0x1234));
});

auto bytespan_equality = larvae::RegisterTest("ByteSpan", "Equality", []() {
    uint8_t data1[] = {1, 2, 3};
    uint8_t data2[] = {1, 2, 3};
    uint8_t data3[] = {1, 2, 4};
    uint8_t data4[] = {1, 2};

    wax::ByteSpan span1{data1, 3};
    wax::ByteSpan span2{data2, 3};
    wax::ByteSpan span3{data3, 3};
    wax::ByteSpan span4{data4, 2};
    wax::ByteSpan span_self{data1, 3};

    larvae::AssertTrue(span1 == span2);    // Same content, different buffer
    larvae::AssertFalse(span1 == span3);   // Different content
    larvae::AssertFalse(span1 == span4);   // Different size
    larvae::AssertTrue(span1 == span_self); // Same pointer

    wax::ByteSpan empty1;
    wax::ByteSpan empty2;
    larvae::AssertTrue(empty1 == empty2);  // Both empty
});

auto bytespan_array_ctor = larvae::RegisterTest("ByteSpan", "ArrayConstructor", []() {
    uint8_t data[] = {5, 10, 15, 20};
    wax::ByteSpan span{data};

    larvae::AssertEqual(span.Size(), 4u);
    larvae::AssertEqual(span[0], static_cast<uint8_t>(5));
    larvae::AssertEqual(span[3], static_cast<uint8_t>(20));
});

// ============================================================================
// ByteBuffer Additional Tests
// ============================================================================

auto bytebuffer_append_typed = larvae::RegisterTest("ByteBuffer", "AppendTyped", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    uint32_t val = 0x12345678;
    buffer.Append(val);

    larvae::AssertEqual(buffer.Size(), 4u);
    larvae::AssertEqual(buffer[0], static_cast<uint8_t>(0x78));
    larvae::AssertEqual(buffer[1], static_cast<uint8_t>(0x56));
    larvae::AssertEqual(buffer[2], static_cast<uint8_t>(0x34));
    larvae::AssertEqual(buffer[3], static_cast<uint8_t>(0x12));
});

auto bytebuffer_append_span = larvae::RegisterTest("ByteBuffer", "AppendByteSpan", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    uint8_t data[] = {10, 20, 30};
    wax::ByteSpan span{data, 3};

    buffer.Append(span);

    larvae::AssertEqual(buffer.Size(), 3u);
    larvae::AssertEqual(buffer[0], static_cast<uint8_t>(10));
    larvae::AssertEqual(buffer[1], static_cast<uint8_t>(20));
    larvae::AssertEqual(buffer[2], static_cast<uint8_t>(30));
});

auto bytebuffer_resize = larvae::RegisterTest("ByteBuffer", "Resize", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    buffer.Append(static_cast<uint8_t>(1));
    buffer.Append(static_cast<uint8_t>(2));
    buffer.Append(static_cast<uint8_t>(3));

    buffer.Resize(5);
    larvae::AssertEqual(buffer.Size(), 5u);
    larvae::AssertEqual(buffer[0], static_cast<uint8_t>(1));
    larvae::AssertEqual(buffer[1], static_cast<uint8_t>(2));
    larvae::AssertEqual(buffer[2], static_cast<uint8_t>(3));

    buffer.Resize(1);
    larvae::AssertEqual(buffer.Size(), 1u);
    larvae::AssertEqual(buffer[0], static_cast<uint8_t>(1));
});

auto bytebuffer_view_offset = larvae::RegisterTest("ByteBuffer", "ViewWithOffset", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    buffer.Append(static_cast<uint8_t>(10));
    buffer.Append(static_cast<uint8_t>(20));
    buffer.Append(static_cast<uint8_t>(30));
    buffer.Append(static_cast<uint8_t>(40));

    wax::ByteSpan sub = buffer.View(1, 2);
    larvae::AssertEqual(sub.Size(), 2u);
    larvae::AssertEqual(sub[0], static_cast<uint8_t>(20));
    larvae::AssertEqual(sub[1], static_cast<uint8_t>(30));
});

auto bytebuffer_mutable = larvae::RegisterTest("ByteBuffer", "MutableAccess", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    buffer.Append(static_cast<uint8_t>(1));
    buffer.Append(static_cast<uint8_t>(2));
    buffer.Append(static_cast<uint8_t>(3));

    // Mutable indexing
    buffer[1] = 42;
    larvae::AssertEqual(buffer[1], static_cast<uint8_t>(42));

    // Mutable Data()
    uint8_t* ptr = buffer.Data();
    ptr[0] = 99;
    larvae::AssertEqual(buffer[0], static_cast<uint8_t>(99));
});

auto bytebuffer_iterators = larvae::RegisterTest("ByteBuffer", "Iterators", []() {
    comb::LinearAllocator alloc{1024};
    wax::ByteBuffer<comb::LinearAllocator> buffer{alloc};

    buffer.Append(static_cast<uint8_t>(1));
    buffer.Append(static_cast<uint8_t>(2));
    buffer.Append(static_cast<uint8_t>(3));

    // Const iteration
    size_t sum = 0;
    for (uint8_t b : buffer)
    {
        sum += b;
    }
    larvae::AssertEqual(sum, 6u);

    // Mutable iteration
    for (uint8_t& b : buffer)
    {
        b *= 10;
    }
    larvae::AssertEqual(buffer[0], static_cast<uint8_t>(10));
    larvae::AssertEqual(buffer[1], static_cast<uint8_t>(20));
    larvae::AssertEqual(buffer[2], static_cast<uint8_t>(30));
});

// ============================================================================
// BinaryWriter Additional Tests
// ============================================================================

auto writer_varint_signed = larvae::RegisterTest("BinaryWriter", "WriteVarIntSigned", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    // 0 -> ZigZag 0 -> 1 byte
    writer.WriteVarIntSigned(0);
    larvae::AssertEqual(writer.Size(), 1u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(0));

    writer.Clear();

    // -1 -> ZigZag 1 -> 1 byte
    writer.WriteVarIntSigned(-1);
    larvae::AssertEqual(writer.Size(), 1u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(1));

    writer.Clear();

    // 1 -> ZigZag 2 -> 1 byte
    writer.WriteVarIntSigned(1);
    larvae::AssertEqual(writer.Size(), 1u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(2));
});

auto writer_stringz = larvae::RegisterTest("BinaryWriter", "WriteStringZ", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.WriteStringZ("Hi");

    // "Hi" + null = 3 bytes
    larvae::AssertEqual(writer.Size(), 3u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>('H'));
    larvae::AssertEqual(writer.View()[1], static_cast<uint8_t>('i'));
    larvae::AssertEqual(writer.View()[2], static_cast<uint8_t>(0));
});

auto writer_stringz_null = larvae::RegisterTest("BinaryWriter", "WriteStringZNull", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.WriteStringZ(nullptr);

    larvae::AssertEqual(writer.Size(), 1u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(0));
});

auto writer_padding = larvae::RegisterTest("BinaryWriter", "WritePadding", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<uint8_t>(0xFF);
    writer.WritePadding(3);

    larvae::AssertEqual(writer.Size(), 4u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(0xFF));
    larvae::AssertEqual(writer.View()[1], static_cast<uint8_t>(0));
    larvae::AssertEqual(writer.View()[2], static_cast<uint8_t>(0));
    larvae::AssertEqual(writer.View()[3], static_cast<uint8_t>(0));
});

auto writer_alignment = larvae::RegisterTest("BinaryWriter", "WriteAlignment", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<uint8_t>(0xFF);         // 1 byte
    writer.WriteAlignment(4);            // Pad to 4-byte boundary: 3 zeros

    larvae::AssertEqual(writer.Size(), 4u);

    writer.WriteAlignment(4);            // Already aligned: no change
    larvae::AssertEqual(writer.Size(), 4u);
});

auto writer_bytes_span = larvae::RegisterTest("BinaryWriter", "WriteBytesSpan", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    wax::ByteSpan span{data, 3};

    writer.WriteBytes(span);

    larvae::AssertEqual(writer.Size(), 3u);
    larvae::AssertEqual(writer.View()[0], static_cast<uint8_t>(0xAA));
    larvae::AssertEqual(writer.View()[1], static_cast<uint8_t>(0xBB));
    larvae::AssertEqual(writer.View()[2], static_cast<uint8_t>(0xCC));
});

auto writer_string_cstr = larvae::RegisterTest("BinaryWriter", "WriteStringCStr", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.WriteString("Test");

    // uint32 length (4) + "Test" (4) = 8 bytes
    larvae::AssertEqual(writer.Size(), 8u);

    wax::BinaryReader reader{writer.View()};
    uint32_t len = reader.Read<uint32_t>();
    larvae::AssertEqual(len, 4u);

    auto str = reader.ReadBytes(4);
    larvae::AssertEqual(str[0], static_cast<uint8_t>('T'));
    larvae::AssertEqual(str[3], static_cast<uint8_t>('t'));
});

auto writer_string_null = larvae::RegisterTest("BinaryWriter", "WriteStringNull", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.WriteString(static_cast<const char*>(nullptr));

    // Just 4 bytes for length (0)
    larvae::AssertEqual(writer.Size(), 4u);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.Read<uint32_t>(), 0u);
});

// ============================================================================
// BinaryReader Additional Tests
// ============================================================================

auto reader_varint_signed = larvae::RegisterTest("BinaryReader", "ReadVarIntSigned", []() {
    // ZigZag: 0 -> 0, 1 -> -1, 2 -> 1, 3 -> -2, 4 -> 2
    uint8_t data[] = {0, 1, 2, 3, 4};
    wax::BinaryReader reader{data, sizeof(data)};

    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(0));
    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(-1));
    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(1));
    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(-2));
    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(2));
});

auto reader_stringz = larvae::RegisterTest("BinaryReader", "ReadStringZ", []() {
    uint8_t data[] = {'H', 'i', 0, 'X'};
    wax::BinaryReader reader{data, sizeof(data)};

    wax::ByteSpan str = reader.ReadStringZ();

    // Returns "Hi\0" (including null terminator)
    larvae::AssertEqual(str.Size(), 3u);
    larvae::AssertEqual(str[0], static_cast<uint8_t>('H'));
    larvae::AssertEqual(str[1], static_cast<uint8_t>('i'));
    larvae::AssertEqual(str[2], static_cast<uint8_t>(0));

    // Position advanced past null
    larvae::AssertEqual(reader.Position(), 3u);
    larvae::AssertEqual(reader.Read<uint8_t>(), static_cast<uint8_t>('X'));
});

auto reader_tryvarint = larvae::RegisterTest("BinaryReader", "TryReadVarInt", []() {
    // Valid: 300 = 0xAC 0x02
    uint8_t data[] = {0xAC, 0x02};
    wax::BinaryReader reader{data, 2};

    uint64_t value;
    larvae::AssertTrue(reader.TryReadVarInt(value));
    larvae::AssertEqual(value, 300u);
    larvae::AssertTrue(reader.IsEof());
});

auto reader_tryvarint_incomplete = larvae::RegisterTest("BinaryReader", "TryReadVarIntIncomplete", []() {
    // Incomplete VarInt: 0x80 means "more bytes follow" but there are none
    uint8_t data[] = {0x80};
    wax::BinaryReader reader{data, 1};

    uint64_t value;
    larvae::AssertFalse(reader.TryReadVarInt(value));
    larvae::AssertEqual(reader.Position(), 0u); // Position rolled back
});

auto reader_trypeek = larvae::RegisterTest("BinaryReader", "TryPeek", []() {
    uint8_t data[] = {42};
    wax::BinaryReader reader{data, 1};

    uint8_t out;
    larvae::AssertTrue(reader.TryPeek(out));
    larvae::AssertEqual(out, static_cast<uint8_t>(42));
    larvae::AssertEqual(reader.Position(), 0u); // Not advanced

    reader.Skip(1);
    larvae::AssertFalse(reader.TryPeek(out)); // EOF
});

auto reader_bytespan_ctor = larvae::RegisterTest("BinaryReader", "ByteSpanConstructor", []() {
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
    wax::ByteSpan span{data, 4};
    wax::BinaryReader reader{span};

    larvae::AssertEqual(reader.Size(), 4u);
    larvae::AssertEqual(reader.Read<uint32_t>(), 0x12345678u);
});

auto reader_tryreadbytes = larvae::RegisterTest("BinaryReader", "TryReadBytes", []() {
    uint8_t data[] = {1, 2, 3, 4, 5};
    wax::BinaryReader reader{data, 5};

    wax::ByteSpan out;

    larvae::AssertTrue(reader.TryReadBytes(3, out));
    larvae::AssertEqual(out.Size(), 3u);
    larvae::AssertEqual(out[0], static_cast<uint8_t>(1));
    larvae::AssertEqual(out[2], static_cast<uint8_t>(3));

    // Not enough remaining
    larvae::AssertFalse(reader.TryReadBytes(5, out));
    larvae::AssertTrue(out.IsEmpty());
});

auto reader_read_out = larvae::RegisterTest("BinaryReader", "ReadOutputParam", []() {
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
    wax::BinaryReader reader{data, 4};

    uint32_t value = 0;
    reader.Read(value);
    larvae::AssertEqual(value, 0x12345678u);
});

// ============================================================================
// Additional Round-trip Tests
// ============================================================================

auto roundtrip_varint_signed = larvae::RegisterTest("BinarySerialization", "RoundTripVarIntSigned", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.WriteVarIntSigned(0);
    writer.WriteVarIntSigned(1);
    writer.WriteVarIntSigned(-1);
    writer.WriteVarIntSigned(100);
    writer.WriteVarIntSigned(-100);
    writer.WriteVarIntSigned(INT64_MAX);
    writer.WriteVarIntSigned(INT64_MIN);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(0));
    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(1));
    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(-1));
    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(100));
    larvae::AssertEqual(reader.ReadVarIntSigned(), static_cast<int64_t>(-100));
    larvae::AssertEqual(reader.ReadVarIntSigned(), INT64_MAX);
    larvae::AssertEqual(reader.ReadVarIntSigned(), INT64_MIN);
});

auto roundtrip_stringz = larvae::RegisterTest("BinarySerialization", "RoundTripStringZ", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.WriteStringZ("Hello");
    writer.WriteStringZ("World");

    wax::BinaryReader reader{writer.View()};

    auto str1 = reader.ReadStringZ();
    larvae::AssertEqual(str1.Size(), 6u); // "Hello\0"
    larvae::AssertEqual(str1[0], static_cast<uint8_t>('H'));
    larvae::AssertEqual(str1[4], static_cast<uint8_t>('o'));
    larvae::AssertEqual(str1[5], static_cast<uint8_t>(0));

    auto str2 = reader.ReadStringZ();
    larvae::AssertEqual(str2.Size(), 6u); // "World\0"
    larvae::AssertEqual(str2[0], static_cast<uint8_t>('W'));
});

auto roundtrip_string_cstr = larvae::RegisterTest("BinarySerialization", "RoundTripStringCStr", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.WriteString("Test");

    wax::BinaryReader reader{writer.View()};
    auto str = reader.ReadString();

    larvae::AssertEqual(str.Size(), 4u);
    larvae::AssertEqual(str[0], static_cast<uint8_t>('T'));
    larvae::AssertEqual(str[1], static_cast<uint8_t>('e'));
    larvae::AssertEqual(str[2], static_cast<uint8_t>('s'));
    larvae::AssertEqual(str[3], static_cast<uint8_t>('t'));
});

auto roundtrip_alignment = larvae::RegisterTest("BinarySerialization", "RoundTripWithAlignment", []() {
    comb::LinearAllocator alloc{1024};
    wax::BinaryWriter<comb::LinearAllocator> writer{alloc};

    writer.Write<uint8_t>(0xFF);
    writer.WriteAlignment(4);    // 3 bytes padding
    writer.Write<uint32_t>(0xDEADBEEF);

    wax::BinaryReader reader{writer.View()};
    larvae::AssertEqual(reader.Read<uint8_t>(), static_cast<uint8_t>(0xFF));
    reader.Skip(3); // Skip padding
    larvae::AssertEqual(reader.Read<uint32_t>(), 0xDEADBEEFu);
});

} // anonymous namespace
