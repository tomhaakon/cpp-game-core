cmake_minimum_required(VERSION 3.24)

project(
    cpp_game_core
    VERSION 0.1.0
    LANGUAGES CXX
)

option(CPP_GAME_CORE_BUILD_LOG_TEST "Build the logging development test" OFF)

if(TARGET raylib AND TARGET nlohmann_json::nlohmann_json)
    add_library(
        cpp_game_core
        STATIC
            src/Input.cpp
            src/AssetPath.cpp
            src/JsonFile.cpp
            src/Log.cpp
            include/game_core/Input.h
            include/game_core/AssetPath.h
            include/game_core/JsonFile.h
            include/game_core/Log.h
    )

    target_include_directories(
        cpp_game_core
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_compile_features(
        cpp_game_core
        PUBLIC
            cxx_std_17
    )

    set_target_properties(
        cpp_game_core
        PROPERTIES
            CXX_EXTENSIONS OFF
    )

    target_link_libraries(
        cpp_game_core
        PUBLIC
            raylib
            nlohmann_json::nlohmann_json
    )

    add_library(
        cpp_game_core::cpp_game_core
        ALIAS
            cpp_game_core
    )
elseif(NOT CPP_GAME_CORE_BUILD_LOG_TEST)
    message(FATAL_ERROR
        "Standalone cpp_game_core requires raylib and nlohmann_json targets. "
        "Add this project from its parent game, or enable CPP_GAME_CORE_BUILD_LOG_TEST "
        "to build only the dependency-free logging test."
    )
else()
    message(STATUS
        "Building the standalone logging test without raylib or nlohmann/json"
    )
endif()

if(CPP_GAME_CORE_BUILD_LOG_TEST)
    enable_testing()
    find_package(Threads REQUIRED)
    add_executable(cpp_game_core_log_test tests/LogTest.cpp src/Log.cpp)
    target_include_directories(cpp_game_core_log_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_features(cpp_game_core_log_test PRIVATE cxx_std_17)
    target_link_libraries(cpp_game_core_log_test PRIVATE Threads::Threads)
    add_test(NAME cpp_game_core_log_test COMMAND cpp_game_core_log_test)
endif()
