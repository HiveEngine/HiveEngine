#include <nectar/vfs/path.h>
#include <wax/containers/vector.h>

namespace nectar
{
    wax::String<> NormalizePath(wax::StringView path, comb::DefaultAllocator& alloc)
    {
        if (path.Size() == 0)
            return wax::String<>{alloc};

        // Copy into mutable buffer, replace backslashes and lowercase
        wax::String<> buf{alloc};
        buf.Append(path.Data(), path.Size());

        // In-place transform: backslash -> slash, uppercase -> lowercase
        char* data = const_cast<char*>(buf.Data());
        for (size_t i = 0; i < buf.Size(); ++i)
        {
            if (data[i] == '\\')
                data[i] = '/';
            else if (data[i] >= 'A' && data[i] <= 'Z')
                data[i] = static_cast<char>(data[i] + ('a' - 'A'));
        }

        // Split by '/', collect segments
        wax::Vector<wax::StringView> segments{alloc};
        auto view = buf.View();
        size_t start = 0;
        for (size_t i = 0; i <= view.Size(); ++i)
        {
            if (i == view.Size() || view[i] == '/')
            {
                auto seg = view.Substr(start, i - start);
                start = i + 1;

                // Skip empty segments and '.'
                if (seg.Size() == 0) continue;
                if (seg.Size() == 1 && seg[0] == '.') continue;

                // Handle '..'
                if (seg.Size() == 2 && seg[0] == '.' && seg[1] == '.')
                {
                    if (!segments.IsEmpty())
                        segments.PopBack();
                    continue;
                }

                segments.PushBack(seg);
            }
        }

        // Join with '/'
        wax::String<> result{alloc};
        for (size_t i = 0; i < segments.Size(); ++i)
        {
            if (i > 0)
                result.Append('/');
            result.Append(segments[i].Data(), segments[i].Size());
        }
        return result;
    }

    wax::StringView PathParent(wax::StringView path)
    {
        size_t pos = path.RFind('/');
        if (pos == wax::StringView::npos)
            return wax::StringView{};
        return path.Substr(0, pos);
    }

    wax::StringView PathFilename(wax::StringView path)
    {
        size_t pos = path.RFind('/');
        if (pos == wax::StringView::npos)
            return path;
        return path.Substr(pos + 1);
    }

    wax::StringView PathExtension(wax::StringView path)
    {
        auto filename = PathFilename(path);
        size_t pos = filename.RFind('.');
        if (pos == wax::StringView::npos || pos == 0)
            return wax::StringView{};
        return filename.Substr(pos);
    }
}
