/**
 * Platform Utilities Implementation
 *
 * Platform-specific implementations for memory debugging utilities.
 * Only compiled when COMB_MEM_DEBUG=1.
 */

#include <comb/debug/platform_utils.h>

#if COMB_MEM_DEBUG && COMB_MEM_DEBUG_CALLSTACKS

#include <hive/core/log.h>
#include <comb/combmodule.h>

namespace comb::debug
{

/**
 * Print callstack to log (platform-specific symbolication)
 *
 * Resolves addresses to function names and prints to hive::Log.
 *
 * Requirements:
 * - Windows: dbghelp.lib linked, symbols available (.pdb)
 * - Linux/macOS: Compile with -rdynamic for symbol resolution
 */
void PrintCallstack(void* const* frames, uint32_t depth)
{
    if (depth == 0 || frames == nullptr)
    {
        hive::LogInfo(comb::LogCombRoot, "    (no callstack available)");
        return;
    }

#if HIVE_PLATFORM_WINDOWS
    // Windows: Use dbghelp for symbol resolution
    // NOTE: Requires linking against dbghelp.lib

    #include <windows.h>
    #include <dbghelp.h>

    // Initialize symbol handler (once per process)
    static bool symbolsInitialized = false;
    if (!symbolsInitialized)
    {
        HANDLE process = GetCurrentProcess();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);

        if (!SymInitialize(process, nullptr, TRUE))
        {
            hive::LogWarning(comb::LogCombRoot,
                             "[MEM_DEBUG] Failed to initialize symbol handler (error: {})",
                             GetLastError());
            return;
        }

        symbolsInitialized = true;
    }

    HANDLE process = GetCurrentProcess();

    // Allocate symbol info buffer
    constexpr size_t maxNameLength = 256;
    char symbolBuffer[sizeof(SYMBOL_INFO) + maxNameLength];
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = maxNameLength;

    // Resolve each frame
    for (uint32_t i = 0; i < depth; ++i)
    {
        DWORD64 address = reinterpret_cast<DWORD64>(frames[i]);

        // Try to resolve symbol
        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol))
        {
            // Try to get line info
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisplacement = 0;

            if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line))
            {
                // Symbol + file + line
                hive::LogInfo(comb::LogCombRoot,
                              "      #{}: {} ({}:{})",
                              i, symbol->Name, line.FileName, line.LineNumber);
            }
            else
            {
                // Symbol only (no line info)
                hive::LogInfo(comb::LogCombRoot,
                              "      #{}: {} +{} bytes",
                              i, symbol->Name, displacement);
            }
        }
        else
        {
            // No symbol found
            hive::LogInfo(comb::LogCombRoot,
                          "      #{}: 0x{:016X} (no symbol)",
                          i, address);
        }
    }

#elif HIVE_PLATFORM_LINUX || HIVE_PLATFORM_MACOS
    // POSIX: Use backtrace_symbols for symbol resolution
    // NOTE: Requires compiling with -rdynamic for full symbols

    #include <execinfo.h>
    #include <cxxabi.h>  // For __cxa_demangle (C++ name demangling)

    char** symbols = backtrace_symbols(const_cast<void**>(frames), depth);
    if (symbols == nullptr)
    {
        hive::LogWarning(comb::LogCombRoot,
                         "[MEM_DEBUG] Failed to resolve callstack symbols");
        return;
    }

    // Parse and demangle symbols
    for (uint32_t i = 0; i < depth; ++i)
    {
        // Format: "<module>(<function>+<offset>) [<address>]"
        // Example: "./myapp(_Z3foov+0x42) [0x401234]"

        char* mangled_name = nullptr;
        char* offset_begin = nullptr;

        // Find function name start '('
        for (char* p = symbols[i]; *p; ++p)
        {
            if (*p == '(')
            {
                mangled_name = p + 1;
            }
            else if (*p == '+')
            {
                offset_begin = p;
                break;
            }
        }

        if (mangled_name && offset_begin)
        {
            // Extract mangled name
            *offset_begin = '\0';

            // Demangle C++ name
            int status = 0;
            char* demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);

            if (status == 0 && demangled)
            {
                // Demangled successfully
                hive::LogInfo(comb::LogCombRoot,
                              "      #{}: {}",
                              i, demangled);
                free(demangled);
            }
            else
            {
                // Demangling failed, use mangled name
                hive::LogInfo(comb::LogCombRoot,
                              "      #{}: {}",
                              i, mangled_name);
            }

            *offset_begin = '+';  // Restore for next iteration
        }
        else
        {
            // No function name, print raw symbol
            hive::LogInfo(comb::LogCombRoot,
                          "      #{}: {}",
                          i, symbols[i]);
        }
    }

    free(symbols);

#else
    #error "Unsupported platform for PrintCallstack()"
#endif
}

} // namespace comb::debug

#endif // COMB_MEM_DEBUG && COMB_MEM_DEBUG_CALLSTACKS
