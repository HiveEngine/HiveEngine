#pragma once

#if HIVE_FEATURE_VULKAN || HIVE_FEATURE_D3D12
#include <swarm/platform/diligent_swarm.h>
#else
#include <swarm/platform/null_swarm.h>
#endif
