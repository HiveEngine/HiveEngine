#pragma once
#include "platform/Platform.h"

#if defined(HIVE_PLATFORM_WINDOWS) || defined(HIVE_PLATFORM_LINUX)
#define HIVE_BACKEND_VULKAN_SUPPORTED
#endif