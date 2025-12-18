# LLVM/Clang Toolchain for Windows
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-windows.cmake ..

set(CMAKE_SYSTEM_NAME Windows)

# Find LLVM installation
# Priority: Environment variable > VS installation > Common install locations
if(DEFINED ENV{LLVM_PATH})
    set(LLVM_ROOT "$ENV{LLVM_PATH}")
elseif(EXISTS "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64")
    set(LLVM_ROOT "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64")
elseif(EXISTS "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64")
    set(LLVM_ROOT "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64")
elseif(EXISTS "C:/Program Files/LLVM")
    set(LLVM_ROOT "C:/Program Files/LLVM")
else()
    message(FATAL_ERROR "LLVM not found. Set LLVM_PATH environment variable or install LLVM")
endif()

# Set compilers
set(CMAKE_C_COMPILER "${LLVM_ROOT}/bin/clang.exe" CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER "${LLVM_ROOT}/bin/clang++.exe" CACHE FILEPATH "C++ compiler")
set(CMAKE_AR "${LLVM_ROOT}/bin/llvm-ar.exe" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "${LLVM_ROOT}/bin/llvm-ranlib.exe" CACHE FILEPATH "Ranlib")
set(CMAKE_LINKER "${LLVM_ROOT}/bin/lld-link.exe" CACHE FILEPATH "Linker")

# Use LLD linker
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")

# Target triple for Windows x64
set(CMAKE_C_COMPILER_TARGET "x86_64-pc-windows-msvc")
set(CMAKE_CXX_COMPILER_TARGET "x86_64-pc-windows-msvc")

message(STATUS "LLVM Toolchain: ${LLVM_ROOT}")