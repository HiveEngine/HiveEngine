#include <nectar/hive/hive_parser.h>
#include <cstdlib>

namespace nectar
{
    namespace
    {
        wax::StringView TrimWhitespace(wax::StringView line)
        {
            size_t start = 0;
            while (start < line.Size() && (line[start] == ' ' || line[start] == '\t'))
                ++start;

            size_t end = line.Size();
            while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r'))
                --end;

            return line.Substr(start, end - start);
        }

        bool IsDigitOrSign(char c)
        {
            return (c >= '0' && c <= '9') || c == '-' || c == '+';
        }

        // Try to parse an integer. Returns false if it looks like a float.
        bool TryParseInt(wax::StringView text, int64_t& out)
        {
            if (text.IsEmpty()) return false;

            // Check for float (contains '.' or 'e'/'E')
            for (size_t i = 0; i < text.Size(); ++i)
            {
                if (text[i] == '.' || text[i] == 'e' || text[i] == 'E')
                    return false;
            }

            char buf[64];
            size_t len = text.Size() < 63 ? text.Size() : 63;
            for (size_t i = 0; i < len; ++i) buf[i] = text[i];
            buf[len] = '\0';

            char* end = nullptr;
            long long val = std::strtoll(buf, &end, 10);
            if (end != buf + len) return false;

            out = static_cast<int64_t>(val);
            return true;
        }

        bool TryParseFloat(wax::StringView text, double& out)
        {
            if (text.IsEmpty()) return false;

            char buf[64];
            size_t len = text.Size() < 63 ? text.Size() : 63;
            for (size_t i = 0; i < len; ++i) buf[i] = text[i];
            buf[len] = '\0';

            char* end = nullptr;
            double val = std::strtod(buf, &end);
            if (end != buf + len) return false;

            out = val;
            return true;
        }

        // Extract a quoted string from text starting at pos (which should be '"').
        // Returns the content without quotes, and advances pos past the closing quote.
        bool ExtractQuotedString(wax::StringView text, size_t& pos, wax::String<>& out,
                                 comb::DefaultAllocator& alloc)
        {
            if (pos >= text.Size() || text[pos] != '"') return false;
            ++pos; // skip opening quote

            out = wax::String<>{alloc};
            while (pos < text.Size())
            {
                char c = text[pos];
                if (c == '"')
                {
                    ++pos; // skip closing quote
                    return true;
                }
                if (c == '\\' && pos + 1 < text.Size())
                {
                    ++pos;
                    char escaped = text[pos];
                    switch (escaped)
                    {
                        case 'n': out.Append('\n'); break;
                        case 't': out.Append('\t'); break;
                        case '\\': out.Append('\\'); break;
                        case '"': out.Append('"'); break;
                        default: out.Append('\\'); out.Append(escaped); break;
                    }
                    ++pos;
                    continue;
                }
                out.Append(c);
                ++pos;
            }
            return false; // unterminated string
        }

        HiveValue ParseValue(wax::StringView text, comb::DefaultAllocator& alloc, bool& ok)
        {
            ok = true;
            auto trimmed = TrimWhitespace(text);
            if (trimmed.IsEmpty())
            {
                ok = false;
                return HiveValue{};
            }

            // String: "..."
            if (trimmed[0] == '"')
            {
                size_t pos = 0;
                wax::String<> str{alloc};
                if (ExtractQuotedString(trimmed, pos, str, alloc))
                {
                    HiveValue v{};
                    v.type = HiveValue::Type::String;
                    v.str = static_cast<wax::String<>&&>(str);
                    return v;
                }
                ok = false;
                return HiveValue{};
            }

            // Bool
            if (trimmed.Equals("true"))  return HiveValue::MakeBool(true);
            if (trimmed.Equals("false")) return HiveValue::MakeBool(false);

            // String array: [...]
            if (trimmed[0] == '[')
            {
                auto v = HiveValue::MakeStringArray(alloc);

                // Find matching ']'
                size_t end = trimmed.Size();
                if (end < 2 || trimmed[end - 1] != ']')
                {
                    ok = false;
                    return HiveValue{};
                }

                auto inner = TrimWhitespace(trimmed.Substr(1, end - 2));
                if (inner.IsEmpty()) return v; // empty array []

                // Parse comma-separated quoted strings
                size_t pos = 0;
                while (pos < inner.Size())
                {
                    // Skip whitespace and commas
                    while (pos < inner.Size() && (inner[pos] == ' ' || inner[pos] == '\t' || inner[pos] == ','))
                        ++pos;
                    if (pos >= inner.Size()) break;

                    if (inner[pos] == '"')
                    {
                        wax::String<> elem{alloc};
                        if (!ExtractQuotedString(inner, pos, elem, alloc))
                        {
                            ok = false;
                            return HiveValue{};
                        }
                        v.array.PushBack(static_cast<wax::String<>&&>(elem));
                    }
                    else
                    {
                        ok = false;
                        return HiveValue{};
                    }
                }
                return v;
            }

            // Number — try int first, then float
            if (IsDigitOrSign(trimmed[0]))
            {
                int64_t int_val{};
                if (TryParseInt(trimmed, int_val))
                    return HiveValue::MakeInt(int_val);

                double float_val{};
                if (TryParseFloat(trimmed, float_val))
                    return HiveValue::MakeFloat(float_val);
            }

            // Unquoted string fallback
            ok = false;
            return HiveValue{};
        }
    }

    HiveParseResult HiveParser::Parse(wax::StringView content, comb::DefaultAllocator& alloc)
    {
        HiveParseResult result{HiveDocument{alloc}, wax::Vector<HiveParseError>{alloc}};

        wax::String<> current_section{alloc};
        size_t line_num = 0;
        size_t pos = 0;

        while (pos < content.Size())
        {
            // Extract one line
            size_t line_start = pos;
            while (pos < content.Size() && content[pos] != '\n')
                ++pos;

            wax::StringView raw_line = content.Substr(line_start, pos - line_start);
            if (pos < content.Size()) ++pos; // skip '\n'
            ++line_num;

            auto line = TrimWhitespace(raw_line);

            // Skip empty lines and comments
            if (line.IsEmpty()) continue;
            if (line[0] == '#') continue;

            // Section header: [name]
            if (line[0] == '[')
            {
                size_t close = line.Find(']');
                if (close == wax::StringView::npos || close < 2)
                {
                    HiveParseError err{};
                    err.line = line_num;
                    err.message = wax::String<>{alloc, "Invalid section header"};
                    result.errors.PushBack(static_cast<HiveParseError&&>(err));
                    continue;
                }

                auto name = TrimWhitespace(line.Substr(1, close - 1));
                if (name.IsEmpty())
                {
                    HiveParseError err{};
                    err.line = line_num;
                    err.message = wax::String<>{alloc, "Empty section name"};
                    result.errors.PushBack(static_cast<HiveParseError&&>(err));
                    continue;
                }

                current_section = wax::String<>{alloc, name};
                result.document.AddSection(name);
                continue;
            }

            // Key = Value
            size_t eq = line.Find('=');
            if (eq == wax::StringView::npos)
            {
                HiveParseError err{};
                err.line = line_num;
                err.message = wax::String<>{alloc, "Expected key = value"};
                result.errors.PushBack(static_cast<HiveParseError&&>(err));
                continue;
            }

            if (current_section.IsEmpty())
            {
                HiveParseError err{};
                err.line = line_num;
                err.message = wax::String<>{alloc, "Key-value outside of section"};
                result.errors.PushBack(static_cast<HiveParseError&&>(err));
                continue;
            }

            auto key = TrimWhitespace(line.Substr(0, eq));
            auto value_text = TrimWhitespace(line.Substr(eq + 1));

            if (key.IsEmpty())
            {
                HiveParseError err{};
                err.line = line_num;
                err.message = wax::String<>{alloc, "Empty key"};
                result.errors.PushBack(static_cast<HiveParseError&&>(err));
                continue;
            }

            // Strip inline comment (only outside quotes)
            // We handle this by checking if the value was fully parsed
            bool parse_ok = false;
            auto value = ParseValue(value_text, alloc, parse_ok);
            if (!parse_ok)
            {
                HiveParseError err{};
                err.line = line_num;
                err.message = wax::String<>{alloc, "Invalid value"};
                result.errors.PushBack(static_cast<HiveParseError&&>(err));
                continue;
            }

            result.document.SetValue(current_section.View(), key, static_cast<HiveValue&&>(value));
        }

        return result;
    }
}
