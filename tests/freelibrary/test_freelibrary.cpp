#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Force hive_engine.dll to stay loaded by importing a symbol
__declspec(dllimport) int HiveTestPing();

int main() {
    // Call to force the import — keeps hive_engine.dll ref count > 0
    int ping = HiveTestPing();
    (void)ping;

    HMODULE h = LoadLibraryA("gameplay_test.dll");
    if (!h) { fprintf(stderr, "FAIL: %lu\n", GetLastError()); return 1; }
    fprintf(stderr, "Loaded. FreeLibrary...\n");
    FreeLibrary(h);
    fprintf(stderr, "OK. Again...\n");
    h = LoadLibraryA("gameplay_test.dll");
    FreeLibrary(h);
    fprintf(stderr, "SUCCESS\n");
    return 0;
}
