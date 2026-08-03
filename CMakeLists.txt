cmake_minimum_required(VERSION 3.24)

project(
    teya_core
    VERSION 0.1.0
    LANGUAGES CXX
)

option(TEYA_CORE_BUILD_TESTS "Build teya-core tests" ${PROJECT_IS_TOP_LEVEL})

set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "Build raylib games" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "Build nlohmann/json tests" FORCE)
if(NOT TARGET raylib)
    add_subdirectory(external/raylib EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_subdirectory(external/json EXCLUDE_FROM_ALL)
endif()

if(TARGET raylib AND TARGET nlohmann_json::nlohmann_json)
    add_library(
        teya_core
        STATIC
            src/Input.cpp
            src/AssetPath.cpp
            src/JsonFile.cpp
            src/Log.cpp
            include/teya/core/Input.h
            include/teya/core/AssetPath.h
            include/teya/core/JsonFile.h
            include/teya/core/Log.h
    )

    target_include_directories(
        teya_core
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_compile_features(
        teya_core
        PUBLIC
            cxx_std_17
    )

    set_target_properties(
        teya_core
        PROPERTIES
            CXX_EXTENSIONS OFF
    )

    target_link_libraries(
        teya_core
        PUBLIC
            raylib
            nlohmann_json::nlohmann_json
    )

    add_library(
        Teya::Core
        ALIAS
            teya_core
    )
elseif(NOT TEYA_CORE_BUILD_TESTS)
    message(FATAL_ERROR
        "Standalone teya-core requires raylib and nlohmann_json targets. "
        "Initialize its submodules, or enable TEYA_CORE_BUILD_TESTS "
        "to build only the dependency-free logging test."
    )
else()
    message(STATUS
        "Building the standalone logging test without raylib or nlohmann/json"
    )
endif()

if(TEYA_CORE_BUILD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)
    add_executable(teya_core_log_test tests/LogTest.cpp)
    target_link_libraries(teya_core_log_test PRIVATE Teya::Core Threads::Threads)
    add_test(NAME teya_core_log_test COMMAND teya_core_log_test)
endif()
