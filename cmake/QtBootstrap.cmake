get_filename_component(_hive_qt_root_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(_hive_qt_source_dir "${_hive_qt_root_dir}/Forge/extern/qtbase")

set(_hive_qt_build_config "${CMAKE_BUILD_TYPE}")
if(NOT _hive_qt_build_config)
    set(_hive_qt_build_config "Debug")
endif()
string(TOLOWER "${_hive_qt_build_config}" _hive_qt_config_suffix)

set(_hive_qt_package_dir "${HIVE_QT_PACKAGE_DIR}")
if(NOT _hive_qt_package_dir)
    set(_hive_qt_package_dir "${_hive_qt_root_dir}/out/build/qt-${_hive_qt_config_suffix}/install")
endif()

set(_hive_qt_build_dir "${HIVE_QT_BOOTSTRAP_BUILD_DIR}")
if(NOT _hive_qt_build_dir)
    set(_hive_qt_build_dir "${_hive_qt_root_dir}/out/build/qt-${_hive_qt_config_suffix}")
endif()

function(hive_qt_find_prebuilt PACKAGE_DIR OUT_FOUND)
    set(_prev_global FALSE)
    if(DEFINED CMAKE_FIND_PACKAGE_TARGETS_GLOBAL)
        set(_prev_global TRUE)
        set(_prev_global_val "${CMAKE_FIND_PACKAGE_TARGETS_GLOBAL}")
    endif()

    set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)
    list(PREPEND CMAKE_PREFIX_PATH "${PACKAGE_DIR}")
    find_package(Qt6 QUIET COMPONENTS Widgets
        PATHS "${PACKAGE_DIR}"
    )

    if(_prev_global)
        set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL "${_prev_global_val}")
    else()
        unset(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL)
    endif()

    if(Qt6_FOUND)
        set(${OUT_FOUND} TRUE PARENT_SCOPE)
    else()
        set(${OUT_FOUND} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(hive_qt_bootstrap SOURCE_DIR BUILD_DIR INSTALL_DIR)
    if(NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "Forge: qtbase submodule not found at ${SOURCE_DIR}.\n"
            "Run: git submodule update --init Forge/extern/qtbase"
        )
    endif()

    file(MAKE_DIRECTORY "${BUILD_DIR}")
    file(MAKE_DIRECTORY "${INSTALL_DIR}")

    set(_qt_cmake "${CMAKE_COMMAND}")
    if(WIN32)
        get_filename_component(_qt_cmake_bin_dir "${CMAKE_COMMAND}" DIRECTORY)
        string(TOLOWER "${_qt_cmake_bin_dir}" _qt_cmake_bin_lower)
        if(_qt_cmake_bin_lower MATCHES "msys|mingw|ucrt")
            find_program(_qt_native_cmake NAMES cmake
                PATHS
                    "$ENV{LOCALAPPDATA}/Programs/CLion/bin/cmake/win/x64/bin"
                    "C:/Program Files/CMake/bin"
                    "C:/Program Files (x86)/CMake/bin"
                NO_DEFAULT_PATH
            )
            if(_qt_native_cmake)
                set(_qt_cmake "${_qt_native_cmake}")
                message(STATUS "Forge: using native CMake for Qt bootstrap: ${_qt_cmake}")
            else()
                message(WARNING
                    "Forge: MSYS2 CMake detected. Qt bootstrap may fail due to "
                    "conflicting system headers. Install CMake natively or set "
                    "CMAKE_COMMAND to a non-MSYS2 cmake."
                )
            endif()
        endif()
    endif()

    set(_qt_configure_stdout_log "${BUILD_DIR}/hive_qt_configure.stdout.log")
    set(_qt_configure_stderr_log "${BUILD_DIR}/hive_qt_configure.stderr.log")

    set(_qt_configure_cmd
        "${_qt_cmake}"
        -S "${SOURCE_DIR}"
        -B "${BUILD_DIR}"
        -G "${CMAKE_GENERATOR}"
        "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}"
        "-DCMAKE_BUILD_TYPE=${_hive_qt_build_config}"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DQT_BUILD_EXAMPLES=OFF"
        "-DQT_BUILD_TESTS=OFF"
        "-DQT_BUILD_BENCHMARKS=OFF"
        "-DFEATURE_sql=OFF"
        "-DFEATURE_network=OFF"
        "-DFEATURE_testlib=OFF"
        "-DFEATURE_dbus=OFF"
        "-DFEATURE_printsupport=OFF"
        "-DFEATURE_concurrent=OFF"
        "-DFEATURE_xml=OFF"
        "-DFEATURE_opengl=OFF"
        "-DFEATURE_opengl_dynamic=OFF"
        "-DINPUT_opengl=no"
        "-DFEATURE_windeployqt=OFF"
        "-DFEATURE_zstd=OFF"
        "-DFEATURE_int128=OFF"
        "-DCMAKE_CXX_FLAGS=-U__SIZEOF_INT128__"
        "-DCMAKE_C_FLAGS=-U__SIZEOF_INT128__"
        "-DCMAKE_IGNORE_PREFIX_PATH=C:/msys64"
        "-DQT_BUILD_TOOLS_BY_DEFAULT=OFF"
        "-DQT_WILL_INSTALL=ON"
    )

    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND _qt_configure_cmd -A "${CMAKE_GENERATOR_PLATFORM}")
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND _qt_configure_cmd -T "${CMAKE_GENERATOR_TOOLSET}")
    endif()
    if(CMAKE_C_COMPILER)
        list(APPEND _qt_configure_cmd "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
    endif()
    if(CMAKE_CXX_COMPILER)
        list(APPEND _qt_configure_cmd "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
    endif()
    if(CMAKE_AR)
        list(APPEND _qt_configure_cmd "-DCMAKE_AR=${CMAKE_AR}")
    endif()
    if(CMAKE_LINKER)
        list(APPEND _qt_configure_cmd "-DCMAKE_LINKER=${CMAKE_LINKER}")
    endif()
    if(CMAKE_C_COMPILER_LAUNCHER)
        list(APPEND _qt_configure_cmd "-DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}")
    endif()
    if(CMAKE_CXX_COMPILER_LAUNCHER)
        list(APPEND _qt_configure_cmd "-DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}")
    endif()
    if(CMAKE_RC_COMPILER)
        list(APPEND _qt_configure_cmd "-DCMAKE_RC_COMPILER=${CMAKE_RC_COMPILER}")
    endif()
    if(WIN32)
        file(GLOB _qt_sdk_versions "C:/Program Files (x86)/Windows Kits/10/Include/10.*")
        if(_qt_sdk_versions)
            list(SORT _qt_sdk_versions COMPARE NATURAL ORDER DESCENDING)
            list(GET _qt_sdk_versions 0 _qt_sdk_inc)
            list(APPEND _qt_configure_cmd
                "-DCMAKE_RC_FLAGS=-I \"${_qt_sdk_inc}/um\" -I \"${_qt_sdk_inc}/shared\""
            )
        endif()
    endif()
    if(CMAKE_MSVC_RUNTIME_LIBRARY)
        list(APPEND _qt_configure_cmd "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()

    message(STATUS "Forge: bootstrapping Qt6 from source in ${BUILD_DIR}")
    message(STATUS "Forge: this may take 15-20 minutes on first run...")

    execute_process(
        COMMAND ${_qt_configure_cmd}
        RESULT_VARIABLE _qt_configure_result
        OUTPUT_FILE "${_qt_configure_stdout_log}"
        ERROR_FILE  "${_qt_configure_stderr_log}"
    )
    if(NOT _qt_configure_result EQUAL 0)
        message(FATAL_ERROR
            "Forge: Qt6 configure failed.\n"
            "See ${_qt_configure_stdout_log} and ${_qt_configure_stderr_log}"
        )
    endif()

    execute_process(
        COMMAND "${_qt_cmake}" --build "${BUILD_DIR}" --config ${_hive_qt_build_config}
        RESULT_VARIABLE _qt_build_result
    )
    if(NOT _qt_build_result EQUAL 0)
        message(FATAL_ERROR "Forge: Qt6 build failed.")
    endif()

    execute_process(
        COMMAND "${_qt_cmake}" --install "${BUILD_DIR}" --config ${_hive_qt_build_config}
        RESULT_VARIABLE _qt_install_result
    )
    if(NOT _qt_install_result EQUAL 0)
        message(FATAL_ERROR "Forge: Qt6 install failed.")
    endif()

    message(STATUS "Forge: Qt6 built and installed to ${INSTALL_DIR}")
endfunction()

# ---- Resolve Qt6 ----

hive_qt_find_prebuilt("${_hive_qt_package_dir}" _hive_qt_found)

if(NOT _hive_qt_found AND HIVE_BOOTSTRAP_QT)
    hive_qt_bootstrap(
        "${_hive_qt_source_dir}"
        "${_hive_qt_build_dir}"
        "${_hive_qt_package_dir}"
    )
    hive_qt_find_prebuilt("${_hive_qt_package_dir}" _hive_qt_found)
endif()

if(_hive_qt_found)
    message(STATUS "Forge: using Qt6 from ${_hive_qt_package_dir}")
    set(HIVE_QT_STATUS "Qt6 @ ${_hive_qt_package_dir}")
    list(PREPEND CMAKE_PREFIX_PATH "${_hive_qt_package_dir}")
elseif(HIVE_BOOTSTRAP_QT)
    message(FATAL_ERROR
        "Forge: Qt6 bootstrap completed but find_package(Qt6) still failed.\n"
        "Check ${_hive_qt_build_dir}/hive_qt_*.log for details."
    )
else()
    message(FATAL_ERROR
        "Forge: Qt6 not found in ${_hive_qt_package_dir}.\n"
        "Set HIVE_QT_PACKAGE_DIR or enable HIVE_BOOTSTRAP_QT.\n"
        "Submodule: git submodule update --init Forge/extern/qtbase"
    )
endif()
