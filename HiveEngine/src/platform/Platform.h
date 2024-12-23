#pragma once

#ifdef _WIN64
#define HIVE_PLATFORM_WINDOWS
#endif

#ifdef __linux__
#define HIVE_PLATFORM_LINUX
#endif