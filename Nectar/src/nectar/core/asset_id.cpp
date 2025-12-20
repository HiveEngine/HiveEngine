#include <nectar/core/asset_id.h>
#include <random>
#include <chrono>

#if defined(_WIN32)
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#elif defined(__linux__) || defined(__APPLE__)
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace nectar
{
    namespace
    {
        bool FillCryptoRandom(uint8_t* buffer, size_t size) noexcept
        {
#if defined(_WIN32)
            NTSTATUS status = BCryptGenRandom(
                nullptr,
                buffer,
                static_cast<ULONG>(size),
                BCRYPT_USE_SYSTEM_PREFERRED_RNG
            );
            return BCRYPT_SUCCESS(status);
#elif defined(__linux__) || defined(__APPLE__)
            int fd = open("/dev/urandom", O_RDONLY);
            if (fd < 0)
            {
                return false;
            }

            ssize_t bytes_read = read(fd, buffer, size);
            close(fd);

            return bytes_read == static_cast<ssize_t>(size);
#else
            (void)buffer;
            (void)size;
            return false;
#endif
        }

        void FillFallbackRandom(uint8_t* buffer, size_t size) noexcept
        {
            static thread_local std::mt19937_64 engine{
                static_cast<uint64_t>(
                    std::chrono::high_resolution_clock::now().time_since_epoch().count()
                )
            };

            std::uniform_int_distribution<uint64_t> dist;

            size_t i = 0;
            while (i + 8 <= size)
            {
                uint64_t val = dist(engine);
                std::memcpy(buffer + i, &val, 8);
                i += 8;
            }

            if (i < size)
            {
                uint64_t val = dist(engine);
                std::memcpy(buffer + i, &val, size - i);
            }
        }

        constexpr uint8_t HexCharToNibble(char c) noexcept
        {
            if (c >= '0' && c <= '9')
            {
                return static_cast<uint8_t>(c - '0');
            }
            if (c >= 'a' && c <= 'f')
            {
                return static_cast<uint8_t>(c - 'a' + 10);
            }
            if (c >= 'A' && c <= 'F')
            {
                return static_cast<uint8_t>(c - 'A' + 10);
            }
            return 0xFF;
        }
    }

    AssetId AssetId::Generate() noexcept
    {
        uint8_t bytes[kByteSize];

        if (!FillCryptoRandom(bytes, kByteSize))
        {
            FillFallbackRandom(bytes, kByteSize);
        }

        return FromBytes(bytes);
    }

    AssetId AssetId::FromString(const char* str, size_t length) noexcept
    {
        if (length != kStringLength)
        {
            return Invalid();
        }

        uint8_t bytes[kByteSize];

        for (size_t i = 0; i < kByteSize; ++i)
        {
            uint8_t high_nibble = HexCharToNibble(str[i * 2]);
            uint8_t low_nibble = HexCharToNibble(str[i * 2 + 1]);

            if (high_nibble == 0xFF || low_nibble == 0xFF)
            {
                return Invalid();
            }

            bytes[i] = static_cast<uint8_t>((high_nibble << 4) | low_nibble);
        }

        return FromBytes(bytes);
    }

}
