#pragma once

#include <cstdint>
#include <cstdio>

namespace nectar
{
    inline int FileSeek64(FILE* f, int64_t offset, int origin)
    {
#ifdef _WIN32
        return _fseeki64(f, offset, origin);
#else
        return fseeko(f, static_cast<off_t>(offset), origin);
#endif
    }

    inline int64_t FileTell64(FILE* f)
    {
#ifdef _WIN32
        return _ftelli64(f);
#else
        return static_cast<int64_t>(ftello(f));
#endif
    }

    inline int64_t FileSize(FILE* f)
    {
        FileSeek64(f, 0, SEEK_END);
        int64_t size = FileTell64(f);
        FileSeek64(f, 0, SEEK_SET);
        return size;
    }
} // namespace nectar
