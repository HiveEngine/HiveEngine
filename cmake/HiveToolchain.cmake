include(CMakeParseArguments)

add_library(HiveToolchain_Base INTERFACE)
add_library(HiveToolchain_Warnings INTERFACE)
add_library(HiveToolchain_Config INTERFACE)
add_library(HiveToolchain_FastMath INTERFACE)
add_library(HiveToolchain_SIMD INTERFACE)
add_library(HiveToolchain_Sanitizers INTERFACE)
add_library(HiveToolchain_LTO INTERFACE)
add_library(HiveDefines_Platform INTERFACE)
add_library(HiveDefines_Config INTERFACE)
add_library(HiveDefines_Mode INTERFACE)
add_library(HiveDefines_Features INTERFACE)

add_library(HiveFeature_Exception_On INTERFACE)
add_library(HiveFeature_Exception_Off INTERFACE)
add_library(HiveFeature_RTTI_On INTERFACE)
add_library(HiveFeature_RTTI_Off INTERFACE)
add_library(HiveFeature_WError INTERFACE)

set(HIVE_COMPILER_IS_CLANG_CL OFF)
if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(HIVE_COMPILER_IS_CLANG_CL ON)
endif()

if(MSVC AND NOT HIVE_COMPILER_IS_CLANG_CL)
    set(HIVE_VENDOR_WARNING_COMPILE_OPTIONS /w)
else()
    set(HIVE_VENDOR_WARNING_COMPILE_OPTIONS -w)
endif()

if(MSVC)
    target_compile_options(HiveToolchain_Base INTERFACE /permissive- /Zc:__cplusplus /Zc:inline /utf-8)
    if(NOT HIVE_COMPILER_IS_CLANG_CL)
        target_compile_options(HiveToolchain_Base INTERFACE /MP /external:W0)
    endif()

    target_compile_options(HiveToolchain_Warnings INTERFACE
        /W4
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /w14296
        /w14640
        /wd4100
        /wd4530
    )

    if(HIVE_COMPILER_IS_CLANG_CL)
        target_compile_options(HiveToolchain_Warnings INTERFACE
            -Wno-unsafe-buffer-usage
            -Wno-unsafe-buffer-usage-in-libc-call
            -Wno-c++98-compat
        )
    endif()

    target_compile_options(HiveToolchain_Config INTERFACE
        "$<$<CONFIG:Debug>:/Od;/MDd;/Zi>"
        "$<$<CONFIG:Release>:/O2;/Oi;/MD;/GS-;/Gy>"
        "$<$<CONFIG:RelWithDebInfo>:/O2;/Zi;/MD>"
        "$<$<CONFIG:Profile>:/O2;/Zi;/MD;/GS-;/Gy>"
        "$<$<CONFIG:Retail>:/O2;/Oi;/MD;/GS-;/Gy>"
    )

    target_compile_definitions(HiveToolchain_Config INTERFACE
        "$<$<CONFIG:Debug>:_DEBUG;_ITERATOR_DEBUG_LEVEL=2>"
        "$<$<CONFIG:Release>:NDEBUG;_ITERATOR_DEBUG_LEVEL=0>"
        "$<$<CONFIG:RelWithDebInfo>:NDEBUG;_ITERATOR_DEBUG_LEVEL=1>"
        "$<$<CONFIG:Profile>:NDEBUG;_ITERATOR_DEBUG_LEVEL=0>"
        "$<$<CONFIG:Retail>:NDEBUG;_ITERATOR_DEBUG_LEVEL=0>"
    )

    target_compile_options(HiveFeature_Exception_Off INTERFACE /EHs-c-)
    target_compile_options(HiveFeature_Exception_On INTERFACE /EHsc)
    target_compile_options(HiveFeature_RTTI_Off INTERFACE /GR-)
    target_compile_options(HiveFeature_RTTI_On INTERFACE /GR)
    target_compile_options(HiveFeature_WError INTERFACE /WX)

    if(HIVE_ENABLE_FAST_MATH)
        target_compile_options(HiveToolchain_FastMath INTERFACE /fp:fast)
    endif()

    if(HIVE_ENABLE_SIMD AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_options(HiveToolchain_SIMD INTERFACE /arch:SSE2)
    endif()
else()
    target_compile_options(HiveToolchain_Warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
    )

    target_compile_options(HiveToolchain_Config INTERFACE
        "$<$<CONFIG:Debug>:-O0;-g3>"
        "$<$<CONFIG:Release>:-O3;-ffunction-sections;-fdata-sections>"
        "$<$<CONFIG:RelWithDebInfo>:-O2;-g>"
        "$<$<CONFIG:Profile>:-O2;-g;-fno-omit-frame-pointer>"
        "$<$<CONFIG:Retail>:-O3;-ffunction-sections;-fdata-sections>"
    )

    target_compile_definitions(HiveToolchain_Config INTERFACE
        "$<$<CONFIG:Debug>:_GLIBCXX_DEBUG>"
    )

    if(NOT WIN32)
        target_link_options(HiveToolchain_Config INTERFACE
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:Retail>>:-Wl,--gc-sections>"
        )
    endif()

    target_compile_options(HiveFeature_Exception_Off INTERFACE -fno-exceptions)
    target_compile_options(HiveFeature_Exception_On INTERFACE -fexceptions)
    target_compile_options(HiveFeature_RTTI_Off INTERFACE -fno-rtti)
    target_compile_options(HiveFeature_RTTI_On INTERFACE -frtti)
    target_compile_options(HiveFeature_WError INTERFACE -Werror)

    if(HIVE_ENABLE_FAST_MATH)
        target_compile_options(HiveToolchain_FastMath INTERFACE -ffast-math)
    endif()

    if(HIVE_ENABLE_SIMD)
        target_compile_options(HiveToolchain_SIMD INTERFACE -msse2)
    endif()

    if(HIVE_ENABLE_SANITIZERS)
        target_compile_options(HiveToolchain_Sanitizers INTERFACE
            "$<$<CONFIG:Debug>:-fno-omit-frame-pointer;-fno-optimize-sibling-calls>"
        )

        if(HIVE_SANITIZER_TYPE STREQUAL "Address")
            target_compile_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=address>")
            target_link_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=address>")
            message(STATUS "AddressSanitizer enabled for Debug builds")
        elseif(HIVE_SANITIZER_TYPE STREQUAL "UndefinedBehavior")
            if(WIN32)
                target_compile_options(HiveToolchain_Sanitizers INTERFACE
                    "$<$<CONFIG:Debug>:-fsanitize=undefined;-fno-sanitize=alignment,function,vptr>"
                )
                target_link_options(HiveToolchain_Sanitizers INTERFACE
                    "$<$<CONFIG:Debug>:-fsanitize=undefined;-fno-sanitize=alignment,function,vptr>"
                )
            else()
                target_compile_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=undefined>")
                target_link_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=undefined>")
            endif()
            message(STATUS "UndefinedBehaviorSanitizer enabled for Debug builds")
        elseif(HIVE_SANITIZER_TYPE STREQUAL "Thread")
            target_compile_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=thread>")
            target_link_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=thread>")
            message(STATUS "ThreadSanitizer enabled for Debug builds")
        elseif(HIVE_SANITIZER_TYPE STREQUAL "Memory")
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
                target_compile_options(HiveToolchain_Sanitizers INTERFACE
                    "$<$<CONFIG:Debug>:-fsanitize=memory;-fsanitize-memory-track-origins>"
                )
                target_link_options(HiveToolchain_Sanitizers INTERFACE "$<$<CONFIG:Debug>:-fsanitize=memory>")
                message(STATUS "MemorySanitizer enabled for Debug builds")
            else()
                message(WARNING "MemorySanitizer only available with Clang on Linux/macOS")
            endif()
        endif()
    endif()
endif()

set(HIVE_IPO_SUPPORTED OFF)
if(HIVE_ENABLE_LTO)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
        target_compile_options(HiveToolchain_LTO INTERFACE
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Profile>,$<CONFIG:Retail>>:-flto=thin>"
        )
        target_link_options(HiveToolchain_LTO INTERFACE
            "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Profile>,$<CONFIG:Retail>>:-flto=thin>"
        )
        message(STATUS "LTO enabled for optimized builds (Clang thin LTO)")
    else()
        include(CheckIPOSupported)
        check_ipo_supported(RESULT HIVE_IPO_SUPPORTED OUTPUT HIVE_IPO_OUTPUT)
        if(HIVE_IPO_SUPPORTED)
            message(STATUS "LTO/IPO enabled for optimized builds")
        else()
            message(WARNING "LTO/IPO not supported: ${HIVE_IPO_OUTPUT}")
        endif()
    endif()
endif()

function(hive_get_target_scope out_var target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Unknown target '${target}'")
    endif()

    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY")
        set(${out_var} INTERFACE PARENT_SCOPE)
    else()
        set(${out_var} PRIVATE PARENT_SCOPE)
    endif()
endfunction()

function(hive_get_vendor_warning_compile_options out_var)
    set(${out_var} ${HIVE_VENDOR_WARNING_COMPILE_OPTIONS} PARENT_SCOPE)
endfunction()

function(hive_silence_target_warnings target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Unknown target '${target}'")
    endif()

    target_compile_options(${target} PRIVATE ${HIVE_VENDOR_WARNING_COMPILE_OPTIONS})
endfunction()

function(hive_configure_target target)
    set(options NO_WARNINGS)
    set(oneValueArgs RTTI EXCEPTIONS)
    cmake_parse_arguments(HIVE_CFG "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT HIVE_CFG_RTTI)
        set(HIVE_CFG_RTTI OFF)
    endif()

    if(NOT HIVE_CFG_EXCEPTIONS)
        set(HIVE_CFG_EXCEPTIONS OFF)
    endif()

    hive_get_target_scope(_scope ${target})

    target_link_libraries(${target} ${_scope}
        HiveToolchain_Base
        HiveToolchain_Config
        HiveDefines_Platform
        HiveDefines_Config
        HiveDefines_Mode
        HiveDefines_Features
    )

    if(NOT HIVE_CFG_NO_WARNINGS)
        target_link_libraries(${target} ${_scope} HiveToolchain_Warnings)
        if(HIVE_WARNINGS_AS_ERRORS)
            target_link_libraries(${target} ${_scope} HiveFeature_WError)
        endif()
    endif()

    if(HIVE_ENABLE_FAST_MATH)
        target_link_libraries(${target} ${_scope} HiveToolchain_FastMath)
    endif()

    if(HIVE_ENABLE_SIMD)
        target_link_libraries(${target} ${_scope} HiveToolchain_SIMD)
    endif()

    if(HIVE_ENABLE_SANITIZERS)
        target_link_libraries(${target} ${_scope} HiveToolchain_Sanitizers)
    endif()

    if(HIVE_ENABLE_LTO AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
        target_link_libraries(${target} ${_scope} HiveToolchain_LTO)
    endif()

    if(HIVE_CFG_EXCEPTIONS STREQUAL "ON")
        target_link_libraries(${target} ${_scope} HiveFeature_Exception_On)
    elseif(HIVE_CFG_EXCEPTIONS STREQUAL "OFF")
        target_link_libraries(${target} ${_scope} HiveFeature_Exception_Off)
    else()
        message(FATAL_ERROR "hive_configure_target(${target}): EXCEPTIONS must be ON or OFF")
    endif()

    if(HIVE_CFG_RTTI STREQUAL "ON")
        target_link_libraries(${target} ${_scope} HiveFeature_RTTI_On)
    elseif(HIVE_CFG_RTTI STREQUAL "OFF")
        target_link_libraries(${target} ${_scope} HiveFeature_RTTI_Off)
    else()
        message(FATAL_ERROR "hive_configure_target(${target}): RTTI must be ON or OFF")
    endif()

    get_target_property(_type ${target} TYPE)
    if(HIVE_ENABLE_LTO AND HIVE_IPO_SUPPORTED AND NOT _type STREQUAL "INTERFACE_LIBRARY")
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_PROFILE ON)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RETAIL ON)
    endif()
endfunction()

if(WIN32)
    target_compile_definitions(HiveDefines_Platform INTERFACE HIVE_PLATFORM_WINDOWS=1)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(HiveDefines_Platform INTERFACE HIVE_PLATFORM_LINUX=1)
elseif(APPLE)
    target_compile_definitions(HiveDefines_Platform INTERFACE HIVE_PLATFORM_MACOS=1)
endif()

target_compile_definitions(HiveDefines_Config INTERFACE
    $<$<CONFIG:Debug>:HIVE_CONFIG_DEBUG=1>
    $<$<CONFIG:Release>:HIVE_CONFIG_RELEASE=1>
    $<$<CONFIG:RelWithDebInfo>:HIVE_CONFIG_RELEASE=1>
    $<$<CONFIG:Profile>:HIVE_CONFIG_PROFILE=1>
    $<$<CONFIG:Retail>:HIVE_CONFIG_RETAIL=1>
)

if(HIVE_ENGINE_MODE STREQUAL "Editor")
    target_compile_definitions(HiveDefines_Mode INTERFACE HIVE_MODE_EDITOR=1)
elseif(HIVE_ENGINE_MODE STREQUAL "Game")
    target_compile_definitions(HiveDefines_Mode INTERFACE HIVE_MODE_GAME=1)
elseif(HIVE_ENGINE_MODE STREQUAL "Headless")
    target_compile_definitions(HiveDefines_Mode INTERFACE HIVE_MODE_HEADLESS=1)
endif()
